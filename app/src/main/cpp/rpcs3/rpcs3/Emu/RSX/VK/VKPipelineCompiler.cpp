#include "stdafx.h"
#include "VKPipelineCompiler.h"
#include "VKRenderPass.h"
#include "vkutils/device.h"
#include "Utilities/Thread.h"
#include "Utilities/File.h"

#include "Emu/cache_utils.hpp"
#include "util/sysinfo.hpp"

namespace vk
{
	// Global list of worker threads
	std::unique_ptr<named_thread_group<pipe_compiler>> g_pipe_compilers;
	int g_num_pipe_compilers = 0;
	atomic_t<int> g_compiler_index{};

	// Driver-level pipeline cache (SPIR-V -> native ISA), persisted to disk so the driver
	// reuses its compilation across launches instead of recompiling cold every boot.
	// Pipeline caches are internally synchronized for vkCreate*Pipelines, so worker threads
	// share this handle without an extra lock. Defined here; the create calls live in
	// VKProgramPipeline.cpp and reference it via an extern declaration.
	VkPipelineCache g_pipeline_cache = VK_NULL_HANDLE;

	static std::string get_pipeline_cache_path()
	{
		const std::string root = rpcs3::cache::get_ppu_cache();
		return root.empty() ? std::string{} : root + "vk_pipeline_cache.bin";
	}

	static bool pipeline_cache_header_ok(const std::vector<u8>& data)
	{
		if (data.size() < 32)
		{
			return false;
		}
		u32 length = 0, version = 0;
		std::memcpy(&length, data.data() + 0, sizeof(u32));
		std::memcpy(&version, data.data() + 4, sizeof(u32));
		return length >= 32 && length <= data.size() && version == VK_PIPELINE_CACHE_HEADER_VERSION_ONE;
	}

	void load_pipeline_cache(const vk::render_device& dev)
	{
		std::vector<u8> data;
		if (const std::string path = get_pipeline_cache_path(); !path.empty())
		{
			if (fs::file f{ path }; f)
			{
				data = f.to_vector<u8>();
				if (!data.empty() && !pipeline_cache_header_ok(data))
				{
					rsx_log.warning("Discarding malformed Vulkan pipeline cache (%zu bytes)", data.size());
					data.clear();
				}
			}
		}

		VkPipelineCacheCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		info.initialDataSize = data.size();
		info.pInitialData = data.empty() ? nullptr : data.data();

		if (_vkCreatePipelineCache(dev, &info, nullptr, &g_pipeline_cache) != VK_SUCCESS)
		{
			g_pipeline_cache = VK_NULL_HANDLE;
			rsx_log.error("Failed to create Vulkan pipeline cache; pipelines will be compiled without caching");
			return;
		}
		if (!data.empty())
		{
			rsx_log.notice("Loaded Vulkan pipeline cache (%zu bytes)", data.size());
		}
	}

	// Write the current cache to disk without destroying it. Safe to call mid-session
	// (e.g. right after the startup compile burst) so the cache survives a later crash.
	void flush_pipeline_cache(const vk::render_device& dev)
	{
		if (g_pipeline_cache == VK_NULL_HANDLE)
		{
			return;
		}

		const std::string path = get_pipeline_cache_path();
		if (path.empty())
		{
			return;
		}

		usz size = 0;
		if (_vkGetPipelineCacheData(dev, g_pipeline_cache, &size, nullptr) != VK_SUCCESS || !size)
		{
			return;
		}

		std::vector<u8> data(size);
		if (_vkGetPipelineCacheData(dev, g_pipeline_cache, &size, data.data()) != VK_SUCCESS)
		{
			return;
		}
		data.resize(size);

		if (fs::pending_file file{ path }; file.file)
		{
			file.file.write(data.data(), data.size());
			file.commit();
			rsx_log.notice("Saved Vulkan pipeline cache (%zu bytes)", data.size());
		}
	}

	void save_pipeline_cache(const vk::render_device& dev)
	{
		if (g_pipeline_cache == VK_NULL_HANDLE)
		{
			return;
		}

		flush_pipeline_cache(dev);

		_vkDestroyPipelineCache(dev, g_pipeline_cache, nullptr);
		g_pipeline_cache = VK_NULL_HANDLE;
	}

	pipe_compiler::pipe_compiler()
	{
		// TODO: Initialize workqueue
	}

	pipe_compiler::~pipe_compiler()
	{
		// TODO: Destroy and do cleanup
	}

	void pipe_compiler::initialize(const vk::render_device* pdev)
	{
		m_device = pdev;
	}

	void pipe_compiler::operator()()
	{
		while (thread_ctrl::state() != thread_state::aborting)
		{
			for (auto&& job : m_work_queue.pop_all())
			{
				if (job.is_graphics_job)
				{
					auto compiled = int_compile_graphics_pipe(job.graphics_data, job.graphics_modules, job.inputs, {}, job.flags);
					job.callback_func(compiled);
				}
				else
				{
					auto compiled = int_compile_compute_pipe(job.compute_data, job.inputs, job.flags);
					job.callback_func(compiled);
				}
			}

			thread_ctrl::wait_on(m_work_queue);
		}
	}

	std::unique_ptr<glsl::program> pipe_compiler::int_compile_compute_pipe(
		const VkComputePipelineCreateInfo& create_info,
		const std::vector<glsl::program_input>& cs_inputs,
		op_flags flags)
	{
		auto program = std::make_unique<glsl::program>(*m_device, create_info, cs_inputs);
		program->link(flags & SEPARATE_SHADER_OBJECTS);
		return program;
	}

	std::unique_ptr<glsl::program> pipe_compiler::int_compile_graphics_pipe(
		const VkGraphicsPipelineCreateInfo& create_info,
		const std::vector<glsl::program_input>& vs_inputs,
		const std::vector<glsl::program_input>& fs_inputs,
		op_flags flags)
	{
		auto program = std::make_unique<glsl::program>(*m_device, create_info, vs_inputs, fs_inputs);
		program->link(flags & SEPARATE_SHADER_OBJECTS);
		return program;
	}

	std::unique_ptr<glsl::program> pipe_compiler::int_compile_graphics_pipe(
		const vk::pipeline_props &create_info,
		VkShaderModule modules[2],
		const std::vector<glsl::program_input>& vs_inputs,
		const std::vector<glsl::program_input>& fs_inputs,
		op_flags flags)
	{
		VkPipelineShaderStageCreateInfo shader_stages[2] = {};
		shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shader_stages[0].module = modules[0];
		shader_stages[0].pName = "main";

		shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shader_stages[1].module = modules[1];
		shader_stages[1].pName = "main";

		std::vector<VkDynamicState> dynamic_state_descriptors;
		dynamic_state_descriptors.push_back(VK_DYNAMIC_STATE_VIEWPORT);
		dynamic_state_descriptors.push_back(VK_DYNAMIC_STATE_SCISSOR);
		dynamic_state_descriptors.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
		dynamic_state_descriptors.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
		dynamic_state_descriptors.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
		dynamic_state_descriptors.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
		dynamic_state_descriptors.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
		dynamic_state_descriptors.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);

		auto pdss = &create_info.state.ds;
		VkPipelineDepthStencilStateCreateInfo ds2;
		if (g_render_device->get_depth_bounds_support()) [[likely]]
		{
			dynamic_state_descriptors.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
		}
		else if (pdss->depthBoundsTestEnable)
		{
			rsx_log.warning("Depth bounds test is enabled in the pipeline object but not supported by the current driver.");

			ds2 = *pdss;
			pdss = &ds2;
			ds2.depthBoundsTestEnable = VK_FALSE;
		}

		VkPipelineDynamicStateCreateInfo dynamic_state_info = {};
		dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state_info.pDynamicStates = dynamic_state_descriptors.data();
		dynamic_state_info.dynamicStateCount = ::size32(dynamic_state_descriptors);

		VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

		VkPipelineViewportStateCreateInfo vp = {};
		vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		vp.viewportCount = 1;
		vp.scissorCount = 1;

		auto pmss = &create_info.state.ms;
		VkPipelineMultisampleStateCreateInfo ms2;
		ensure(pmss->rasterizationSamples == VkSampleCountFlagBits((create_info.renderpass_key >> 16) & 0xF)); // "Multisample state mismatch!"

		if (pmss->rasterizationSamples != VK_SAMPLE_COUNT_1_BIT || pmss->sampleShadingEnable) [[unlikely]]
		{
			ms2 = *pmss;
			pmss = &ms2;

			if (ms2.rasterizationSamples != VK_SAMPLE_COUNT_1_BIT)
			{
				// Update the sample mask pointer
				ms2.pSampleMask = &create_info.state.temp_storage.msaa_sample_mask;
			}

			if (g_cfg.video.antialiasing_level == msaa_level::none && ms2.sampleShadingEnable)
			{
				// Do not compile with MSAA enabled if multisampling is disabled
				rsx_log.warning("MSAA is disabled globally but a shader with multi-sampling enabled was submitted for compilation.");
				ms2.sampleShadingEnable = VK_FALSE;
			}
		}

		// Rebase pointers from pipeline structure in case it is moved/copied
		VkPipelineColorBlendStateCreateInfo cs = create_info.state.cs;
		cs.pAttachments = create_info.state.att_state;

		VkGraphicsPipelineCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		info.pVertexInputState = &vi;
		info.pInputAssemblyState = &create_info.state.ia;
		info.pRasterizationState = &create_info.state.rs;
		info.pColorBlendState = &cs;
		info.pMultisampleState = pmss;
		info.pViewportState = &vp;
		info.pDepthStencilState = pdss;
		info.stageCount = 2;
		info.pStages = shader_stages;
		info.pDynamicState = &dynamic_state_info;
		info.layout = VK_NULL_HANDLE;
		info.basePipelineIndex = -1;
		info.basePipelineHandle = VK_NULL_HANDLE;
		info.renderPass = vk::get_renderpass(*m_device, create_info.renderpass_key);

		return int_compile_graphics_pipe(info, vs_inputs, fs_inputs, flags);
	}

	std::unique_ptr<glsl::program> pipe_compiler::compile(
		const VkComputePipelineCreateInfo& create_info,
		op_flags flags, callback_t callback,
		const std::vector<glsl::program_input>& cs_inputs)
	{
		if (flags & COMPILE_INLINE)
		{
			return int_compile_compute_pipe(create_info, cs_inputs, flags);
		}

		m_work_queue.push(create_info, cs_inputs, flags, callback);
		return {};
	}

	std::unique_ptr<glsl::program> pipe_compiler::compile(
		const VkGraphicsPipelineCreateInfo& create_info,
		op_flags flags, callback_t /*callback*/,
		const std::vector<glsl::program_input>& vs_inputs,
		const std::vector<glsl::program_input>& fs_inputs)
	{
		// It is very inefficient to defer this as all pointers need to be saved
		ensure(flags & COMPILE_INLINE);
		return int_compile_graphics_pipe(create_info, vs_inputs, fs_inputs, flags);
	}

	std::unique_ptr<glsl::program> pipe_compiler::compile(
		const vk::pipeline_props &create_info,
		VkShaderModule vs,
		VkShaderModule fs,
		op_flags flags, callback_t callback,
		const std::vector<glsl::program_input>& vs_inputs,
		const std::vector<glsl::program_input>& fs_inputs)
	{
		VkShaderModule modules[] = { vs, fs };
		if (flags & COMPILE_INLINE)
		{
			return int_compile_graphics_pipe(create_info, modules, vs_inputs, fs_inputs, flags);
		}

		m_work_queue.push(create_info, modules, vs_inputs, fs_inputs, flags, callback);
		return {};
	}

	void initialize_pipe_compiler(int num_worker_threads)
	{
		if (num_worker_threads == 0)
		{
			// Select a conservative but modern default for async pipeline compilation.
			// Older heuristics topped out too early on high-core CPUs and left large
			// shader bursts queued longer than necessary.
			const auto hw_threads = utils::get_thread_count();

			if (hw_threads >= 24)
			{
				num_worker_threads = 12;
			}
			else if (hw_threads >= 16)
			{
				num_worker_threads = 8;
			}
			else if (hw_threads > 12)
			{
				num_worker_threads = 6;
			}
			else if (hw_threads > 8)
			{
				num_worker_threads = 4;
			}
			else if (hw_threads == 8)
			{
				num_worker_threads = 2;
			}
			else
			{
				num_worker_threads = 1;
			}

			rsx_log.notice("Async pipeline compiler auto-selected %d worker(s) for %u host thread(s).",
				num_worker_threads, hw_threads);
		}

		ensure(num_worker_threads >= 1);
		ensure(g_render_device); // "Cannot initialize pipe compiler before creating a logical device"

		// Bring up the on-disk driver pipeline cache before any pipeline is built
		load_pipeline_cache(*g_render_device);

		// Create the thread pool
		g_pipe_compilers = std::make_unique<named_thread_group<pipe_compiler>>("RSX.W", num_worker_threads);
		g_num_pipe_compilers = num_worker_threads;

		// Initialize the workers. At least one inline compiler shall exist (doesn't actually run)
		for (pipe_compiler& compiler : *g_pipe_compilers.get())
		{
			compiler.initialize(g_render_device);
		}
	}

	void destroy_pipe_compiler()
	{
		g_pipe_compilers.reset();

		// Flush the accumulated driver pipeline cache to disk
		if (g_pipeline_cache != VK_NULL_HANDLE && g_render_device)
		{
			save_pipeline_cache(*g_render_device);
		}
	}

	pipe_compiler* get_pipe_compiler()
	{
		ensure(g_pipe_compilers);
		int thread_index = g_compiler_index++;

		return g_pipe_compilers.get()->begin() + (thread_index % g_num_pipe_compilers);
	}
}

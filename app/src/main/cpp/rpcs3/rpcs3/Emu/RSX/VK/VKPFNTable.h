// Wrangler for Vulkan functions.
// TODO: Eventually, we shall declare vulkan with NO_PROTOTYPES and wrap everything here for android multi-driver support.
// For now, we just use it for extensions since we're on VK_1_0

#ifndef DBG_ADRENO_GPU_MEM
#if defined(DECL_VK_FUNCTION)
#define VK_FUNC(func) extern PFN_##func _##func
#elif defined(DEF_VK_FUNCTION)
#define VK_FUNC(func) PFN_##func _##func
#elif defined(LOAD_VK_FUNCTION)

#if __ANDROID__
#define VK_FUNC(func) _##func=reinterpret_cast<PFN_##func>(dlsym(vk_lib_handle, #func))
#elif _WIN32
#define VK_FUNC(func) _##func=reinterpret_cast<PFN_##func>(##func)
#else
#error ""
#endif

#else
#error ""
#endif
#else

#if defined(DECL_VK_FUNCTION)
#define VK_FUNC(func) extern int64_t n_##func;\
template<typename... Args> \
auto _##func(Args&&... args)->decltype(func(args...)){ \
     n_##func=  static_cast<int64_t>(meminfo_gpu_mem_usage_kb());                                       \
   if constexpr(std::is_same_v<decltype(func(args...)),void>){ \
      func(std::forward<Args>(args)...);                                        \
    save_msg(std::format("/storage/emulated/0/Android/data/aenu.aps3e/files/aps3e/logs/{}.txt",#func),#func,static_cast<int64_t>(meminfo_gpu_mem_usage_kb())-n_##func)   ; \
    }else{                                        \
         auto r= func(std::forward<Args>(args)...);    \
        save_msg(std::format("/storage/emulated/0/Android/data/aenu.aps3e/files/aps3e/logs/{}.txt",#func),#func,static_cast<int64_t>(meminfo_gpu_mem_usage_kb())-n_##func)   ; \
        return r;                               \
}}
#elif defined(DEF_VK_FUNCTION)
#define VK_FUNC(func) int64_t n_##func=0

#elif defined(LOAD_VK_FUNCTION)
#define VK_FUNC(func)

#elif defined(CLEAR_N_VK_FUNCTION)
#define VK_FUNC(func) n_##func=0

#elif defined(PRINT_N_VK_FUNCTION)
#define VK_FUNC(func) if(n_##func) rsx_log.warning("#### %s: %d",#func,n_##func)
#else
#error ""

#endif
#endif
VK_FUNC(vkCreateInstance);
VK_FUNC(vkDestroyInstance);
VK_FUNC(vkEnumeratePhysicalDevices);
VK_FUNC(vkGetPhysicalDeviceFeatures);
//VK_FUNC(vkGetPhysicalDeviceFeatures2KHR); // 注意：虽然属于KHR扩展，但属于1.0功能集
VK_FUNC(vkGetPhysicalDeviceProperties);
//VK_FUNC(vkGetPhysicalDeviceProperties2KHR);
VK_FUNC(vkGetPhysicalDeviceQueueFamilyProperties);
VK_FUNC(vkGetPhysicalDeviceMemoryProperties);
VK_FUNC(vkGetInstanceProcAddr);
VK_FUNC(vkGetDeviceProcAddr);
VK_FUNC(vkCreateDevice);
VK_FUNC(vkDestroyDevice);
VK_FUNC(vkEnumerateInstanceExtensionProperties);
VK_FUNC(vkEnumerateDeviceExtensionProperties);
VK_FUNC(vkEnumerateInstanceLayerProperties);
VK_FUNC(vkEnumerateDeviceLayerProperties);
VK_FUNC(vkGetDeviceQueue);
VK_FUNC(vkQueueSubmit);
VK_FUNC(vkQueueWaitIdle);
VK_FUNC(vkDeviceWaitIdle);
VK_FUNC(vkAllocateMemory);
VK_FUNC(vkFreeMemory);
VK_FUNC(vkMapMemory);
VK_FUNC(vkUnmapMemory);
VK_FUNC(vkFlushMappedMemoryRanges);
VK_FUNC(vkInvalidateMappedMemoryRanges);
VK_FUNC(vkBindBufferMemory);
VK_FUNC(vkBindImageMemory);
VK_FUNC(vkGetBufferMemoryRequirements);
VK_FUNC(vkGetImageMemoryRequirements);
VK_FUNC(vkGetImageSparseMemoryRequirements);
VK_FUNC(vkQueueBindSparse);
VK_FUNC(vkCreateFence);
VK_FUNC(vkDestroyFence);
VK_FUNC(vkResetFences);
VK_FUNC(vkGetFenceStatus);
VK_FUNC(vkWaitForFences);
VK_FUNC(vkCreateSemaphore);
VK_FUNC(vkDestroySemaphore);
VK_FUNC(vkCreateEvent);
VK_FUNC(vkDestroyEvent);
VK_FUNC(vkGetEventStatus);
VK_FUNC(vkSetEvent);
VK_FUNC(vkResetEvent);
VK_FUNC(vkCreateQueryPool);
VK_FUNC(vkDestroyQueryPool);
VK_FUNC(vkGetQueryPoolResults);
VK_FUNC(vkCreateBuffer);
VK_FUNC(vkDestroyBuffer);
VK_FUNC(vkCreateBufferView);
VK_FUNC(vkDestroyBufferView);
VK_FUNC(vkCreateImage);
VK_FUNC(vkDestroyImage);
VK_FUNC(vkGetImageSubresourceLayout);
VK_FUNC(vkCreateImageView);
VK_FUNC(vkDestroyImageView);
VK_FUNC(vkCreateShaderModule);
VK_FUNC(vkDestroyShaderModule);
VK_FUNC(vkCreateRenderPass);
VK_FUNC(vkDestroyRenderPass);
VK_FUNC(vkCreatePipelineLayout);
VK_FUNC(vkDestroyPipelineLayout);
VK_FUNC(vkCreateGraphicsPipelines);
VK_FUNC(vkCreateComputePipelines);
VK_FUNC(vkDestroyPipeline);
VK_FUNC(vkCreatePipelineCache);
VK_FUNC(vkGetPipelineCacheData);
VK_FUNC(vkDestroyPipelineCache);
VK_FUNC(vkCreateSampler);
VK_FUNC(vkDestroySampler);
VK_FUNC(vkCreateDescriptorSetLayout);
VK_FUNC(vkDestroyDescriptorSetLayout);
VK_FUNC(vkCreateDescriptorPool);
VK_FUNC(vkDestroyDescriptorPool);
VK_FUNC(vkResetDescriptorPool);
VK_FUNC(vkAllocateDescriptorSets);
VK_FUNC(vkFreeDescriptorSets);
VK_FUNC(vkUpdateDescriptorSets);
VK_FUNC(vkCreateFramebuffer);
VK_FUNC(vkDestroyFramebuffer);
VK_FUNC(vkCreateCommandPool);
VK_FUNC(vkDestroyCommandPool);
VK_FUNC(vkResetCommandPool);
VK_FUNC(vkAllocateCommandBuffers);
VK_FUNC(vkFreeCommandBuffers);
VK_FUNC(vkBeginCommandBuffer);
VK_FUNC(vkEndCommandBuffer);
VK_FUNC(vkResetCommandBuffer);
VK_FUNC(vkCmdBindPipeline);
VK_FUNC(vkCmdSetViewport);
VK_FUNC(vkCmdSetScissor);
VK_FUNC(vkCmdSetLineWidth);
VK_FUNC(vkCmdSetDepthBias);
VK_FUNC(vkCmdSetBlendConstants);
VK_FUNC(vkCmdSetDepthBounds);
VK_FUNC(vkCmdSetStencilCompareMask);
VK_FUNC(vkCmdSetStencilWriteMask);
VK_FUNC(vkCmdSetStencilReference);
VK_FUNC(vkCmdBindDescriptorSets);
VK_FUNC(vkCmdBindIndexBuffer);
VK_FUNC(vkCmdBindVertexBuffers);
VK_FUNC(vkCmdDraw);
VK_FUNC(vkCmdDrawIndexed);
VK_FUNC(vkCmdDrawIndirect);
VK_FUNC(vkCmdDrawIndexedIndirect);
VK_FUNC(vkCmdDispatch);
VK_FUNC(vkCmdDispatchIndirect);
VK_FUNC(vkCmdCopyBuffer);
VK_FUNC(vkCmdCopyImage);
VK_FUNC(vkCmdBlitImage);
VK_FUNC(vkCmdCopyBufferToImage);
VK_FUNC(vkCmdCopyImageToBuffer);
VK_FUNC(vkCmdUpdateBuffer);
VK_FUNC(vkCmdFillBuffer);
VK_FUNC(vkCmdClearColorImage);
VK_FUNC(vkCmdClearDepthStencilImage);
VK_FUNC(vkCmdClearAttachments);
VK_FUNC(vkCmdResolveImage);
VK_FUNC(vkCmdSetEvent);
VK_FUNC(vkCmdResetEvent);
VK_FUNC(vkCmdWaitEvents);
VK_FUNC(vkCmdPipelineBarrier);
VK_FUNC(vkCmdBeginRenderPass);
VK_FUNC(vkCmdNextSubpass);
VK_FUNC(vkCmdEndRenderPass);
VK_FUNC(vkCmdExecuteCommands);

VK_FUNC(vkCmdPushConstants);
#if __ANDROID__
VK_FUNC(vkCreateAndroidSurfaceKHR);
#elif _WIN32
VK_FUNC(vkCreateWin32SurfaceKHR);
#endif
VK_FUNC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
VK_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR);


VK_FUNC(vkDestroySurfaceKHR);
VK_FUNC(vkGetPhysicalDeviceSurfaceSupportKHR);


VK_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR);
VK_FUNC(vkGetPhysicalDeviceFormatProperties);


VK_FUNC(vkCmdResetQueryPool);
VK_FUNC(vkCmdBeginQuery);
VK_FUNC(vkCmdEndQuery);
VK_FUNC(vkCmdCopyQueryPoolResults);

VK_FUNC(vkCreateSwapchainKHR);
VK_FUNC(vkDestroySwapchainKHR);

VK_FUNC(vkGetSwapchainImagesKHR);

VK_FUNC(vkAcquireNextImageKHR);

VK_FUNC(vkQueuePresentKHR);

//VK 1.1
VK_FUNC(vkGetPhysicalDeviceProperties2);
VK_FUNC(vkGetPhysicalDeviceFeatures2);

#ifndef DBG_ADRENO_GPU_MEM
#if defined(DECL_VK_FUNCTION)||defined(DEF_VK_FUNCTION)||defined(CLEAR_N_VK_FUNCTION)||defined(PRINT_N_VK_FUNCTION)
#define INSTANCE_VK_FUNCTION
#define DEVICE_VK_FUNCTION
#include "VKPFNTableEXT.h"
#undef INSTANCE_VK_FUNCTION
#undef DEVICE_VK_FUNCTION
#endif
#endif

#undef VK_FUNC

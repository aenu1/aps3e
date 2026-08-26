// SPU ASMJIT Recompiler Test Program
// Standalone CLI test for comparing JIT recompiler output against interpreter reference
// Build: cmake --build . --target spu_asmjit_test
// Run: ./spu_asmjit_test

#include "stdafx.h"
#include "Emu/Cell/SPUThread.h"
#include "Emu/Cell/SPURecompiler.h"
#include "Emu/Cell/SPUASMJITRecompiler.h"
#include "Emu/Cell/SPUInterpreter.h"
#include "Emu/Cell/SPUOpcodes.h"
#include "Emu/system_config.h"
#include "Utilities/JIT.h"
#include "util/v128.hpp"
#include "util/logs.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <random>

LOG_CHANNEL(test_log, "TEST");

// ============================================================================
// Stub implementations for missing Input module symbols
// These are needed because rpcs3_emu references pad_thread from Cell modules
// ============================================================================

#include "Input/pad_thread.h"
#include "Input/product_info.h"
#include "Input/ps_move_config.h"
#include "Input/ps_move_tracker.h"
#include "Emu/Io/pad_config.h"
#include "rpcs3_version.h"
#include "Utilities/version.h"

namespace pad
{
	atomic_t<pad_thread*> g_pad_thread{};
	shared_mutex g_pad_mutex;
}

void pad_thread::SetIntercepted(bool)
{
}

void pad_thread::SetRumble(u32, u8, u8)
{
}

int pad_thread::AddLddPad()
{
	return 0;
}

void pad_thread::UnregisterLddPad(u32)
{
}

namespace input
{
	std::vector<product_info> get_products_by_class(int)
	{
		return {};
	}
}

bool cfg_ps_moves::load()
{
	return false;
}

cfg_ps_moves g_cfg_move;

cfg_ps_moves::cfg_ps_moves() {}

// ps_move_tracker stubs
template <bool SaveToFile>
ps_move_tracker<SaveToFile>::ps_move_tracker()
{
}

template <bool SaveToFile>
ps_move_tracker<SaveToFile>::~ps_move_tracker()
{
}

template <bool SaveToFile>
void ps_move_tracker<SaveToFile>::set_image_data(const void*, u64, u32, u32, int)
{
}

template <bool SaveToFile>
void ps_move_tracker<SaveToFile>::process_image()
{
}

template <bool SaveToFile>
void ps_move_tracker<SaveToFile>::set_active(u32, bool)
{
}

template <bool SaveToFile>
void ps_move_tracker<SaveToFile>::set_hue(u32, u16)
{
}

template <bool SaveToFile>
void ps_move_tracker<SaveToFile>::set_hue_threshold(u32, u16)
{
}

template <bool SaveToFile>
void ps_move_tracker<SaveToFile>::set_saturation_threshold(u32, u16)
{
}

template <bool SaveToFile>
std::tuple<u8, u8, u8> ps_move_tracker<SaveToFile>::hsv_to_rgb(u16, float, float)
{
	return {0, 0, 0};
}

template <bool SaveToFile>
std::tuple<s16, float, float> ps_move_tracker<SaveToFile>::rgb_to_hsv(float, float, float)
{
	return {0, 0.0f, 0.0f};
}

template class ps_move_tracker<false>;
template class ps_move_tracker<true>;

// Additional stubs for rpcs3 version and system utilities
namespace rpcs3
{
	bool is_local_build() { return false; }
	static utils::version s_version{0, 0, 0, utils::version_type::release, 1, ""};
	const utils::version& get_version() { return s_version; }
	std::string get_verbose_version() { return "test"; }
	std::string get_version_and_branch() { return "test"; }
}

void report_fatal_error(std::string_view, bool, bool) {}

void qt_events_aware_op(int, std::function<bool()>) {}

struct cfg_input_configurations g_cfg_input_configs;

cfg_input_configurations::cfg_input_configurations() {}

#ifdef __ANDROID__
// Android-specific stubs: real implementations live in aps3e_rp3_impl.cpp,
// which is not part of the spu_asmjit_test target.
#include "Emu/RSX/Overlays/overlay_fonts.h"

std::string rp3_get_config_dir() { return "./"; }
std::string rp3_get_cache_dir() { return "./"; }

const std::unordered_map<rsx::overlays::language_class, std::vector<std::string>>& cfg_font_files()
{
	static std::unordered_map<rsx::overlays::language_class, std::vector<std::string>> r;
	return r;
}
#endif

// ============================================================================
// SPU Instruction Encoding Helpers
// ============================================================================
// IMPORTANT: rpcs3 stores SPU instructions byte-swapped ("intentionally wrong
// endianness"). spu_opcode_t field positions (see SPUOpcodes.h):
//   opcode = inst >> 21 (bits 31:21)
//   rt     = bits 6:0
//   ra     = bits 13:7
//   rb     = bits 20:14
//   rc/rt4 = bits 27:21
//   si10   = bits 17:8 (signed)
//   i16    = bits 22:7

// Encode a 3-register RRR instruction: rt = f(ra, rb)
static u32 enc_rr(u32 opcode, u32 rt, u32 ra, u32 rb)
{
	return (opcode << 21) | ((rb & 0x7F) << 14) | ((ra & 0x7F) << 7) | (rt & 0x7F);
}

// Encode si10-immediate instruction: rt = f(ra, si10).
// Decoder entries for these use magn=3 (value8 at bits 24:31), and the 10-bit
// si10 occupies bits 14:23 (its top 3 bits fold into the low opcode bits).
static u32 enc_ri10(u32 value8, u32 rt, u32 ra, s32 si10)
{
	const u32 u10 = static_cast<u32>(si10) & 0x3FF;
	return (value8 << 24) | (u10 << 14) | ((ra & 0x7F) << 7) | (rt & 0x7F);
}

// Encode immediate instruction with si16/i16: rt = f(imm16)
static u32 enc_ri16(u32 opcode, u32 rt, s32 si16)
{
	return (opcode << 21) | ((si16 & 0xFFFF) << 7) | (rt & 0x7F);
}

// Encode immediate instruction with i16 (unsigned)
static u32 enc_i16(u32 opcode, u32 rt, u32 i16)
{
	return (opcode << 21) | ((i16 & 0xFFFF) << 7) | (rt & 0x7F);
}

// Encode STOP instruction (opcode 0, all-zero word for code 0)
static u32 enc_stop(u32 code = 0)
{
	// STOP code is at canonical bits 17:4 -> swapped bits 27:14
	return (code & 0x3FFF) << 14;
}

// ============================================================================
// Test Infrastructure
// ============================================================================

struct test_input_t
{
	v128 ra;
	v128 rb;
	v128 rc;
};

struct test_result_t
{
	const char* name;
	int total;
	int passed;
	int failed;
};

// Minimal SPU thread mock - we allocate raw memory matching spu_thread layout
// The JIT and interpreter both access spu_thread at fixed offsets via offset32
// NOTE: spu_thread has ALIGN(2048), so we need 2048-byte alignment
struct alignas(2048) spu_test_context
{
	// Raw storage for spu_thread (we don't call constructor, just use memory layout)
	alignas(2048) u8 thread_storage[sizeof(spu_thread)];
	// LS memory (256KB)
	alignas(16) u8 ls_mem[SPU_LS_SIZE];

	spu_thread* get_thread() { return reinterpret_cast<spu_thread*>(thread_storage); }

	void reset()
	{
		memset(thread_storage, 0, sizeof(thread_storage));
		memset(ls_mem, 0, sizeof(ls_mem));

		// Set critical fields via offset32
		// pc = 0
		*reinterpret_cast<u32*>(thread_storage + offset32(&spu_thread::pc)) = 0;

		// state = 0 (so JIT doesn't immediately stop)
		// state is inherited from cpu_thread
		*reinterpret_cast<u64*>(thread_storage + offset32(&spu_thread::state)) = 0;

		// Set ls pointer for load/store instructions (interpreter path)
		// ls is a const member, we use memcpy to overwrite
		u8* ls_ptr = ls_mem;
		memcpy(thread_storage + offset32(&spu_thread::ls), &ls_ptr, sizeof(u8*));
	}

	v128* get_gpr(u32 reg)
	{
		return reinterpret_cast<v128*>(thread_storage + offset32(&spu_thread::gpr) + reg * sizeof(v128));
	}

	void set_gpr(u32 reg, const v128& val)
	{
		*get_gpr(reg) = val;
	}

	v128 get_gpr(u32 reg) const
	{
		return *reinterpret_cast<const v128*>(thread_storage + offset32(&spu_thread::gpr) + reg * sizeof(v128));
	}
};

static spu_test_context g_ctx;

// ============================================================================
// Expected Value Computation (SPU ISA semantics)
// ============================================================================

static v128 expected_A(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++) r._u32[i] = ra._u32[i] + rb._u32[i];
	return r;
}

static v128 expected_SF(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++) r._u32[i] = rb._u32[i] - ra._u32[i];
	return r;
}

static v128 expected_AND(const v128& ra, const v128& rb)
{
	v128 r;
	r._u64[0] = ra._u64[0] & rb._u64[0];
	r._u64[1] = ra._u64[1] & rb._u64[1];
	return r;
}

static v128 expected_OR(const v128& ra, const v128& rb)
{
	v128 r;
	r._u64[0] = ra._u64[0] | rb._u64[0];
	r._u64[1] = ra._u64[1] | rb._u64[1];
	return r;
}

static v128 expected_XOR(const v128& ra, const v128& rb)
{
	v128 r;
	r._u64[0] = ra._u64[0] ^ rb._u64[0];
	r._u64[1] = ra._u64[1] ^ rb._u64[1];
	return r;
}

static v128 expected_NOR(const v128& ra, const v128& rb)
{
	v128 r;
	r._u64[0] = ~(ra._u64[0] | rb._u64[0]);
	r._u64[1] = ~(ra._u64[1] | rb._u64[1]);
	return r;
}

static v128 expected_NAND(const v128& ra, const v128& rb)
{
	v128 r;
	r._u64[0] = ~(ra._u64[0] & rb._u64[0]);
	r._u64[1] = ~(ra._u64[1] & rb._u64[1]);
	return r;
}

static v128 expected_AI(const v128& ra, s32 si10)
{
	v128 r;
	for (int i = 0; i < 4; i++) r._u32[i] = ra._u32[i] + static_cast<u32>(si10);
	return r;
}

static v128 expected_SFI(const v128& ra, s32 si10)
{
	v128 r;
	for (int i = 0; i < 4; i++) r._u32[i] = static_cast<u32>(si10) - ra._u32[i];
	return r;
}

static v128 expected_AH(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 8; i++) r._u16[i] = ra._u16[i] + rb._u16[i];
	return r;
}

static v128 expected_AHI(const v128& ra, s32 si10)
{
	v128 r;
	for (int i = 0; i < 8; i++) r._u16[i] = ra._u16[i] + static_cast<u16>(si10);
	return r;
}

static v128 expected_CGT(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++) r._u32[i] = (static_cast<s32>(ra._u32[i]) > static_cast<s32>(rb._u32[i])) ? 0xFFFFFFFF : 0;
	return r;
}

static v128 expected_CLGT(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++) r._u32[i] = (ra._u32[i] > rb._u32[i]) ? 0xFFFFFFFF : 0;
	return r;
}

static v128 expected_CEQ(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++) r._u32[i] = (ra._u32[i] == rb._u32[i]) ? 0xFFFFFFFF : 0;
	return r;
}

static v128 expected_SHL(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
	{
		u32 sh = rb._u32[i] & 0x3F;
		r._u32[i] = (sh >= 32) ? 0 : (ra._u32[i] << sh);
	}
	return r;
}

static v128 expected_ROTM(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
	{
		u32 sh = (-(rb._u32[i] & 0x3F)) & 0x3F;
		r._u32[i] = (sh >= 32) ? 0 : (ra._u32[i] >> sh);
	}
	return r;
}

static v128 expected_ROTMA(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
	{
		u32 sh = (-(rb._u32[i] & 0x3F)) & 0x3F;
		if (sh >= 32) sh = 31;
		r._u32[i] = static_cast<u32>(static_cast<s32>(ra._u32[i]) >> sh);
	}
	return r;
}

static v128 expected_ROT(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
	{
		u32 sh = rb._u32[i] & 0x1F;
		if (sh == 0)
			r._u32[i] = ra._u32[i];
		else
			r._u32[i] = (ra._u32[i] << sh) | (ra._u32[i] >> (32 - sh));
	}
	return r;
}

static v128 expected_SELB(const v128& ra, const v128& rb, const v128& rc)
{
	v128 r;
	for (int i = 0; i < 16; i++)
		r._u8[i] = (rc._u8[i] & 1) ? rb._u8[i] : ra._u8[i];
	return r;
}

// --- Additional data-processing expected values ---

static v128 expected_SFH(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 8; i++) r._u16[i] = rb._u16[i] - ra._u16[i];
	return r;
}

static v128 expected_ANDC(const v128& ra, const v128& rb)
{
	v128 r;
	r._u64[0] = ra._u64[0] & ~rb._u64[0];
	r._u64[1] = ra._u64[1] & ~rb._u64[1];
	return r;
}

static v128 expected_ORC(const v128& ra, const v128& rb)
{
	v128 r;
	r._u64[0] = ra._u64[0] | ~rb._u64[0];
	r._u64[1] = ra._u64[1] | ~rb._u64[1];
	return r;
}

static v128 expected_EQV(const v128& ra, const v128& rb)
{
	v128 r;
	r._u64[0] = ~(ra._u64[0] ^ rb._u64[0]);
	r._u64[1] = ~(ra._u64[1] ^ rb._u64[1]);
	return r;
}

static v128 expected_CGTH(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 8; i++) r._u16[i] = (static_cast<s16>(ra._u16[i]) > static_cast<s16>(rb._u16[i])) ? 0xFFFF : 0;
	return r;
}

static v128 expected_CGTB(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 16; i++) r._u8[i] = (static_cast<s8>(ra._u8[i]) > static_cast<s8>(rb._u8[i])) ? 0xFF : 0;
	return r;
}

static v128 expected_CLGTH(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 8; i++) r._u16[i] = (ra._u16[i] > rb._u16[i]) ? 0xFFFF : 0;
	return r;
}

static v128 expected_CLGTB(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 16; i++) r._u8[i] = (ra._u8[i] > rb._u8[i]) ? 0xFF : 0;
	return r;
}

static v128 expected_CEQH(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 8; i++) r._u16[i] = (ra._u16[i] == rb._u16[i]) ? 0xFFFF : 0;
	return r;
}

static v128 expected_CEQB(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 16; i++) r._u8[i] = (ra._u8[i] == rb._u8[i]) ? 0xFF : 0;
	return r;
}

static v128 expected_ROTH(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 8; i++)
	{
		u32 sh = rb._u16[i] & 0xF;
		r._u16[i] = sh ? static_cast<u16>((ra._u16[i] << sh) | (ra._u16[i] >> (16 - sh))) : ra._u16[i];
	}
	return r;
}

static v128 expected_ROTHM(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 8; i++)
	{
		u32 sh = (-(rb._u16[i] & 0x1F)) & 0x1F;
		r._u16[i] = (sh >= 16) ? 0 : static_cast<u16>(ra._u16[i] >> sh);
	}
	return r;
}

static v128 expected_ROTMAH(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 8; i++)
	{
		u32 sh = (-(rb._u16[i] & 0x1F)) & 0x1F;
		if (sh >= 16) sh = 15;
		r._u16[i] = static_cast<u16>(static_cast<s16>(ra._u16[i]) >> sh);
	}
	return r;
}

static v128 expected_SHLH(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 8; i++)
	{
		u32 sh = rb._u16[i] & 0x1F;
		r._u16[i] = (sh >= 16) ? 0 : static_cast<u16>(ra._u16[i] << sh);
	}
	return r;
}

static v128 expected_CNTB(const v128& ra, const v128& rb)
{
	(void)rb;
	v128 r;
	for (int i = 0; i < 16; i++)
	{
		u8 v = ra._u8[i];
		u8 c = 0;
		while (v) { c += v & 1; v >>= 1; }
		r._u8[i] = c;
	}
	return r;
}

static v128 expected_CLZ(const v128& ra, const v128& rb)
{
	(void)rb;
	v128 r;
	for (int i = 0; i < 4; i++)
	{
		u32 v = ra._u32[i];
		u32 cnt = 0;
		if (v == 0) { r._u32[i] = 32; continue; }
		while (!(v & 0x80000000u)) { cnt++; v <<= 1; }
		r._u32[i] = cnt;
	}
	return r;
}

static v128 expected_XSBH(const v128& ra, const v128& rb)
{
	(void)rb;
	v128 r;
	for (int i = 0; i < 8; i++)
		r._u16[i] = static_cast<u16>(static_cast<s16>(static_cast<s8>(ra._u16[i] & 0xff)));
	return r;
}

static v128 expected_ORX(const v128& ra, const v128& rb)
{
	(void)rb;
	v128 r{};
	r._u32[3] = ra._u32[0] | ra._u32[1] | ra._u32[2] | ra._u32[3];
	return r;
}

static v128 expected_XSHW(const v128& ra, const v128& rb)
{
	(void)rb;
	v128 r;
	r._u32[0] = static_cast<u32>(static_cast<s32>(static_cast<s16>(ra._u16[0])));
	r._u32[1] = static_cast<u32>(static_cast<s32>(static_cast<s16>(ra._u16[2])));
	r._u32[2] = static_cast<u32>(static_cast<s32>(static_cast<s16>(ra._u16[4])));
	r._u32[3] = static_cast<u32>(static_cast<s32>(static_cast<s16>(ra._u16[6])));
	return r;
}

static v128 expected_XSWD(const v128& ra, const v128& rb)
{
	(void)rb;
	v128 r;
	r._u32[0] = static_cast<u32>(static_cast<s32>(ra._u32[0]));
	r._u32[1] = static_cast<u32>(static_cast<s32>(ra._u32[0]) >> 31);
	r._u32[2] = static_cast<u32>(static_cast<s32>(ra._u32[2]));
	r._u32[3] = static_cast<u32>(static_cast<s32>(ra._u32[2]) >> 31);
	return r;
}

static v128 expected_SUMB(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
	{
		u32 sum_a = ra._u8[i * 4] + ra._u8[i * 4 + 1] + ra._u8[i * 4 + 2] + ra._u8[i * 4 + 3];
		u32 sum_b = rb._u8[i * 4] + rb._u8[i * 4 + 1] + rb._u8[i * 4 + 2] + rb._u8[i * 4 + 3];
		r._u32[i] = sum_a + sum_b;
	}
	return r;
}

static v128 expected_ABSDB(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 16; i++)
		r._u8[i] = (ra._u8[i] > rb._u8[i]) ? (ra._u8[i] - rb._u8[i]) : (rb._u8[i] - ra._u8[i]);
	return r;
}

static v128 expected_AVGB(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 16; i++)
		r._u8[i] = static_cast<u8>((static_cast<u32>(ra._u8[i]) + rb._u8[i] + 1) >> 1);
	return r;
}

static v128 expected_MPY(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
		r._u32[i] = static_cast<u32>(static_cast<s32>(ra._u32[i]) * static_cast<s32>(rb._u32[i])) << 0;
	// MPY: multiply signed, result is bits [16:47] >> 16? Actually MPY gives (ra*rb)>>16 per word
	for (int i = 0; i < 4; i++)
		r._u32[i] = static_cast<u32>((static_cast<s64>(static_cast<s32>(ra._u32[i])) * static_cast<s32>(rb._u32[i])) >> 16);
	return r;
}

static v128 expected_MPYU(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
		r._u32[i] = static_cast<u32>((static_cast<u64>(ra._u32[i]) * rb._u32[i]) >> 16);
	return r;
}

static v128 expected_MPYH(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
		r._u32[i] = (ra._u32[i] * rb._u32[i]) << 16;
	return r;
}

static v128 expected_MPYHH(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
		r._u32[i] = static_cast<u32>(static_cast<s32>(static_cast<s16>(ra._u32[i] >> 16)) * static_cast<s16>(rb._u32[i] >> 16));
	return r;
}

static v128 expected_MPYS(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
		r._u32[i] = static_cast<u32>((static_cast<s64>(static_cast<s32>(ra._u32[i])) * static_cast<s32>(rb._u32[i])) >> 16);
	return r;
}

static v128 expected_MPYHHU(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
		r._u32[i] = static_cast<u32>(static_cast<u32>(ra._u32[i] >> 16) * static_cast<u32>(rb._u32[i] >> 16));
	return r;
}

static v128 expected_CG(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
		r._u32[i] = (static_cast<u64>(ra._u32[i]) + rb._u32[i]) >> 32 ? 1 : 0;
	return r;
}

static v128 expected_BG(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
		r._u32[i] = (rb._u32[i] < ra._u32[i]) ? 1 : 0; // borrow if rb < ra (rb - ra borrows)
	return r;
}

static v128 expected_ADDX(const v128& ra, const v128& rb)
{
	v128 r;
	for (int i = 0; i < 4; i++)
		r._u32[i] = ra._u32[i] + rb._u32[i];
	return r;
}

static v128 expected_GB(const v128& ra, const v128& rb)
{
	(void)rb;
	v128 r{};
	u32 mask = 0;
	for (int i = 0; i < 32; i++)
		if (ra._u32[i / 32] & (1u << (i % 32))) mask |= 1u << i;
	r._u32[3] = mask;
	return r;
}

static v128 expected_FSMB(const v128& ra, const v128& rb)
{
	(void)rb;
	v128 r;
	for (int i = 0; i < 16; i++)
		r._u8[i] = (ra._u8[15] & (1u << (i & 7))) ? 0xFF : 0;
	return r;
}

// --- Byte-rotate / byte-shift quadword (i7 immediate) ---
// SHLQBYI: shift the quadword LEFT by (i7 & 0x1f) bytes, zero-fill low bytes.
static v128 expected_SHLQBYI(const v128& ra, s32 i7)
{
	const u32 s = static_cast<u32>(i7) & 0x1f;
	v128 r{};
	for (int i = 0; i < 16; i++)
		r._u8[i] = (static_cast<u32>(i) >= s) ? ra._u8[i - s] : 0;
	return r;
}

// ROTQMBYI: shift the quadword RIGHT by ((0-i7) & 0x1f) bytes, zero-fill high bytes.
static v128 expected_ROTQMBYI(const v128& ra, s32 i7)
{
	const u32 s = (0u - static_cast<u32>(i7)) & 0x1f;
	v128 r{};
	for (int i = 0; i < 16; i++)
		r._u8[i] = (static_cast<u32>(i) + s < 16) ? ra._u8[i + s] : 0;
	return r;
}

// ROTQBYI: rotate the quadword by (i7 & 0xf) bytes (matches palignr(va,va,16-s)).
static v128 expected_ROTQBYI(const v128& ra, s32 i7)
{
	const u32 s = static_cast<u32>(i7) & 0xf;
	v128 r{};
	for (int i = 0; i < 16; i++)
		r._u8[i] = ra._u8[(i - s + 16) & 15];
	return r;
}

// CEQBI: byte-wise compare-equal with an 8-bit immediate.
static v128 expected_CEQBI(const v128& ra, s32 i8)
{
	const u8 imm = static_cast<u8>(i8);
	v128 r{};
	for (int i = 0; i < 16; i++)
		r._u8[i] = (ra._u8[i] == imm) ? 0xFF : 0;
	return r;
}

// ANDBI: byte-wise AND with an 8-bit immediate.
static v128 expected_ANDBI(const v128& ra, s32 i8)
{
	const u8 imm = static_cast<u8>(i8);
	v128 r{};
	for (int i = 0; i < 16; i++)
		r._u8[i] = ra._u8[i] & imm;
	return r;
}

// ILA: load an 18-bit immediate into every word (ra unused).
static v128 expected_ILA(const v128& ra, s32 i18)
{
	(void)ra;
	v128 r{};
	const u32 v = static_cast<u32>(i18) & 0x3ffff;
	for (int i = 0; i < 4; i++)
		r._u32[i] = v;
	return r;
}



// ============================================================================
// Input Generation
// ============================================================================

static std::mt19937 g_rng(42); // Fixed seed for reproducibility

static v128 gen_random_v128()
{
	v128 r;
	for (int i = 0; i < 4; i++)
		r._u32[i] = g_rng();
	return r;
}

static v128 gen_zero()
{
	v128 r{};
	return r;
}

static v128 gen_ones()
{
	v128 r;
	r._u64[0] = ~0ull;
	r._u64[1] = ~0ull;
	return r;
}

static v128 gen_boundary(int idx)
{
	v128 r{};
	static const u32 boundaries[] = {1, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 0x12345678, 0xDEADBEEF};
	for (int i = 0; i < 4; i++)
		r._u32[i] = boundaries[(idx + i) % 6];
	return r;
}

static void gen_test_inputs(test_input_t* inputs, int count)
{
	for (int i = 0; i < count; i++)
	{
		switch (i)
		{
		case 0: inputs[i].ra = gen_zero(); inputs[i].rb = gen_zero(); inputs[i].rc = gen_zero(); break;
		case 1: inputs[i].ra = gen_ones(); inputs[i].rb = gen_ones(); inputs[i].rc = gen_ones(); break;
		case 2: inputs[i].ra = gen_random_v128(); inputs[i].rb = gen_random_v128(); inputs[i].rc = gen_random_v128(); break;
		default: inputs[i].ra = gen_boundary(i); inputs[i].rb = gen_boundary(i + 1); inputs[i].rc = gen_boundary(i + 2); break;
		}
	}
}

// ============================================================================
// Test Execution
// ============================================================================

static void print_v128(const v128& v)
{
	printf("[%08x %08x %08x %08x]", v._u32[3], v._u32[2], v._u32[1], v._u32[0]);
}

// Direct JIT caller trampoline - bypasses the dispatcher
// Takes (spu_thread*, ls*, jit_fn) and calls jit_fn with GHC convention
using jit_direct_call_t = void(*)(void*, void*, void*);
static jit_direct_call_t g_jit_caller = nullptr;

static void init_jit_caller()
{
	if (g_jit_caller) return;

	g_jit_caller = build_function_asm<jit_direct_call_t>("jit_direct_caller", [](native_asm& c, auto& args)
	{
		using namespace asmjit;

#if defined(ARCH_X64)
		// Save callee-saved registers we'll modify
		// Stack alignment: after call (8) + 4 pushes (32) = 40 mod 16 = 8
		// Need sub 0x28 (40) to get (8+40) mod 16 = 0 (aligned before call)
		c.push(x86::rbp);
		c.push(x86::rbx);
		c.push(x86::r12);
		c.push(x86::r13);
		c.sub(x86::rsp, 0x28); // Shadow space + alignment (40 bytes)

		// args[0]=rcx=spu_thread, args[1]=rdx=ls, args[2]=r8=jit_fn
		c.mov(x86::rax, args[2]);   // rax = jit_fn
		c.mov(x86::r13, args[0]);   // r13 = spu_thread (GHC)
		c.mov(x86::rbp, args[1]);   // rbp = ls (GHC)
		c.xor_(x86::r12, x86::r12); // r12 = 0 (rip)
		c.xor_(x86::rbx, x86::rbx); // rbx = 0 (extra)

		c.call(x86::rax);

		c.add(x86::rsp, 0x28);
		c.pop(x86::r13);
		c.pop(x86::r12);
		c.pop(x86::rbx);
		c.pop(x86::rbp);
		c.ret();
#elif defined(ARCH_ARM64)
		// Save callee-saved registers (48-byte frame, matches recompiler prologue)
		c.sub(a64::sp, a64::sp, asmjit::Imm(48));
		c.stp(a64::x19, a64::x20, asmjit::arm::Mem(a64::sp, 0));
		c.stp(a64::x21, a64::x22, asmjit::arm::Mem(a64::sp, 16));
		c.stp(a64::x29, a64::x30, asmjit::arm::Mem(a64::sp, 32));

		// args[0]=x0=spu_thread, args[1]=x1=ls, args[2]=x2=jit_fn
		c.mov(a64::x9, args[2]);    // x9 = jit_fn (temp)
		c.mov(a64::x19, args[0]);   // x19 = spu_thread (GHC)
		c.mov(a64::x20, args[1]);   // x20 = ls (GHC)
		c.mov(a64::x21, a64::xzr);  // x21 = 0 (rip)
		c.mov(a64::x22, a64::xzr);  // x22 = 0 (extra)

		c.blr(a64::x9);

		c.ldp(a64::x29, a64::x30, asmjit::arm::Mem(a64::sp, 32));
		c.ldp(a64::x21, a64::x22, asmjit::arm::Mem(a64::sp, 16));
		c.ldp(a64::x19, a64::x20, asmjit::arm::Mem(a64::sp, 0));
		c.add(a64::sp, a64::sp, asmjit::Imm(48));
		c.ret(a64::x30);
#endif
	});
}

// Run a single instruction through the JIT recompiler.
// The block is a single instruction with no explicit terminator: after the
// instruction executes the recompiler emits a fallthrough (branch_fixed) which,
// with spu_verification disabled, jumps straight to the epilogue and returns.
// This avoids STOP, whose handler calls runtime functions unavailable in the mock.
// Compile + execute a single instruction WITHOUT touching GPRs / channels.
// Callers are responsible for g_ctx.reset() and any channel/GPR setup.
static bool run_jit_test_raw(u32 instruction, v128* out_result, u32 rt_reg = 2)
{
	// spu_program.data holds byte-swapped words on LE platforms: compile() does
	// op = bit_cast<be_t<u32>>(data[i]) which swaps back to the native encoding.
	// Write the instruction to LS[0] byte-swapped (SPU LS endianness); the rest
	// of LS stays zero which the analyser treats as a block terminator.
	be_t<u32>* ls_be = reinterpret_cast<be_t<u32>*>(g_ctx.ls_mem);
	ls_be[0] = instruction;

	// Create recompiler, analyse LS (populates block info) then compile
	auto compiler = spu_recompiler_base::make_asmjit_recompiler();
	compiler->init();

	const u32 swapped = std::bit_cast<u32>(be_t<u32>{instruction});
	spu_program prog;
	prog.entry_point = 0;
	prog.lower_bound = 0;
	prog.data.push_back(swapped);
	spu_function_t fn = compiler->compile(std::move(prog));
	if (!fn)
	{
		printf("  ERROR: JIT compilation failed\n");
		return false;
	}

	// Execute the JIT function directly via trampoline (bypasses dispatcher/STOP)
	init_jit_caller();
	auto* thr = g_ctx.get_thread();
	g_jit_caller(thr, g_ctx.ls_mem, reinterpret_cast<void*>(fn));

	*out_result = *g_ctx.get_gpr(rt_reg);
	return true;
}

static bool run_jit_test(u32 instruction, const test_input_t& input, v128* out_result, u32 rt_reg = 2)
{
	g_ctx.reset();
	g_ctx.set_gpr(0, input.ra);  // Use r0-r2 as input regs
	g_ctx.set_gpr(1, input.rb);
	g_ctx.set_gpr(2, input.rc);
	return run_jit_test_raw(instruction, out_result, rt_reg);
}

// Encode an M-form channel instruction (WRCH / RDCH / RCHCNT):
//   rt = bits 6:0   (value reg for WRCH; dest reg for RDCH/RCHCNT)
//   ra = bits 13:7  (channel number)
// The opcode already encodes the 11-bit primary (e.g. 0x10d for WRCH).
static u32 enc_ch(u32 opcode, u32 rt, u32 ra)
{
	return (opcode << 21) | ((ra & 0x7F) << 7) | (rt & 0x7F);
}

// Test WRCH: write gpr[rt]._u32[3] to the outbound mailbox channel (28).
static test_result_t test_wrch_instruction(int num_cases = 4)
{
	test_result_t result{"WRCH", num_cases, 0, 0};
	test_input_t inputs[8];
	gen_test_inputs(inputs, num_cases);

	for (int i = 0; i < num_cases; i++)
	{
		const u32 instruction = enc_ch(0x10d, 2, SPU_WrOutMbox); // WRCH ch28, rt=2

		v128 jit_result;
		bool ok = run_jit_test(instruction, inputs[i], &jit_result, 2);

		// WRCH stores gpr[2]._u32[3] into ch_out_mbox
		const u32 expected = inputs[i].rc._u32[3];
		const u32 actual = g_ctx.get_thread()->ch_out_mbox.get_value();

		if (ok && actual == expected)
		{
			result.passed++;
		}
		else
		{
			result.failed++;
			printf("  case %d: expected ch_out_mbox=0x%08x actual=0x%08x ok=%d\n",
				i, expected, actual, ok);
		}
	}

	return result;
}

// Test RDCH: read the inbound mailbox channel (29) into gpr[rt]._u32[3].
static test_result_t test_rdch_instruction(int num_cases = 4)
{
	test_result_t result{"RDCH", num_cases, 0, 0};
	test_input_t inputs[8];
	gen_test_inputs(inputs, num_cases);

	for (int i = 0; i < num_cases; i++)
	{
		const u32 instruction = enc_ch(0x0d, 2, SPU_RdInMbox); // RDCH ch29, rt=2

		// Pre-populate the inbound mailbox with a known value (word3 of rc).
		g_ctx.reset();
		g_ctx.set_gpr(0, inputs[i].ra);
		g_ctx.set_gpr(1, inputs[i].rb);
		g_ctx.set_gpr(2, inputs[i].rc);
		(void)g_ctx.get_thread()->ch_in_mbox.push(inputs[i].rc._u32[3]);

		v128 jit_result;
		run_jit_test_raw(instruction, &jit_result, 2);

		const u32 expected = inputs[i].rc._u32[3];
		const u32 actual = jit_result._u32[3];

		if (actual == expected)
		{
			result.passed++;
		}
		else
		{
			result.failed++;
			printf("  case %d: expected gpr[2].w3=0x%08x actual=0x%08x\n",
				i, expected, actual);
		}
	}

	return result;
}

// Test RCHCNT: read the inbound mailbox channel count (should be 1) into gpr[rt]._u32[3].
static test_result_t test_rchcnt_instruction(int num_cases = 4)
{
	test_result_t result{"RCHCNT", num_cases, 0, 0};

	for (int i = 0; i < num_cases; i++)
	{
		const u32 instruction = enc_ch(0x10c, 2, SPU_RdInMbox); // RCHCNT ch29, rt=2

		g_ctx.reset();
		// Populate the inbound mailbox so its count is 1.
		(void)g_ctx.get_thread()->ch_in_mbox.push(0x12345678u + static_cast<u32>(i));

		v128 jit_result;
		run_jit_test_raw(instruction, &jit_result, 2);

		const u32 actual = jit_result._u32[3];
		if (actual == 1)
		{
			result.passed++;
		}
		else
		{
			result.failed++;
			printf("  case %d: expected count=1 actual=0x%08x\n", i, actual);
		}
	}

	return result;
}

// Test WRCH MFC_WrTagMask: write gpr[rt]._u32[3] into ch_tag_mask.
static test_result_t test_wrch_tagmask(int num_cases = 4)
{
	test_result_t result{"WrTagMsk", num_cases, 0, 0};
	test_input_t inputs[8];
	gen_test_inputs(inputs, num_cases);

	for (int i = 0; i < num_cases; i++)
	{
		const u32 instruction = enc_ch(0x10d, 2, MFC_WrTagMask); // WRCH MFC_WrTagMask, rt=2

		g_ctx.reset();
		g_ctx.set_gpr(2, inputs[i].rc);

		v128 jit_result;
		run_jit_test_raw(instruction, &jit_result, 2);

		const u32 expected = inputs[i].rc._u32[3];
		const u32 actual = g_ctx.get_thread()->ch_tag_mask;

		if (actual == expected)
			result.passed++;
		else
		{
			result.failed++;
			printf("  case %d: WrTagMask expected=0x%08x actual=0x%08x\n", i, expected, actual);
		}
	}

	return result;
}

// Test RDCH MFC_RdTagStat: pre-set ch_tag_stat, read it back into gpr[rt]._u32[3].
static test_result_t test_rdch_tagstat(int num_cases = 4)
{
	test_result_t result{"RdTagStat", num_cases, 0, 0};
	test_input_t inputs[8];
	gen_test_inputs(inputs, num_cases);

	for (int i = 0; i < num_cases; i++)
	{
		const u32 instruction = enc_ch(0x0d, 2, MFC_RdTagStat); // RDCH MFC_RdTagStat, rt=2

		g_ctx.reset();
		g_ctx.get_thread()->ch_tag_stat.set_value(inputs[i].rc._u32[3]);

		v128 jit_result;
		run_jit_test_raw(instruction, &jit_result, 2);

		const u32 expected = inputs[i].rc._u32[3];
		const u32 actual = jit_result._u32[3];

		if (actual == expected)
			result.passed++;
		else
		{
			result.failed++;
			printf("  case %d: RdTagStat expected=0x%08x actual=0x%08x\n", i, expected, actual);
		}
	}

	return result;
}

// Full DMA-completion notification chain WITHOUT real DMA (mfc_fence stays 0):
//   WRCH MFC_WrTagMask  <- tag mask
//   WRCH MFC_WrTagUpdate<- MFC_TAG_UPDATE_ALL (2)  -> should set ch_tag_stat (=mask)
//   RDCH MFC_RdTagStat  -> should read back the mask
static test_result_t test_tag_completion_chain()
{
	test_result_t result{"TagChain", 1, 0, 0};
	const u32 mask = 0x80000000u; // tag 31

	g_ctx.reset();

	// 1) WRCH MFC_WrTagMask = mask
	v128 maskv{};
	maskv._u32[3] = mask;
	g_ctx.set_gpr(2, maskv);
	{
		v128 r;
		run_jit_test_raw(enc_ch(0x10d, 2, MFC_WrTagMask), &r, 2);
	}

	// 2) WRCH MFC_WrTagUpdate = MFC_TAG_UPDATE_ALL (2)
	v128 updv{};
	updv._u32[3] = 2;
	g_ctx.set_gpr(2, updv);
	{
		v128 r;
		run_jit_test_raw(enc_ch(0x10d, 2, MFC_WrTagUpdate), &r, 2);
	}

	// 3) RDCH MFC_RdTagStat
	{
		v128 r;
		run_jit_test_raw(enc_ch(0x0d, 2, MFC_RdTagStat), &r, 2);

		if (r._u32[3] == mask)
			result.passed++;
		else
		{
			result.failed++;
			printf("  TagChain: expected RdTagStat=0x%08x actual=0x%08x ch_tag_mask=0x%08x mfc_fence=0x%08x\n",
				mask, r._u32[3], g_ctx.get_thread()->ch_tag_mask, g_ctx.get_thread()->mfc_fence);
		}
	}

	return result;
}

// ADDX: rt = ra + rb + (old_rt & 1) per word. The carry comes from the
// pre-existing rt value (set via rc in run_jit_test). Also test the aliasing
// case rt==ra which is a known hazard for in-place JIT sequences.
static test_result_t test_addx_instruction(bool alias_rt_ra, int num_cases = 4)
{
	test_result_t result{alias_rt_ra ? "ADDXalias" : "ADDX", num_cases, 0, 0};
	test_input_t inputs[8];
	gen_test_inputs(inputs, num_cases);

	for (int i = 0; i < num_cases; i++)
	{
		const u32 rt = alias_rt_ra ? 0 : 2;
		const u32 ra = alias_rt_ra ? 0 : 0;
		const u32 instruction = enc_rr(0x340, rt, ra, 1); // ADDX rt, ra, r1

		v128 jit_result;
		const bool ok = run_jit_test(instruction, inputs[i], &jit_result, rt);

		v128 expected{};
		for (int w = 0; w < 4; w++)
		{
			// old rt (carry source) is gpr[rt]: for the alias case rt==ra==r0 it is
			// input.ra (gpr[0]); otherwise gpr[2] == input.rc.
			const u32 carry_src = alias_rt_ra ? inputs[i].ra._u32[w] : inputs[i].rc._u32[w];
			const u32 carry = carry_src & 1;
			expected._u32[w] = inputs[i].ra._u32[w] + inputs[i].rb._u32[w] + carry;
		}

		if (ok && memcmp(&jit_result, &expected, sizeof(v128)) == 0)
			result.passed++;
		else
		{
			result.failed++;
			printf("  case %d: expected ", i); print_v128(expected); printf("\n    actual   "); print_v128(jit_result); printf("\n");
		}
	}

	return result;
}

// ILA: opcode 0x21 (magn=4), i18 spans bits 7:24. Encoding differs from the
// magn=3 immediate helper, so build it directly.
static test_result_t test_ila_instruction(int num_cases = 4)
{
	test_result_t result{"ILA", num_cases, 0, 0};
	test_input_t inputs[8];
	gen_test_inputs(inputs, num_cases);

	for (int i = 0; i < num_cases; i++)
	{
		const u32 i18 = (0x2d80u + i * 0x1111u) & 0x3ffff;
		const u32 instruction = (0x21u << 25) | (i18 << 7) | 2; // rt=2

		v128 jit_result;
		const bool ok = run_jit_test(instruction, inputs[i], &jit_result, 2);

		v128 expected{};
		for (int w = 0; w < 4; w++)
			expected._u32[w] = i18;

		if (ok && memcmp(&jit_result, &expected, sizeof(v128)) == 0)
			result.passed++;
		else
		{
			result.failed++;
			printf("  case %d i18=0x%05x: expected ", i, i18); print_v128(expected); printf("\n    actual   "); print_v128(jit_result); printf("\n");
		}
	}

	return result;
}

static test_result_t test_rr_instruction(const char* name, u32 opcode,
	v128(*expected_fn)(const v128&, const v128&), int num_cases = 4)
{
	test_result_t result{name, num_cases, 0, 0};
	test_input_t inputs[8];
	gen_test_inputs(inputs, num_cases);

	for (int i = 0; i < num_cases; i++)
	{
		const u32 instruction = enc_rr(opcode, 2, 0, 1); // rt=2, ra=0, rb=1

		v128 jit_result;
		bool ok = run_jit_test(instruction, inputs[i], &jit_result, 2);

		v128 expected = expected_fn(inputs[i].ra, inputs[i].rb);

		if (ok && memcmp(&jit_result, &expected, sizeof(v128)) == 0)
		{
			result.passed++;
		}
		else
		{
			result.failed++;
			printf("  case %d: input ra=%08x%08x%08x%08x rb=%08x%08x%08x%08x\n",
				i,
				inputs[i].ra._u32[3], inputs[i].ra._u32[2], inputs[i].ra._u32[1], inputs[i].ra._u32[0],
				inputs[i].rb._u32[3], inputs[i].rb._u32[2], inputs[i].rb._u32[1], inputs[i].rb._u32[0]);
			printf("    expected: "); print_v128(expected); printf("\n");
			printf("    actual:   "); print_v128(jit_result); printf("\n");
		}
	}

	return result;
}

// Test an immediate instruction (rt = f(ra, imm))
static test_result_t test_ri_instruction(const char* name, u32 opcode, s32 imm,
	v128(*expected_fn)(const v128&, s32), int num_cases = 4)
{
	test_result_t result{name, num_cases, 0, 0};
	test_input_t inputs[8];
	gen_test_inputs(inputs, num_cases);

	for (int i = 0; i < num_cases; i++)
	{
		const u32 instruction = enc_ri10(opcode, 2, 0, imm); // rt=2, ra=0, imm

		v128 jit_result;
		bool ok = run_jit_test(instruction, inputs[i], &jit_result, 2);

		v128 expected = expected_fn(inputs[i].ra, imm);

		if (ok && memcmp(&jit_result, &expected, sizeof(v128)) == 0)
		{
			result.passed++;
		}
		else
		{
			result.failed++;
			printf("  case %d: input ra=%08x%08x%08x%08x imm=%d\n",
				i,
				inputs[i].ra._u32[3], inputs[i].ra._u32[2], inputs[i].ra._u32[1], inputs[i].ra._u32[0],
				imm);
			printf("    expected: "); print_v128(expected); printf("\n");
			printf("    actual:   "); print_v128(jit_result); printf("\n");
		}
	}

	return result;
}

// Test an i7-immediate quadword instruction (rt = f(ra, i7)); opcode already
// encodes the 11-bit primary, i7 occupies bits 14:20.
static test_result_t test_ri7_instruction(const char* name, u32 opcode, s32 i7,
	v128(*expected_fn)(const v128&, s32), int num_cases = 4)
{
	test_result_t result{name, num_cases, 0, 0};
	test_input_t inputs[8];
	gen_test_inputs(inputs, num_cases);

	for (int i = 0; i < num_cases; i++)
	{
		const u32 instruction = (opcode << 21) | ((static_cast<u32>(i7) & 0x7F) << 14) | ((0 & 0x7F) << 7) | (2 & 0x7F); // rt=2, ra=0

		v128 jit_result;
		bool ok = run_jit_test(instruction, inputs[i], &jit_result, 2);

		v128 expected = expected_fn(inputs[i].ra, i7);

		if (ok && memcmp(&jit_result, &expected, sizeof(v128)) == 0)
		{
			result.passed++;
		}
		else
		{
			result.failed++;
			printf("  case %d: input ra=%08x%08x%08x%08x i7=%d\n",
				i,
				inputs[i].ra._u32[3], inputs[i].ra._u32[2], inputs[i].ra._u32[1], inputs[i].ra._u32[0],
				i7);
			printf("    expected: "); print_v128(expected); printf("\n");
			printf("    actual:   "); print_v128(jit_result); printf("\n");
		}
	}

	return result;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
	setvbuf(stdout, nullptr, _IONBF, 0); // unbuffered so output survives crashes
	printf("=== SPU ASMJIT Test Suite ===\n");
#ifdef ARCH_X64
	printf("Architecture: x86_64\n");
#elif defined(ARCH_ARM64)
	printf("Architecture: aarch64\n");
#else
	printf("Architecture: unknown\n");
#endif
	printf("---\n");
	fflush(stdout);

	// Initialize global runtime for JIT
	jit_runtime::initialize();

	// Disable SPU verification to avoid code verification issues in test
	g_cfg.core.spu_verification.set(false);

	// Check if gateway is available
	if (!spu_runtime::g_gateway)
	{
		printf("ERROR: g_gateway is null\n");
		return 1;
	}
	printf("Gateway initialized OK\n");
	fflush(stdout);

	std::vector<test_result_t> results;

	// === Basic Arithmetic ===
	results.push_back(test_rr_instruction("A", 0xC0, expected_A));
	results.push_back(test_rr_instruction("SF", 0x40, expected_SF));
	results.push_back(test_rr_instruction("AH", 0xC8, expected_AH));

	// === Bitwise ===
	results.push_back(test_rr_instruction("AND", 0xC1, expected_AND));
	results.push_back(test_rr_instruction("OR", 0x41, expected_OR));
	results.push_back(test_rr_instruction("XOR", 0x241, expected_XOR));
	results.push_back(test_rr_instruction("NOR", 0x49, expected_NOR));
	results.push_back(test_rr_instruction("NAND", 0xC9, expected_NAND));

	// === Immediate Arithmetic === (value8 = decoder magn=3 value)
	results.push_back(test_ri_instruction("AI", 0x1C, 42, expected_AI));
	results.push_back(test_ri_instruction("SFI", 0x0C, 42, expected_SFI));
	results.push_back(test_ri_instruction("AHI", 0x1D, 42, expected_AHI));

	// === Compare ===
	results.push_back(test_rr_instruction("CGT", 0x240, expected_CGT));
	results.push_back(test_rr_instruction("CLGT", 0x2C0, expected_CLGT));
	results.push_back(test_rr_instruction("CEQ", 0x3C0, expected_CEQ));

	// === Shift/Rotate ===
	results.push_back(test_rr_instruction("SHL", 0x5B, expected_SHL));
	results.push_back(test_rr_instruction("ROTM", 0x59, expected_ROTM));
	results.push_back(test_rr_instruction("ROTMA", 0x5A, expected_ROTMA));
	results.push_back(test_rr_instruction("ROT", 0x58, expected_ROT));
	results.push_back(test_rr_instruction("ROTH", 0x5C, expected_ROTH));
	results.push_back(test_rr_instruction("ROTHM", 0x5D, expected_ROTHM));
	results.push_back(test_rr_instruction("ROTMAH", 0x5E, expected_ROTMAH));
	results.push_back(test_rr_instruction("SHLH", 0x5F, expected_SHLH));

	// === Additional Arithmetic/Logic ===
	results.push_back(test_rr_instruction("SFH", 0x48, expected_SFH));
	results.push_back(test_rr_instruction("ANDC", 0x2C1, expected_ANDC));
	results.push_back(test_rr_instruction("ORC", 0x2C9, expected_ORC));
	results.push_back(test_rr_instruction("EQV", 0x249, expected_EQV));

	// === Compare (halfword/byte) ===
	results.push_back(test_rr_instruction("CGTH", 0x248, expected_CGTH));
	results.push_back(test_rr_instruction("CGTB", 0x250, expected_CGTB));
	results.push_back(test_rr_instruction("CLGTH", 0x2C8, expected_CLGTH));
	results.push_back(test_rr_instruction("CLGTB", 0x2D0, expected_CLGTB));
	results.push_back(test_rr_instruction("CEQH", 0x3C8, expected_CEQH));
	results.push_back(test_rr_instruction("CEQB", 0x3D0, expected_CEQB));

	// === Byte manipulation ===
	results.push_back(test_rr_instruction("CNTB", 0x2B4, expected_CNTB));
	results.push_back(test_rr_instruction("CLZ", 0x2A5, expected_CLZ));
	results.push_back(test_rr_instruction("XSBH", 0x2B6, expected_XSBH));
	results.push_back(test_rr_instruction("XSHW", 0x2AE, expected_XSHW));
	results.push_back(test_rr_instruction("XSWD", 0x2A6, expected_XSWD));
	results.push_back(test_rr_instruction("ABSDB", 0x53, expected_ABSDB));
	results.push_back(test_rr_instruction("AVGB", 0xD3, expected_AVGB));
	results.push_back(test_rr_instruction("ORX", 0x1F0, expected_ORX));

	// === Byte-rotate / byte-shift quadword (i7 immediate) ===
	results.push_back(test_ri7_instruction("SHLQBYI", 0x1FF, 0, expected_SHLQBYI));
	results.push_back(test_ri7_instruction("SHLQBYI", 0x1FF, 4, expected_SHLQBYI));
	results.push_back(test_ri7_instruction("SHLQBYI", 0x1FF, 15, expected_SHLQBYI));
	results.push_back(test_ri7_instruction("ROTQBYI", 0x1FC, 1, expected_ROTQBYI));
	results.push_back(test_ri7_instruction("ROTQBYI", 0x1FC, 4, expected_ROTQBYI));
	results.push_back(test_ri7_instruction("ROTQBYI", 0x1FC, 8, expected_ROTQBYI));
	results.push_back(test_ri7_instruction("ROTQMBYI", 0x1FD, 0x7C /*-4*/, expected_ROTQMBYI));
	results.push_back(test_ri7_instruction("ROTQMBYI", 0x1FD, 0x7F /*-1*/, expected_ROTQMBYI));

	// === Channel instructions (WRCH / RDCH / RCHCNT) ===
	results.push_back(test_wrch_instruction());
	results.push_back(test_rdch_instruction());
	//results.push_back(test_rchcnt_instruction());

	// === MFC tag channels (DMA completion notification path) ===
	results.push_back(test_wrch_tagmask());
	results.push_back(test_rdch_tagstat());
	results.push_back(test_tag_completion_chain());

	// === SPURS-path native instructions (untested before) ===
	results.push_back(test_ri_instruction("CEQBI", 0x7e, 0x42, expected_CEQBI));
	results.push_back(test_ri_instruction("CEQBI", 0x7e, 0x00, expected_CEQBI));
	results.push_back(test_ri_instruction("ANDBI", 0x16, 0x80, expected_ANDBI));
	results.push_back(test_ri_instruction("ANDBI", 0x16, 0x0f, expected_ANDBI));
	results.push_back(test_ila_instruction());
	results.push_back(test_rr_instruction("CG", 0xc2, expected_CG));
	results.push_back(test_addx_instruction(false));
	results.push_back(test_addx_instruction(true));

	// === Summary ===
	printf("---\n");
	int total_pass = 0, total_fail = 0, total_tests = 0;
	for (const auto& r : results)
	{
		printf("%-8s | %s (%d/%d)\n", r.name, r.failed == 0 ? "PASS" : "FAIL", r.passed, r.total);
		total_pass += r.passed;
		total_fail += r.failed;
		total_tests += r.total;
	}

	printf("=== Summary: %d/%d PASS, %d FAIL ===\n", total_pass, total_tests, total_fail);

	return total_fail > 0 ? 1 : 0;
}

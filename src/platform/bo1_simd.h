// bo1_simd.h - accesso portabile ai lane di __m128 (MSVC: union con m128_f32...; GCC/clang: tipo vettoriale).
// Su ARM __m128 arriva da sse2neon (float32x4_t), stesso discorso.
#pragma once

#if defined(_MSC_VER)
#define BO1_M128_F32(v) ((v).m128_f32)
#define BO1_M128_U32(v) ((v).m128_u32)
#define BO1_M128_I32(v) ((v).m128_i32)
#define BO1_M128_U64(v) ((v).m128_u64)
#define BO1_M128_I64(v) ((v).m128_i64)
#define BO1_M128_U16(v) ((v).m128_u16)
#define BO1_M128_U8(v) ((v).m128_u8)
#define BO1_M128_INIT(...) { { __VA_ARGS__ } }
#else
#include <stdint.h>
#define BO1_M128_F32(v) ((float *)&(v))
#define BO1_M128_U32(v) ((uint32_t *)&(v))
#define BO1_M128_I32(v) ((int32_t *)&(v))
#define BO1_M128_U64(v) ((uint64_t *)&(v))
#define BO1_M128_I64(v) ((int64_t *)&(v))
#define BO1_M128_U16(v) ((uint16_t *)&(v))
#define BO1_M128_U8(v) ((uint8_t *)&(v))
#define BO1_M128_INIT(...) { __VA_ARGS__ }
#endif

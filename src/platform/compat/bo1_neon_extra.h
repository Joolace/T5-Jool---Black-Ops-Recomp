// bo1_neon_extra.h - sse2neon piu' le poche intrinseche MMX che usa il motore e che sse2neon non ha
// (skinning SSE in r_model_skin_sse.cpp). Solo ARM.
#pragma once
#include <sse2neon.h>

#ifndef _m_empty
#define _m_empty _mm_empty // MMX: su NEON non c'e' stato da svuotare
#endif

static inline __m64 bo1_mm_cvtsi32_si64(int a) { return vreinterpret_s64_u32(vset_lane_u32((uint32_t)a, vdup_n_u32(0), 0)); }
static inline __m64 bo1_mm_setzero_si64(void) { return vdup_n_s64(0); }
static inline __m64 bo1_m_punpcklbw(__m64 a, __m64 b) { return vreinterpret_s64_u8(vzip1_u8(vreinterpret_u8_s64(a), vreinterpret_u8_s64(b))); }
static inline __m64 bo1_m_punpcklwd(__m64 a, __m64 b) { return vreinterpret_s64_u16(vzip1_u16(vreinterpret_u16_s64(a), vreinterpret_u16_s64(b))); }
static inline __m64 bo1_m_packuswb(__m64 a, __m64 b) { return vreinterpret_s64_u8(vqmovun_s16(vcombine_s16(vreinterpret_s16_s64(a), vreinterpret_s16_s64(b)))); }
#ifndef _mm_cvtsi32_si64
#define _mm_cvtsi32_si64 bo1_mm_cvtsi32_si64
#endif
#ifndef _mm_setzero_si64
#define _mm_setzero_si64 bo1_mm_setzero_si64
#endif
#ifndef _m_punpcklbw
#define _m_punpcklbw bo1_m_punpcklbw
#endif
#ifndef _m_punpcklwd
#define _m_punpcklwd bo1_m_punpcklwd
#endif
#ifndef _m_packuswb
#define _m_packuswb bo1_m_packuswb
#endif

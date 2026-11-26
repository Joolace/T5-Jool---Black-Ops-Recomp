#pragma once
// Su x86 l'header vero del compilatore; su ARM le stesse intrinseche tradotte in NEON da sse2neon.
#if defined(__i386__) || defined(__x86_64__)
#include_next <mmintrin.h>
#else
#include "bo1_neon_extra.h"
#endif

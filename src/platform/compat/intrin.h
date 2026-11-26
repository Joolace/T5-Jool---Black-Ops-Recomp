#pragma once
#if defined(__i386__) || defined(__x86_64__)
#include <x86intrin.h>
#else
#include "bo1_neon_extra.h"
#endif
#include "win_compat.h"

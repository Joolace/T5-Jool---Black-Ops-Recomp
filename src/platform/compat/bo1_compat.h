// bo1_compat.h - incluso forzatamente (-include) in ogni file quando si compila con clang fuori da Windows.
// Raccoglie le differenze tra MSVC/Win32 e Linux/Android senza toccare il codice decompilato.
#pragma once

#if !defined(_WIN32)

#ifndef BO1_PLATFORM_SDL
#define BO1_PLATFORM_SDL 1
#endif

// --- Header di sistema inclusi PRIMA dei rename qui sotto, così le loro dichiarazioni restano intatte ---
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <float.h>
#include <malloc.h>
#include <unistd.h>
#include <strings.h>
#include <wchar.h>
#ifdef __cplusplus
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <new>
#include <algorithm>
#include <limits>
#include <vector>
#include <string>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <csetjmp>
#include <ctime>
#include <cwchar>
#include <cctype>
// Tutti gli header standard che possono usare min/max come nomi vanno inclusi PRIMA delle macro min/max.
#include <chrono>
#include <condition_variable>
#include <deque>
#include <queue>
#include <stack>
#include <array>
#include <tuple>
#include <optional>
#include <variant>
#include <numeric>
#include <random>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <bitset>
#include <valarray>
#include <unordered_set>
#include <type_traits>
#include <utility>
#include <iterator>
#include <cassert>
#include <cfloat>
#include <climits>
#include <cstdarg>
#include <cstddef>
#include <cinttypes>
#endif
#include <setjmp.h>
#include <libgen.h>

// --- Nomi del motore che collidono con funzioni POSIX/libc ---
// Il motore definisce le proprie random(), crandom() ecc.: le rinominiamo solo nel codice del gioco.
#define random bo1_random
#define lerp bo1_lerp // C++20 ha std::lerp, che libstdc++ porta anche nello spazio globale
#define index bo1_index // POSIX index() in <strings.h>
#undef basename
#define basename bo1_basename // POSIX/GNU basename() in <libgen.h>

// --- Parole chiave MSVC (in parte già accettate da clang con -fms-extensions) ---
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif
// Le calling convention x86 non esistono su ARM/x86_64 e su Linux i386 non servono (non chiamiamo DLL di
// Windows): le togliamo del tutto, così dichiarazioni e definizioni non possono più divergere.
#define __cdecl
#define __stdcall
#define __fastcall
#define __thiscall
#define __vectorcall
#define WINAPI __stdcall
#define CALLBACK __stdcall
#define APIENTRY __stdcall

#include "win_compat.h"

// windows.h definisce min/max come macro e il codice decompilato ci conta. Le definiamo dopo aver incluso
// gli header della libreria standard qui sopra, così non rompono std::min/numeric_limits::max.
#ifndef NOMINMAX
// fopen/rename con percorsi in stile Windows ("main\\file.cfg", maiuscole diverse dal disco): passano da
// bo1_resolve_path (src/platform/win_compat.cpp). Macro con argomenti, così std::... e i puntatori a funzione
// non vengono toccati.
#if !defined(_WIN32)
#ifdef __cplusplus
extern "C" {
#endif
FILE *bo1_fopen(const char *path, const char *mode);
int bo1_rename(const char *from, const char *to);
int bo1_rand(void);
void bo1_srand(unsigned int seed);
#ifdef __cplusplus
}
#endif
#define fopen(p, m) bo1_fopen((p), (m))
// rand() come nel CRT di Microsoft: 0..32767 (glibc arriva a 2^31-1). Il motore divide per 32768
// (random() in com_math.cpp): con i valori di glibc GaussianRandom girava all'infinito.
#define rand() bo1_rand()
#define srand(s) bo1_srand(s)
#undef RAND_MAX
#define RAND_MAX 0x7fff
#define rename(a, b) bo1_rename((a), (b))
#endif

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#endif

#endif // !_WIN32

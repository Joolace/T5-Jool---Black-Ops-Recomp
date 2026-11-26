// win_compat.h - sottoinsieme dell'API Win32 usato dal motore, reimplementato sopra POSIX.
//
// Con DXVK-native i tipi base (DWORD, HRESULT, GUID, RECT, HWND, ...) arrivano da <windows_base.h>,
// che è lo stesso header che usa il suo d3d9.h: lo includiamo per primo così i tipi coincidono.
// Senza DXVK (es. solo controllo sintassi) definiamo noi gli stessi tipi con le stesse dimensioni.
// Nota: su Windows LONG/DWORD sono SEMPRE 32 bit, anche su 64 bit: qui usiamo int32_t/uint32_t.
#pragma once

#if !defined(_WIN32)

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <wchar.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <alloca.h>
#include <malloc.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Tipi scalari: definiti sempre. Se windows_base.h di DXVK li ha già, un typedef identico è ammesso in C/C++.
typedef int32_t  INT;
typedef uint32_t UINT;
typedef int32_t  LONG;
typedef uint32_t ULONG;
typedef int32_t  *LPLONG;
typedef int64_t  LONGLONG;
typedef uint64_t ULONGLONG;
typedef uint32_t DWORD;
typedef uint32_t *LPDWORD;
typedef uint16_t WORD;
typedef uint8_t  BYTE;
typedef int32_t  BOOL;
typedef float    FLOAT;
typedef char     CHAR;
typedef wchar_t  WCHAR;
typedef int32_t  HRESULT;
typedef void    *PVOID;
typedef void    *LPVOID;
typedef const void *LPCVOID;
typedef char    *LPSTR;
typedef const char *LPCSTR;
typedef wchar_t *LPWSTR;
typedef const wchar_t *LPCWSTR;
typedef uintptr_t ULONG_PTR;
typedef intptr_t  LONG_PTR;
typedef size_t    SIZE_T;
typedef ULONG_PTR DWORD_PTR;
typedef uintptr_t UINT_PTR;
typedef intptr_t  INT_PTR;
typedef void *HANDLE;
typedef HANDLE HWND;
typedef HANDLE HMONITOR;
typedef HANDLE HDC;
typedef HANDLE HMODULE;
typedef HANDLE HINSTANCE;
typedef HANDLE HKEY;
typedef HANDLE HICON;
typedef HANDLE HCURSOR;
typedef HANDLE HBRUSH;
typedef HANDLE HMENU;
typedef HANDLE HGLOBAL;
#if !defined(__cplusplus) && !defined(nullptr)
#define nullptr ((void *)0) // le funzioni inline qui sotto servono anche ai file .c (speex, libtomcrypt...)
#endif

#if __has_include(<windows_base.h>)
// DXVK-native: strutture e macro (GUID, RECT, LARGE_INTEGER, FAILED...) dal suo header, lo stesso del suo d3d9.h.
// MEMORYSTATUS e SECURITY_ATTRIBUTES lì sono ridotti (2 campi): li rinominiamo e usiamo le versioni complete qui sotto.
#define MEMORYSTATUS bo1_dxvk_MEMORYSTATUS
#define SECURITY_ATTRIBUTES bo1_dxvk_SECURITY_ATTRIBUTES
#include <windows_base.h>
#undef MEMORYSTATUS
#undef SECURITY_ATTRIBUTES
#include <unknwn.h> // IUnknown di DXVK: il suo d3d9.h lo dà per scontato (include <windows.h>, che qui è il nostro)
#define BO1_HAVE_DXVK_WINBASE 1
// nomi delle strutture usati dal codice decompilato (DXVK usa "struct RECT", Windows "struct tagRECT")
typedef RECT tagRECT;
typedef POINT tagPOINT;
typedef SIZE tagSIZE;
typedef LARGE_INTEGER _LARGE_INTEGER;
typedef LUID _LUID;
typedef PALETTEENTRY tagPALETTEENTRY;
#else
#ifndef VOID
#define VOID void
#endif
// ------------------------------------------------------------------------------------------------
// Strutture e macro base (stesso layout di windows_base.h di DXVK-native)
// ------------------------------------------------------------------------------------------------
typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; };
    struct { DWORD LowPart; LONG HighPart; } u;
    int64_t QuadPart;
} LARGE_INTEGER;

typedef struct GUID { uint32_t Data1; uint16_t Data2; uint16_t Data3; uint8_t Data4[8]; } GUID;
typedef GUID IID;
typedef struct tagRECT { LONG left, top, right, bottom; } RECT, *LPRECT;
typedef struct tagPOINT { LONG x, y; } POINT, *LPPOINT;
typedef struct tagSIZE { LONG cx, cy; } SIZE;
typedef struct _LUID { DWORD LowPart; LONG HighPart; } LUID;
typedef struct _PALETTEENTRY { BYTE peRed, peGreen, peBlue, peFlags; } PALETTEENTRY;

#define TRUE 1
#define FALSE 0
#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_FAIL ((HRESULT)0x80004005)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define E_INVALIDARG ((HRESULT)0x80070057)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define MAKE_HRESULT(sev, fac, code) ((HRESULT)(((uint32_t)(sev) << 31) | ((uint32_t)(fac) << 16) | ((uint32_t)(code))))
#endif // windows_base.h
typedef const RECT *LPCRECT;

// ------------------------------------------------------------------------------------------------
// Tipi e macro che windows_base.h di DXVK non fornisce
// ------------------------------------------------------------------------------------------------
typedef int16_t SHORT;
typedef uint16_t USHORT;
typedef unsigned char UCHAR;
typedef uint8_t BOOLEAN;
typedef int64_t __time64_t_compat;
typedef DWORD *PDWORD;
typedef BYTE *PBYTE;
typedef ULONG *PULONG;
typedef USHORT *PUSHORT;
typedef BYTE *LPBYTE;
typedef ULONG_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef int64_t INT64;
typedef int64_t LONG64;
typedef uint64_t ULONG64;
typedef uint64_t UINT64;
typedef uint64_t DWORD64;
typedef uint64_t DWORDLONG;
typedef int32_t INT32;
typedef uint32_t UINT32;
typedef LONG volatile *PLONG_VOLATILE;
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);
typedef intptr_t (*FARPROC)(void);
typedef HANDLE HFONT;
typedef HANDLE HBITMAP;
typedef struct _TOKEN_PRIVILEGES { DWORD PrivilegeCount; struct { LUID Luid; DWORD Attributes; } Privileges[1]; } TOKEN_PRIVILEGES, *PTOKEN_PRIVILEGES;

typedef struct _FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; } FILETIME;
typedef struct _SYSTEMTIME {
    WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;

#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef INFINITE
#define INFINITE 0xFFFFFFFFu
#endif
#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#endif
#define STILL_ACTIVE 259
#define WAIT_OBJECT_0 0u
#define WAIT_ABANDONED 0x80u
#define WAIT_TIMEOUT 258u
#define WAIT_FAILED 0xFFFFFFFFu
#define MAXIMUM_WAIT_OBJECTS 64
#define ERROR_SUCCESS 0L
#define ERROR_ALREADY_EXISTS 183L
#define CREATE_SUSPENDED 0x4
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_LOWEST (-2)
#define THREAD_PRIORITY_TIME_CRITICAL 15
#define TLS_OUT_OF_INDEXES 0xFFFFFFFFu
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_DECOMMIT 0x4000
#define MEM_RELEASE 0x8000
#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_EXECUTE_READWRITE 0x40
#define MB_OK 0x0
#define MB_ICONERROR 0x10
#define MB_ICONSTOP MB_ICONERROR
#define MB_ICONWARNING 0x30
#define MB_YESNO 0x4
#define MB_OKCANCEL 0x1
#define MB_TOPMOST 0x40000
#define MB_SETFOREGROUND 0x10000
#define IDOK 1
#define IDCANCEL 2
#define IDYES 6
#define IDNO 7

#ifndef LOWORD
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#endif

#ifndef _countof
#ifdef __cplusplus
template <typename T, size_t N> char (*bo1_countof_helper(T (&)[N]))[N];
#define _countof(a) (sizeof(*bo1_countof_helper(a)))
#else
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#endif
#endif

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(p) ((void)(p))
#endif

// ------------------------------------------------------------------------------------------------
// CRT di MSVC
// ------------------------------------------------------------------------------------------------
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _strcmpi strcasecmp
#define strcmpi strcasecmp
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define _strdup strdup
#define _unlink unlink
#ifdef __cplusplus
extern "C"
#endif
int bo1_access(const char *path, int mode); // risolve il percorso
#define _access bo1_access
#define _getcwd getcwd
#define _chdir chdir
#define _fileno fileno
#define _isnan isnan
#define _finite isfinite
#define _alloca alloca
#define _stat stat
#define _fstat fstat
#define _ftelli64 ftello
#define _fseeki64 fseeko
#define _atoi64 atoll
#define _strtoi64 strtoll
#define _strtoui64 strtoull
#define _wcsicmp wcscasecmp
#define __debugbreak() __builtin_trap()
#define _byteswap_ushort __builtin_bswap16
#define _byteswap_ulong __builtin_bswap32
#define _byteswap_uint64 __builtin_bswap64
#define _aligned_malloc(size, align) bo1_aligned_malloc((size), (align))
#define _aligned_free free
#define _aligned_realloc(ptr, size, align) bo1_aligned_realloc((ptr), (size), (align))
#define _TRUNCATE ((size_t)-1)
typedef int (*_CoreCrtNonSecureSearchSortCompareFunction)(const void *, const void *);
#define _putenv putenv
#define _errno() (&errno)
#define _getpid getpid
#define _isatty isatty
#define _write write
#define _read read
#define _close close
#define _lseek lseek

#ifdef __cplusplus
extern "C" {
#endif

static inline void *bo1_aligned_malloc(size_t size, size_t align)
{
    void *p = NULL;
    if (align < sizeof(void *)) align = sizeof(void *);
    return posix_memalign(&p, align, size) == 0 ? p : NULL;
}

static inline void *bo1_aligned_realloc(void *ptr, size_t size, size_t align)
{
    if (!ptr) return bo1_aligned_malloc(size, align);
    if (!size) { free(ptr); return NULL; }
    void *n = bo1_aligned_malloc(size, align);
    if (!n) return NULL;
    size_t old = malloc_usable_size(ptr);
    memcpy(n, ptr, old < size ? old : size);
    free(ptr);
    return n;
}
static inline int _vsnwprintf_s(wchar_t *buf, size_t size, size_t count, const wchar_t *fmt, va_list ap)
{
    (void)count; int r = vswprintf(buf, size, fmt, ap);
    return r < 0 ? -1 : r;
}
static inline int _snwprintf_s(wchar_t *buf, size_t size, size_t count, const wchar_t *fmt, ...)
{
    va_list ap; va_start(ap, fmt); int r = _vsnwprintf_s(buf, size, count, fmt, ap); va_end(ap); return r;
}
static inline char *_strlwr(char *s) { for (char *p = s; *p; ++p) *p = (char)tolower((unsigned char)*p); return s; }
static inline char *_strupr(char *s) { for (char *p = s; *p; ++p) *p = (char)toupper((unsigned char)*p); return s; }
#define strlwr _strlwr
#define strupr _strupr

static inline char *_itoa(int value, char *buf, int radix)
{
    if (radix == 10) { sprintf(buf, "%d", value); return buf; }
    if (radix == 16) { sprintf(buf, "%x", (unsigned)value); return buf; }
    char tmp[34]; int i = 0; unsigned v = (unsigned)value;
    do { int d = (int)(v % (unsigned)radix); tmp[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); v /= (unsigned)radix; } while (v);
    for (int j = 0; j < i; ++j) buf[j] = tmp[i - 1 - j];
    buf[i] = 0; return buf;
}
#define itoa _itoa

static inline int _vsnprintf_s(char *buf, size_t size, size_t count, const char *fmt, va_list ap)
{
    (void)count; int r = vsnprintf(buf, size, fmt, ap);
    return (r < 0 || (size_t)r >= size) ? -1 : r;
}
static inline int _snprintf_s(char *buf, size_t size, size_t count, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); int r = _vsnprintf_s(buf, size, count, fmt, ap); va_end(ap); return r;
}
static inline int sprintf_s(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); int r = vsnprintf(buf, size, fmt, ap); va_end(ap); return r;
}
static inline int vsprintf_s(char *buf, size_t size, const char *fmt, va_list ap) { return vsnprintf(buf, size, fmt, ap); }
static inline int strcpy_s(char *dst, size_t size, const char *src)
{
    if (!dst || !size) return 22; size_t n = strlen(src); if (n >= size) n = size - 1;
    memcpy(dst, src, n); dst[n] = 0; return 0;
}
static inline int strncpy_s(char *dst, size_t size, const char *src, size_t count)
{
    if (!dst || !size) return 22; size_t n = strnlen(src, count == _TRUNCATE ? size - 1 : count); if (n >= size) n = size - 1;
    memcpy(dst, src, n); dst[n] = 0; return 0;
}
static inline char *_ui64toa(unsigned long long v, char *buf, int radix)
{
    if (radix == 16) sprintf(buf, "%llx", v); else sprintf(buf, "%llu", v);
    return buf;
}
static inline char *_i64toa(long long v, char *buf, int radix)
{
    if (radix == 16) sprintf(buf, "%llx", (unsigned long long)v); else sprintf(buf, "%lld", v);
    return buf;
}
static inline int localtime_s(struct tm *out, const time_t *t) { return localtime_r(t, out) ? 0 : 22; }
static inline int gmtime_s(struct tm *out, const time_t *t) { return gmtime_r(t, out) ? 0 : 22; }
static inline int strcat_s(char *dst, size_t size, const char *src)
{
    size_t l = strnlen(dst, size); return l >= size ? 22 : strcpy_s(dst + l, size - l, src);
}
static inline int memcpy_s(void *dst, size_t dsize, const void *src, size_t count)
{
    if (count > dsize) return 34; memcpy(dst, src, count); return 0;
}
static inline int sscanf_s(const char *buf, const char *fmt, ...)
{
    // ATTENZIONE: sscanf_s vuole la dimensione dopo ogni %s/%c; il motore la usa solo con numeri.
    va_list ap; va_start(ap, fmt); int r = vsscanf(buf, fmt, ap); va_end(ap); return r;
}
static inline char *_ctime64(const long long *t) { time_t v = (time_t)*t; return ctime(&v); }
static inline int fopen_s(FILE **f, const char *name, const char *mode) { *f = fopen(name, mode); return *f ? 0 : errno; }
static inline char *strtok_s(char *s, const char *delim, char **ctx) { return strtok_r(s, delim, ctx); }

#ifdef __cplusplus
} // extern "C"
#ifndef __APPLE__
template <size_t N> static inline int strcpy_s(char (&dst)[N], const char *src) { return strcpy_s(dst, N, src); }
template <size_t N> static inline int strcat_s(char (&dst)[N], const char *src) { return strcat_s(dst, N, src); }
template <size_t N> static inline int strncpy_s(char (&dst)[N], const char *src, size_t count) { return strncpy_s(dst, N, src, count); }
template <size_t N> static inline int sprintf_s(char (&dst)[N], const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); int r = vsnprintf(dst, N, fmt, ap); va_end(ap); return r;
}
template <size_t N> static inline int _snwprintf_s(wchar_t (&dst)[N], size_t count, const wchar_t *fmt, ...)
{
    va_list ap; va_start(ap, fmt); int r = _vsnwprintf_s(dst, N, count, fmt, ap); va_end(ap); return r;
}
template <size_t N> static inline int _vsnwprintf_s(wchar_t (&dst)[N], size_t count, const wchar_t *fmt, va_list ap) { return _vsnwprintf_s(dst, N, count, fmt, ap); }
template <size_t N> static inline int _vsnprintf_s(char (&dst)[N], size_t count, const char *fmt, va_list ap) { return _vsnprintf_s(dst, N, count, fmt, ap); }
template <size_t N> static inline int _snprintf_s(char (&dst)[N], size_t count, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); int r = _vsnprintf_s(dst, N, count, fmt, ap); va_end(ap); return r;
}
#endif
#endif

// ------------------------------------------------------------------------------------------------
// Intrinseci di MSVC
// ------------------------------------------------------------------------------------------------
#define MemoryBarrier() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define _ReadWriteBarrier() __atomic_signal_fence(__ATOMIC_SEQ_CST)
#define _ReadBarrier() __atomic_signal_fence(__ATOMIC_SEQ_CST)
#define _WriteBarrier() __atomic_signal_fence(__ATOMIC_SEQ_CST)
#define YieldProcessor() bo1_cpu_relax()

static inline void bo1_cpu_relax(void)
{
#if defined(__i386__) || defined(__x86_64__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield");
#endif
}

// Le funzioni Interlocked* di Win32 lavorano su LONG con barriera completa. Il codice decompilato le chiama
// anche su `long`, `int`, `unsigned`: su Windows sono tutti 32 bit, su Linux/Android `long` può essere 64 bit.
// In C++ usiamo template, così ogni tipo intero funziona alla sua dimensione naturale.
#ifdef __cplusplus
// Valore per le Interlocked* "a 32 bit" su un `long`/`unsigned long` che qui � a 64 bit: su Windows LONG � a
// 32 bit, quindi una costante come 0xFFFFFFFF (il decompilato scrive cos� il -1) va estesa col segno, non con
// zeri, altrimenti _InterlockedExchangeAdd(&count, 0xFFFFFFFF) aggiunge 4 miliardi invece di togliere 1.
template <typename T, typename V> static inline T bo1_ilk_val(V v)
{
    if constexpr (sizeof(T) == 8 && sizeof(V) == 4 && __is_integral(V) &&
                  (__is_same(__remove_cv(T), long) || __is_same(__remove_cv(T), unsigned long)))
        return (T)(long long)(int)(unsigned int)v;
    else
        return (T)v;
}
template <typename T> static inline T InterlockedIncrement(volatile T *p) { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
template <typename T> static inline T InterlockedDecrement(volatile T *p) { return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST); }
template <typename T, typename V> static inline T InterlockedExchange(volatile T *p, V v) { return __atomic_exchange_n(p, bo1_ilk_val<T>(v), __ATOMIC_SEQ_CST); }
template <typename T, typename V> static inline T InterlockedExchangeAdd(volatile T *p, V v) { return __atomic_fetch_add(p, bo1_ilk_val<T>(v), __ATOMIC_SEQ_CST); }
template <typename T, typename V> static inline T InterlockedAnd(volatile T *p, V v) { return __atomic_fetch_and(p, bo1_ilk_val<T>(v), __ATOMIC_SEQ_CST); }
template <typename T, typename V> static inline T InterlockedOr(volatile T *p, V v) { return __atomic_fetch_or(p, bo1_ilk_val<T>(v), __ATOMIC_SEQ_CST); }
template <typename T, typename V> static inline T InterlockedXor(volatile T *p, V v) { return __atomic_fetch_xor(p, bo1_ilk_val<T>(v), __ATOMIC_SEQ_CST); }
template <typename T, typename V, typename C> static inline T InterlockedCompareExchange(volatile T *p, V xchg, C cmp)
{
    T expected = bo1_ilk_val<T>(cmp);
    __atomic_compare_exchange_n(p, &expected, bo1_ilk_val<T>(xchg), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}
template <typename T, typename V, typename C> static inline T InterlockedCompareExchange64(volatile T *p, V xchg, C cmp)
{
    T expected = (T)cmp;
    __atomic_compare_exchange_n(p, &expected, (T)xchg, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}
template <typename T, typename V> static inline T InterlockedExchangeAdd64(volatile T *p, V v) { return __atomic_fetch_add(p, (T)v, __ATOMIC_SEQ_CST); }
template <typename T, typename V> static inline T InterlockedExchange64(volatile T *p, V v) { return __atomic_exchange_n(p, (T)v, __ATOMIC_SEQ_CST); }
template <typename T> static inline T InterlockedIncrement64(volatile T *p) { return __atomic_add_fetch(p, 1, __ATOMIC_SEQ_CST); }
template <typename T> static inline T InterlockedDecrement64(volatile T *p) { return __atomic_sub_fetch(p, 1, __ATOMIC_SEQ_CST); }
#else
#define InterlockedIncrement(p) __atomic_add_fetch((p), 1, __ATOMIC_SEQ_CST)
#define InterlockedDecrement(p) __atomic_sub_fetch((p), 1, __ATOMIC_SEQ_CST)
#define InterlockedExchange(p, v) __atomic_exchange_n((p), (v), __ATOMIC_SEQ_CST)
#define InterlockedExchangeAdd(p, v) __atomic_fetch_add((p), (v), __ATOMIC_SEQ_CST)
#define InterlockedCompareExchange(p, x, c) __sync_val_compare_and_swap((p), (c), (x))
#endif
static inline void *InterlockedCompareExchangePointer(void *volatile *p, void *xchg, void *cmp)
{
    __atomic_compare_exchange_n(p, &cmp, xchg, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return cmp;
}
static inline void *InterlockedExchangePointer(void *volatile *p, void *v) { return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST); }
#define _InterlockedIncrement InterlockedIncrement
#define _InterlockedDecrement InterlockedDecrement
#define _InterlockedExchange InterlockedExchange
#define _InterlockedExchange64 InterlockedExchange64
#define _InterlockedExchangeAdd64 InterlockedExchangeAdd64
#define _InterlockedExchangeAdd InterlockedExchangeAdd
#define _InterlockedCompareExchange InterlockedCompareExchange
#define _InterlockedCompareExchange64 InterlockedCompareExchange64
#define _InterlockedAnd InterlockedAnd
#define _InterlockedOr InterlockedOr
#define _InterlockedXor InterlockedXor

// _BitScanForward/Reverse: il codice passa sia DWORD* sia unsigned long*; un template accetta entrambi.
#ifdef __cplusplus
template <typename I> static inline unsigned char bo1_BitScanForward(I *index, uint32_t mask)
{
    if (!mask) return 0; *index = (I)__builtin_ctz(mask); return 1;
}
template <typename I> static inline unsigned char bo1_BitScanReverse(I *index, uint32_t mask)
{
    if (!mask) return 0; *index = (I)(31 - __builtin_clz(mask)); return 1;
}
#define _BitScanForward bo1_BitScanForward
#define _BitScanReverse bo1_BitScanReverse
#endif
#if !__has_builtin(__popcnt)
static inline unsigned int __popcnt(unsigned int v) { return (unsigned int)__builtin_popcount(v); }
#endif
#if !__has_builtin(_rotl)
static inline unsigned int _rotl(unsigned int v, int s) { s &= 31; return s ? (v << s) | (v >> (32 - s)) : v; }
static inline unsigned int _rotr(unsigned int v, int s) { s &= 31; return s ? (v >> s) | (v << (32 - s)) : v; }
#endif
#if defined(__aarch64__)
// ARM: contatore virtuale del timer (leggibile in user space; PMCCNTR di __builtin_readcyclecounter da' SIGILL)
static inline uint64_t __rdtsc_compat(void) { uint64_t v; __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v)); return v; }
#define __rdtsc() __rdtsc_compat()
// __readfsdword(0x24) del profiler = id del thread corrente (TEB di Windows); qui il puntatore TLS del thread
static inline unsigned long __readfsdword(unsigned long) { return (unsigned long)(uintptr_t)__builtin_thread_pointer(); }
#elif defined(__arm__)
static inline uint64_t __rdtsc_compat(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec; }
#define __rdtsc() __rdtsc_compat()
static inline unsigned long __readfsdword(unsigned long) { return (unsigned long)(uintptr_t)__builtin_thread_pointer(); }
#else
static inline uint64_t __rdtsc_compat(void) { return __builtin_readcyclecounter(); }
#endif

// ------------------------------------------------------------------------------------------------
// Kernel32/User32: implementati in src/platform/win_compat.cpp
// ------------------------------------------------------------------------------------------------
typedef struct _CRITICAL_SECTION {
    // Spazio per un pthread_mutex_t ricorsivo (40 byte su glibc x86_64, 24 su i386, 40 su bionic arm64).
    unsigned char opaque[64] __attribute__((aligned(8)));
} CRITICAL_SECTION, *LPCRITICAL_SECTION;

// Gestione eccezioni SEH: esiste solo su Windows. I prototipi restano, i gestori non vengono mai chiamati.
typedef struct _EXCEPTION_RECORD { DWORD ExceptionCode; DWORD ExceptionFlags; struct _EXCEPTION_RECORD *ExceptionRecord; PVOID ExceptionAddress; DWORD NumberParameters; ULONG_PTR ExceptionInformation[15]; } EXCEPTION_RECORD, *PEXCEPTION_RECORD;
typedef struct _CONTEXT { DWORD ContextFlags; } CONTEXT, *PCONTEXT;
typedef struct _EXCEPTION_POINTERS { PEXCEPTION_RECORD ExceptionRecord; PCONTEXT ContextRecord; } EXCEPTION_POINTERS, *PEXCEPTION_POINTERS, *LPEXCEPTION_POINTERS;
#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_CONTINUE_EXECUTION (-1)

// SRWLOCK (lock lettori/scrittori leggero) = pthread_rwlock_t
typedef struct _RTL_SRWLOCK { void *opaque[8]; } SRWLOCK, *PSRWLOCK;
#define SRWLOCK_INIT { { 0 } }

typedef struct _MEMORY_BASIC_INFORMATION { PVOID BaseAddress, AllocationBase; DWORD AllocationProtect; SIZE_T RegionSize; DWORD State, Protect, Type; } MEMORY_BASIC_INFORMATION, *PMEMORY_BASIC_INFORMATION;

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

typedef struct _SECURITY_ATTRIBUTES { DWORD nLength; LPVOID lpSecurityDescriptor; BOOL bInheritHandle; } SECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;

#ifdef __cplusplus
extern "C" {
#endif

void InitializeCriticalSection(LPCRITICAL_SECTION cs);
BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION cs, DWORD spin);
void DeleteCriticalSection(LPCRITICAL_SECTION cs);
void EnterCriticalSection(LPCRITICAL_SECTION cs);
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION cs);
void LeaveCriticalSection(LPCRITICAL_SECTION cs);

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES sa, BOOL manualReset, BOOL initialState, LPCSTR name);
#define CreateEvent CreateEventA
BOOL SetEvent(HANDLE h);
BOOL ResetEvent(HANDLE h);
HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES sa, LONG initial, LONG maximum, LPCSTR name);
#define CreateSemaphore CreateSemaphoreA
BOOL ReleaseSemaphore(HANDLE h, LONG count, LONG *prev);
HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES sa, BOOL initialOwner, LPCSTR name);
#define CreateMutex CreateMutexA
BOOL ReleaseMutex(HANDLE h);
DWORD WaitForSingleObject(HANDLE h, DWORD ms);
DWORD WaitForMultipleObjects(DWORD count, const HANDLE *handles, BOOL waitAll, DWORD ms);
BOOL CloseHandle(HANDLE h);

HANDLE CreateThread(LPSECURITY_ATTRIBUTES sa, SIZE_T stack, LPTHREAD_START_ROUTINE fn, LPVOID arg, DWORD flags, LPDWORD id);
DWORD ResumeThread(HANDLE h);
DWORD SuspendThread(HANDLE h);
BOOL SetThreadPriority(HANDLE h, int priority);
int GetThreadPriority(HANDLE h);
DWORD_PTR SetThreadAffinityMask(HANDLE h, DWORD_PTR mask);
DWORD SetThreadIdealProcessor(HANDLE h, DWORD proc);
BOOL GetExitCodeThread(HANDLE h, LPDWORD code);
BOOL TerminateThread(HANDLE h, DWORD code);
HANDLE GetCurrentThread(void);
DWORD GetCurrentThreadId(void);
HANDLE GetCurrentProcess(void);
DWORD GetCurrentProcessId(void);
void Sleep(DWORD ms);
DWORD SleepEx(DWORD ms, BOOL alertable);
BOOL SwitchToThread(void);

DWORD TlsAlloc(void);
BOOL TlsFree(DWORD idx);
LPVOID TlsGetValue(DWORD idx);
BOOL TlsSetValue(DWORD idx, LPVOID value);

DWORD GetTickCount(void);
uint64_t GetTickCount64(void);
DWORD timeGetTime(void);
DWORD timeBeginPeriod(UINT p);
DWORD timeEndPeriod(UINT p);
BOOL QueryPerformanceCounter(LARGE_INTEGER *count);
BOOL QueryPerformanceFrequency(LARGE_INTEGER *freq);
void GetLocalTime(LPSYSTEMTIME st);
void GetSystemTime(LPSYSTEMTIME st);
void GetSystemTimeAsFileTime(FILETIME *ft);

LPVOID VirtualAlloc(LPVOID addr, SIZE_T size, DWORD type, DWORD protect);
BOOL VirtualFree(LPVOID addr, SIZE_T size, DWORD type);
BOOL VirtualProtect(LPVOID addr, SIZE_T size, DWORD newProtect, PDWORD oldProtect);

DWORD GetLastError(void);
void SetLastError(DWORD err);
void OutputDebugStringA(LPCSTR s);
#define OutputDebugString OutputDebugStringA
int MessageBoxA(HWND wnd, LPCSTR text, LPCSTR caption, UINT type);
#define MessageBox MessageBoxA
BOOL IsDebuggerPresent(void);
DWORD GetModuleFileNameA(HMODULE mod, LPSTR buf, DWORD size);
#define GetModuleFileName GetModuleFileNameA
HMODULE GetModuleHandleA(LPCSTR name);
#define GetModuleHandle GetModuleHandleA
DWORD GetCurrentDirectoryA(DWORD size, LPSTR buf);
#define GetCurrentDirectory GetCurrentDirectoryA
BOOL SetCurrentDirectoryA(LPCSTR path);
#define SetCurrentDirectory SetCurrentDirectoryA
BOOL CreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES sa);
#define CreateDirectory CreateDirectoryA
BOOL DeleteFileA(LPCSTR path);
#define DeleteFile DeleteFileA
BOOL MoveFileA(LPCSTR from, LPCSTR to);
#define MoveFile MoveFileA
DWORD GetFileAttributesA(LPCSTR path);
#define GetFileAttributes GetFileAttributesA
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_ATTRIBUTE_READONLY 0x1
DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buf, DWORD size);
#define GetEnvironmentVariable GetEnvironmentVariableA

typedef struct _SYSTEM_INFO {
    WORD wProcessorArchitecture; WORD wReserved; DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress; LPVOID lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask; DWORD dwNumberOfProcessors; DWORD dwProcessorType;
    DWORD dwAllocationGranularity; WORD wProcessorLevel; WORD wProcessorRevision;
} SYSTEM_INFO, *LPSYSTEM_INFO;
void GetSystemInfo(LPSYSTEM_INFO si);

typedef struct _MEMORYSTATUS {
    DWORD dwLength, dwMemoryLoad; SIZE_T dwTotalPhys, dwAvailPhys, dwTotalPageFile, dwAvailPageFile, dwTotalVirtual, dwAvailVirtual;
} MEMORYSTATUS, *LPMEMORYSTATUS;
void GlobalMemoryStatus(LPMEMORYSTATUS ms);
typedef struct _MEMORYSTATUSEX {
    DWORD dwLength, dwMemoryLoad; DWORDLONG ullTotalPhys, ullAvailPhys, ullTotalPageFile, ullAvailPageFile, ullTotalVirtual, ullAvailVirtual, ullAvailExtendedVirtual;
} MEMORYSTATUSEX, *LPMEMORYSTATUSEX;
BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX ms);
HANDLE OpenProcess(DWORD access, BOOL inherit, DWORD pid);
BOOL GetThreadTimes(HANDLE thread, FILETIME *creation, FILETIME *exit, FILETIME *kernel, FILETIME *user);
BOOL GetProcessTimes(HANDLE process, FILETIME *creation, FILETIME *exit, FILETIME *kernel, FILETIME *user);
BOOL GetSystemTimes(FILETIME *idle, FILETIME *kernel, FILETIME *user);
#define DUPLICATE_SAME_ACCESS 0x2
BOOL DuplicateHandle(HANDLE srcProcess, HANDLE src, HANDLE dstProcess, HANDLE *dst, DWORD access, BOOL inherit, DWORD options);
BOOL GetProcessAffinityMask(HANDLE process, DWORD_PTR *processMask, DWORD_PTR *systemMask);
#if defined(__cplusplus) && defined(__LP64__)
extern "C++" inline BOOL GetProcessAffinityMask(HANDLE process, DWORD *processMask, DWORD *systemMask)
{
    DWORD_PTR p = 0, sm = 0;
    BOOL r = GetProcessAffinityMask(process, &p, &sm);
    *processMask = (DWORD)p;
    *systemMask = (DWORD)sm;
    return r;
}
#endif
DWORD GetPriorityClass(HANDLE process);
BOOL SetPriorityClass(HANDLE process, DWORD cls);
#define NORMAL_PRIORITY_CLASS 0x20
#define HIGH_PRIORITY_CLASS 0x80
#define ABOVE_NORMAL_PRIORITY_CLASS 0x8000
LPSTR GetCommandLineA(void);
#define GetCommandLine GetCommandLineA
void bo1_SetCommandLine(int argc, char **argv);
void bo1_SetCurrentThreadName(const char *name);
void RaiseException(DWORD code, DWORD flags, DWORD nargs, const ULONG_PTR *args);
HRESULT CoInitializeEx(LPVOID reserved, DWORD coInit);
#define CoInitialize(r) CoInitializeEx((r), 0)
void CoUninitialize(void);
#define COINIT_MULTITHREADED 0x0
#define COINIT_APARTMENTTHREADED 0x2
int WideCharToMultiByte(UINT cp, DWORD flags, const wchar_t *src, int srcLen, LPSTR dst, int dstLen, LPCSTR defChar, BOOL *usedDef);
int MultiByteToWideChar(UINT cp, DWORD flags, LPCSTR src, int srcLen, wchar_t *dst, int dstLen);
#define CP_ACP 0
#define CP_UTF8 65001
FARPROC GetProcAddress(HMODULE mod, LPCSTR name);
HMODULE LoadLibraryA(LPCSTR name);
#define LoadLibrary LoadLibraryA
BOOL FreeLibrary(HMODULE mod);
int MessageBoxW(HWND wnd, const wchar_t *text, const wchar_t *caption, UINT type);
HINSTANCE ShellExecuteA(HWND wnd, LPCSTR op, LPCSTR file, LPCSTR params, LPCSTR dir, int show);
#define ShellExecute ShellExecuteA
HRESULT CLSIDFromString(const wchar_t *str, GUID *guid);
BOOL SetFileAttributesA(LPCSTR path, DWORD attrs);
#define SetFileAttributes SetFileAttributesA
HANDLE OpenThread(DWORD access, BOOL inherit, DWORD id);
#define THREAD_SUSPEND_RESUME 0x2
#define THREAD_GET_CONTEXT 0x8
#define THREAD_ALL_ACCESS 0x1fffff
typedef struct _PROCESS_MEMORY_COUNTERS {
    DWORD cb, PageFaultCount; SIZE_T PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage,
    QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage, PagefileUsage, PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS, *PPROCESS_MEMORY_COUNTERS;
typedef struct _PROCESS_MEMORY_COUNTERS_EX {
    DWORD cb, PageFaultCount; SIZE_T PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage,
    QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage, PagefileUsage, PeakPagefileUsage, PrivateUsage;
} PROCESS_MEMORY_COUNTERS_EX;
BOOL K32GetProcessMemoryInfo(HANDLE process, PROCESS_MEMORY_COUNTERS *pmc, DWORD cb);
#define GetProcessMemoryInfo K32GetProcessMemoryInfo
BOOL GetProcessHandleCount(HANDLE process, PDWORD count);
BOOL TerminateProcess(HANDLE process, UINT code);
#define THREAD_QUERY_INFORMATION 0x0040
#define THREAD_QUERY_LIMITED_INFORMATION 0x0800
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 0x2
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 0x4
static inline BOOL GetModuleHandleExA(DWORD, LPCSTR, HMODULE *mod) { if (mod) *mod = nullptr; return FALSE; }
#define GetModuleHandleEx GetModuleHandleExA
// Cicli di CPU di un thread/processo: usiamo i nanosecondi di CPU (solo profiler interni, rapporti relativi)
BOOL QueryThreadCycleTime(HANDLE thread, ULONG64 *cycles);
#if defined(__cplusplus) && defined(__LP64__)
// a 64 bit ULONG64/DWORD_PTR sono unsigned long, il codice decompilato passa unsigned long long / DWORD
extern "C++" inline BOOL QueryThreadCycleTime(HANDLE thread, unsigned long long *cycles)
{
    ULONG64 c = 0;
    BOOL r = QueryThreadCycleTime(thread, &c);
    *cycles = c;
    return r;
}
#endif
BOOL QueryProcessCycleTime(HANDLE process, ULONG64 *cycles);
#if defined(__cplusplus) && defined(__LP64__)
extern "C++" inline BOOL QueryProcessCycleTime(HANDLE process, unsigned long long *cycles)
{
    ULONG64 c = 0;
    BOOL r = QueryProcessCycleTime(process, &c);
    *cycles = c;
    return r;
}
#endif
#define SEM_FAILCRITICALERRORS 0x1
#define SEM_NOOPENFILEERRORBOX 0x8000
#define SEM_NOGPFAULTERRORBOX 0x2
static inline UINT SetErrorMode(UINT) { return 0; }
#define _OUT_TO_STDERR 1
#define _WRITE_ABORT_MSG 1
#define _CALL_REPORTFAULT 2
static inline int _set_error_mode(int) { return 0; }
static inline unsigned int _set_abort_behavior(unsigned int, unsigned int) { return 0; }
HBRUSH CreateSolidBrush(DWORD color);
// winmm: nessun dispositivo di registrazione/uscita "waveIn/waveOut" (la voce passa da SDL se mai servirà)
static inline UINT waveInGetNumDevs(void) { return 0; }
static inline UINT waveOutGetNumDevs(void) { return 0; }
static inline UINT mixerGetNumDevs(void) { return 0; }
#define MMSYSERR_NOERROR 0
#define FORMAT_MESSAGE_FROM_SYSTEM 0x1000
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x100
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x200
DWORD FormatMessageA(DWORD flags, LPCVOID source, DWORD msgId, DWORD langId, LPSTR buf, DWORD size, va_list *args);
#define FormatMessage FormatMessageA
LPVOID GlobalLock(HGLOBAL mem);
BOOL GlobalUnlock(HGLOBAL mem);
SIZE_T GlobalSize(HGLOBAL mem);
HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes);
HGLOBAL GlobalFree(HGLOBAL mem);
HANDLE LocalFree(HANDLE mem);
#define GMEM_MOVEABLE 0x2
typedef struct _OSVERSIONINFOA { DWORD dwOSVersionInfoSize, dwMajorVersion, dwMinorVersion, dwBuildNumber, dwPlatformId; CHAR szCSDVersion[128]; } OSVERSIONINFOA, *LPOSVERSIONINFOA;
typedef OSVERSIONINFOA OSVERSIONINFO;
BOOL GetVersionExA(LPOSVERSIONINFOA info);
#define GetVersionEx GetVersionExA
typedef LONG (*LPTOP_LEVEL_EXCEPTION_FILTER)(struct _EXCEPTION_POINTERS *);
LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter);
BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value);
#define SetEnvironmentVariable SetEnvironmentVariableA
BOOL RemoveDirectoryA(LPCSTR path);
#define RemoveDirectory RemoveDirectoryA
DWORD GetFullPathNameA(LPCSTR name, DWORD size, LPSTR buf, LPSTR *filePart);
#define GetFullPathName GetFullPathNameA
BOOL SystemTimeToFileTime(const SYSTEMTIME *st, FILETIME *ft);
LONG CompareFileTime(const FILETIME *a, const FILETIME *b);
// User32: finestra/cursore. Le versioni vere stanno nello strato SDL (platform/sdl); queste sono ripieghi "weak".
HWND GetActiveWindow(void);
HWND GetDesktopWindow(void);
HWND GetForegroundWindow(void);
int ShowCursor(BOOL show);
#define IDC_ARROW ((LPCSTR)32512)
#define IDC_WAIT ((LPCSTR)32514)
HCURSOR LoadCursorA(HINSTANCE inst, LPCSTR name);
#define LoadCursor LoadCursorA
HCURSOR SetCursor(HCURSOR cursor);
SIZE_T VirtualQuery(LPCVOID addr, PMEMORY_BASIC_INFORMATION info, SIZE_T len);

// SRWLOCK: implementati con un mutex (il motore li usa come lock semplici, mai ricorsivi)
void InitializeSRWLock(PSRWLOCK l);
void AcquireSRWLockExclusive(PSRWLOCK l);
void ReleaseSRWLockExclusive(PSRWLOCK l);
void AcquireSRWLockShared(PSRWLOCK l);
void ReleaseSRWLockShared(PSRWLOCK l);
BOOLEAN TryAcquireSRWLockExclusive(PSRWLOCK l);

#ifdef __cplusplus
} // extern "C"
#endif


// --- File (CreateFile/ReadFile/ReadFileEx con OVERLAPPED, usati dal loader dei fastfile) ---
typedef struct _OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union { struct { DWORD Offset; DWORD OffsetHigh; }; PVOID Pointer; };
    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;
typedef void (*LPOVERLAPPED_COMPLETION_ROUTINE)(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped);

#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x1
#define FILE_SHARE_WRITE 0x2
#define FILE_SHARE_DELETE 0x4
#define FILE_APPEND_DATA 0x4
#define FILE_ATTRIBUTE_HIDDEN 0x2
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define TRUNCATE_EXISTING 5
#define FILE_FLAG_OVERLAPPED 0x40000000
#define FILE_FLAG_NO_BUFFERING 0x20000000
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define INVALID_FILE_SIZE ((DWORD)0xFFFFFFFF)
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_PATH_NOT_FOUND 3L
#define ERROR_ACCESS_DENIED 5L
#define ERROR_HANDLE_EOF 38L
#define ERROR_NO_MORE_FILES 18L
#define ERROR_IO_PENDING 997L
#define WAIT_IO_COMPLETION 0xC0u

typedef struct _WIN32_FIND_DATAA {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow, dwReserved0, dwReserved1;
    CHAR cFileName[MAX_PATH];
    CHAR cAlternateFileName[14];
} WIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;
typedef WIN32_FIND_DATAA WIN32_FIND_DATA;

#ifdef __cplusplus
extern "C" {
#endif
HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disposition, DWORD flags, HANDLE templ);
#define CreateFile CreateFileA
BOOL ReadFile(HANDLE f, LPVOID buf, DWORD size, LPDWORD read, LPOVERLAPPED ov);
BOOL ReadFileEx(HANDLE f, LPVOID buf, DWORD size, LPOVERLAPPED ov, LPOVERLAPPED_COMPLETION_ROUTINE done);
BOOL WriteFile(HANDLE f, LPCVOID buf, DWORD size, LPDWORD written, LPOVERLAPPED ov);
BOOL GetOverlappedResult(HANDLE f, LPOVERLAPPED ov, LPDWORD transferred, BOOL wait);
BOOL CancelIo(HANDLE f);
DWORD GetFileSize(HANDLE f, LPDWORD high);
DWORD SetFilePointer(HANDLE f, LONG dist, LONG *distHigh, DWORD method);
BOOL FlushFileBuffers(HANDLE f);
BOOL SetEndOfFile(HANDLE f);
HANDLE FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data);
#define FindFirstFile FindFirstFileA
BOOL FindNextFileA(HANDLE h, LPWIN32_FIND_DATAA data);
#define FindNextFile FindNextFileA
BOOL FindClose(HANDLE h);
BOOL PulseEvent(HANDLE h);
DWORD WaitForSingleObjectEx(HANDLE h, DWORD ms, BOOL alertable);
__attribute__((noreturn)) void ExitProcess(UINT code);

// Converte un percorso Windows ("main\\file.iwd", "C:\\...") in uno POSIX e risolve maiuscole/minuscole:
// i file del gioco copiati da Windows non hanno sempre il case che il codice si aspetta.
const char *bo1_resolve_path(const char *winPath, char *out, size_t outSize);
#ifdef __cplusplus
}
#endif

// _findfirst/_findnext del CRT (usati da Sys_ListFiles): sopra FindFirstFileA
#define _A_NORMAL 0x00
#define _A_RDONLY 0x01
#define _A_HIDDEN 0x02
#define _A_SYSTEM 0x04
#define _A_SUBDIR 0x10
#define _A_ARCH 0x20
// Allineamento esplicito a 8 come MSVC: il motore è compilato con -malign-double e lo strato piattaforma no,
// senza questo i due vedrebbero "name" a offset diversi (nomi dei file troncati).
struct _finddata64i32_t { unsigned attrib; long long time_create __attribute__((aligned(8))), time_access __attribute__((aligned(8))), time_write __attribute__((aligned(8))); uint32_t size; char name[260]; };
typedef struct _finddata64i32_t _finddata_t;
#ifdef __cplusplus
extern "C" {
#endif
intptr_t _findfirst64i32(const char *spec, struct _finddata64i32_t *data);
int _findnext64i32(intptr_t handle, struct _finddata64i32_t *data);
int _findclose(intptr_t handle);
#ifdef __cplusplus
}
#endif
#define _findfirst _findfirst64i32
#define _findnext _findnext64i32

// setjmp/longjmp: il codice decompilato passa il buffer come int* (jmp_buf di MSVC x86 = int[16]).
#include <setjmp.h>
#define _setjmp(buf) sigsetjmp(*(sigjmp_buf *)(buf), 0)
#define _setjmp3(buf, n) sigsetjmp(*(sigjmp_buf *)(buf), 0)
#define longjmp(buf, v) siglongjmp(*(sigjmp_buf *)(buf), (v))

typedef long long __time64_t;
#ifdef __cplusplus
template <typename T> static inline long long _time64(T *t) { long long v = (long long)time(NULL); if (t) *t = (T)v; return v; }
static inline long long _time64(decltype(nullptr)) { return (long long)time(NULL); }
#endif
#ifdef __cplusplus
static inline struct tm *_localtime64(const long long *t) { static thread_local struct tm r; time_t v = (time_t)*t; return localtime_r(&v, &r); }
static inline struct tm *_gmtime64(const long long *t) { static thread_local struct tm r; time_t v = (time_t)*t; return gmtime_r(&v, &r); }
static inline long long _mktime64(struct tm *t) { return (long long)mktime(t); }
#endif

#include "user32_compat.h"

#endif // !_WIN32

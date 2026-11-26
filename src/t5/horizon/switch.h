// switch.h - T5Jool (Linux/Android): the few libnx primitives the portable parts of the Switch
// layer (src/nx/nx_wincompat.cpp, nx_winsock.cpp, nx_prof.cpp...) use, implemented on POSIX
// threads and clocks. Only what those files call: it is not a libnx replacement.
#pragma once
#ifndef T5_HORIZON_SWITCH_H
#define T5_HORIZON_SWITCH_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <stdlib.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef u32 Result;
typedef uintptr_t Handle; // T5Jool: a pthread_t fits (Handle is u32 on Horizon)

#define R_SUCCEEDED(r) ((r) == 0)
#define R_FAILED(r) ((r) != 0)
#define CUR_THREAD_HANDLE ((Handle)0xFFFF8000)
#define CUR_PROCESS_HANDLE ((Handle)0xFFFF8001)

// ---------------------------------------------------------------- clock: nanoseconds as "ticks"
static inline u64 armGetSystemTick(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000000000ull + (u64)ts.tv_nsec;
}
static inline u64 armGetSystemTickFreq(void) { return 1000000000ull; }
static inline u64 armNsToTicks(u64 ns) { return ns; }
static inline u64 armTicksToNs(u64 tick) { return tick; }

static inline void svcSleepThread(s64 ns)
{
    if (ns <= 0) { sched_yield(); return; }
    struct timespec ts = { (time_t)(ns / 1000000000ll), (long)(ns % 1000000000ll) };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
}

// ---------------------------------------------------------------- mutex / condvar
typedef pthread_mutex_t Mutex;
typedef pthread_cond_t CondVar;
static inline void mutexInit(Mutex *m) { pthread_mutex_init(m, NULL); }
static inline void mutexLock(Mutex *m) { pthread_mutex_lock(m); }
static inline void mutexUnlock(Mutex *m) { pthread_mutex_unlock(m); }
static inline bool mutexTryLock(Mutex *m) { return pthread_mutex_trylock(m) == 0; }

typedef struct { pthread_mutex_t m; } RMutex;
static inline void rmutexInit(RMutex *r)
{
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&r->m, &a);
    pthread_mutexattr_destroy(&a);
}
static inline void rmutexLock(RMutex *r) { pthread_mutex_lock(&r->m); }
static inline void rmutexUnlock(RMutex *r) { pthread_mutex_unlock(&r->m); }
static inline bool rmutexTryLock(RMutex *r) { return pthread_mutex_trylock(&r->m) == 0; }

static inline void condvarInit(CondVar *c)
{
    pthread_condattr_t a;
    pthread_condattr_init(&a);
    pthread_condattr_setclock(&a, CLOCK_MONOTONIC);
    pthread_cond_init(c, &a);
    pthread_condattr_destroy(&a);
}
static inline Result condvarWaitTimeout(CondVar *c, Mutex *m, u64 ns)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    u64 t = (u64)ts.tv_nsec + ns;
    ts.tv_sec += (time_t)(t / 1000000000ull);
    ts.tv_nsec = (long)(t % 1000000000ull);
    return pthread_cond_timedwait(c, m, &ts) == 0 ? 0 : 1;
}
static inline void condvarWait(CondVar *c, Mutex *m) { pthread_cond_wait(c, m); }
static inline void condvarWakeAll(CondVar *c) { pthread_cond_broadcast(c); }
static inline void condvarWakeOne(CondVar *c) { pthread_cond_signal(c); }

// ---------------------------------------------------------------- threads
typedef void (*ThreadFunc)(void *);
typedef struct Thread {
    Handle handle;      // the pthread_t
    ThreadFunc entry;
    void *arg;
    size_t stackSize;
    Mutex startLock;
    CondVar startCond;
    bool startGo;
} Thread;

static void *t5_thread_trampoline(void *p)
{
    Thread *t = (Thread *)p;
    mutexLock(&t->startLock);
    while (!t->startGo)
        condvarWait(&t->startCond, &t->startLock);
    mutexUnlock(&t->startLock);
    t->entry(t->arg);
    return NULL;
}

// Horizon threads are created suspended and started by threadStart: the pthread waits on startCond.
static inline Result threadCreate(Thread *t, ThreadFunc entry, void *arg, void *stack, size_t stackSize, int prio, int cpuid)
{
    (void)stack; (void)prio; (void)cpuid;
    memset(t, 0, sizeof(*t));
    t->entry = entry;
    t->arg = arg;
    mutexInit(&t->startLock);
    condvarInit(&t->startCond);
    pthread_attr_t a;
    pthread_attr_init(&a);
    if (stackSize)
        pthread_attr_setstacksize(&a, stackSize < 0x40000 ? 0x40000 : stackSize);
    pthread_t th;
    int rc = pthread_create(&th, &a, t5_thread_trampoline, t);
    pthread_attr_destroy(&a);
    if (rc)
        return 1;
    t->handle = (Handle)th;
    return 0;
}
static inline Result threadStart(Thread *t)
{
    mutexLock(&t->startLock);
    t->startGo = true;
    condvarWakeAll(&t->startCond);
    mutexUnlock(&t->startLock);
    return 0;
}
static inline Result threadWaitForExit(Thread *t) { return pthread_join((pthread_t)t->handle, NULL) ? 1 : 0; }
static inline Result threadClose(Thread *t) { (void)t; return 0; }
static inline Result threadPause(Thread *t) { (void)t; return 0; } // T5Jool: no thread suspension on POSIX
static inline void threadExit(void) { pthread_exit(NULL); }

static inline Result svcGetThreadId(u64 *id, Handle h)
{
#ifdef __linux__
    if (h == CUR_THREAD_HANDLE) { *id = (u64)syscall(SYS_gettid); return 0; }
#endif
    *id = (u64)h;
    return 0;
}
static inline Result svcSetThreadCoreMask(Handle, s32, u32) { return 0; }
static inline Result svcSetThreadPriority(Handle, u32) { return 0; }
// wait for a thread handle (pthread): only "already finished" can be told without joining, so report a timeout
static inline Result waitSingleHandle(Handle, u64) { return 1; }

// ---------------------------------------------------------------- process info
enum { InfoType_CoreMask = 0, InfoType_TotalMemorySize = 6 };
static inline Result svcGetInfo(u64 *out, u32 type, Handle, u64)
{
    if (type == InfoType_CoreMask) {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        if (n < 1) n = 1;
        if (n > 8) n = 8;
        *out = (1ull << n) - 1;
        return 0;
    }
    if (type == InfoType_TotalMemorySize) {
        long pages = sysconf(_SC_PHYS_PAGES), ps = sysconf(_SC_PAGESIZE);
        *out = (u64)pages * (u64)ps;
        return 0;
    }
    return 1;
}

static inline void randomGet(void *buf, size_t len)
{
    u8 *p = (u8 *)buf;
    while (len) {
        ssize_t n = getrandom(p, len, 0);
        if (n <= 0) break;
        p += n;
        len -= (size_t)n;
    }
}

// ---------------------------------------------------------------- memory permissions (guard pages)
typedef enum { Perm_None = 0, Perm_R = 1, Perm_W = 2, Perm_X = 4, Perm_Rw = 3, Perm_Rx = 5 } Permission;
typedef struct { u64 addr; u64 size; u32 type; u32 attr; u32 perm; } MemoryInfo;
typedef u32 u32_pageinfo;
static inline Result svcSetMemoryPermission(void *addr, u64 size, u32 perm)
{
    int p = (perm & Perm_R ? PROT_READ : 0) | (perm & Perm_W ? PROT_WRITE : 0) | (perm & Perm_X ? PROT_EXEC : 0);
    return mprotect(addr, size, p) == 0 ? 0 : 1;
}
static inline Result svcQueryMemory(MemoryInfo *info, u32 *pageInfo, u64 addr)
{
    memset(info, 0, sizeof(*info));
    info->addr = addr & ~0xFFFull;
    info->size = 0x1000;
    info->perm = Perm_Rw;
    if (pageInfo) *pageInfo = 0;
    return 0;
}

static inline Result svcOutputDebugString(const char *s, u64 len) { (void)s; (void)len; return 0; }

// ---------------------------------------------------------------- applet (always focused, handheld)
typedef enum { AppletFocusState_InFocus = 1, AppletFocusState_OutOfFocus = 2 } AppletFocusState;
typedef enum { AppletOperationMode_Handheld = 0, AppletOperationMode_Console = 1 } AppletOperationMode;
typedef enum { AppletType_Application = 0 } AppletType;
static inline AppletFocusState appletGetFocusState(void) { return AppletFocusState_InFocus; }
static inline AppletOperationMode appletGetOperationMode(void) { return AppletOperationMode_Handheld; }
static inline AppletType appletGetAppletType(void) { return AppletType_Application; }
static inline Result appletGetCurrentPerformanceConfiguration(u32 *c) { *c = 0; return 0; }

#endif

// ---------------------------------------------------------------- heap figures (Switch 2's libounce names)
#include <stdio.h>
static inline u64 t5_meminfo_kb(const char *key)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0;
    char line[256];
    u64 v = 0;
    size_t n = strlen(key);
    while (fgets(line, sizeof(line), f))
        if (!strncmp(line, key, n) && line[n] == ':') { v = strtoull(line + n + 1, NULL, 10); break; }
    fclose(f);
    return v;
}
static inline u64 heapGetTotalSize(void) { return t5_meminfo_kb("MemTotal") << 10; }
static inline u64 heapGetFreeSize(void) { return t5_meminfo_kb("MemAvailable") << 10; }

// the game window (SDL_Window*) for the D3D9 bridge's CreateDevice (t5_main.cpp)
void *nwindowGetDefault(void);

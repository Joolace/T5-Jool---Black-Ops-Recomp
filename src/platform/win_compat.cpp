// win_compat.cpp - implementazione POSIX (Linux e Android/bionic) delle funzioni Win32 dichiarate in win_compat.h.
#if !defined(_WIN32)

#include "../platform/bo1_lowmem.h"
#include "compat/win_compat.h"

#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <deque>
#include <functional>
#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

static_assert(sizeof(pthread_mutex_t) <= sizeof(CRITICAL_SECTION::opaque), "CRITICAL_SECTION troppo piccola per pthread_mutex_t");

// ------------------------------------------------------------------------------------------------
// Errori
// ------------------------------------------------------------------------------------------------
static thread_local DWORD t_lastError = 0;
extern "C" DWORD GetLastError(void) { return t_lastError; }
extern "C" void SetLastError(DWORD err) { t_lastError = err; }

// ------------------------------------------------------------------------------------------------
// Critical section = mutex ricorsivo
// ------------------------------------------------------------------------------------------------
// Su Windows una critical section azzerata (variabile globale mai inizializzata) funziona lo stesso, ed è
// sempre ricorsiva: il motore ci conta (Com_Error prima di Sys_InitializeCriticalSections). Qui un marcatore
// in fondo allo spazio opaco dice se il mutex è stato creato; se no lo creiamo al primo uso.
static_assert(sizeof(pthread_mutex_t) <= sizeof(CRITICAL_SECTION::opaque) - 8, "CRITICAL_SECTION troppo piccola");
static const uint32_t kCsMagic = 0xB01C5EC7u;
static uint32_t &CsMagic(LPCRITICAL_SECTION cs) { return *reinterpret_cast<uint32_t *>(cs->opaque + sizeof(cs->opaque) - 4); }
static void CsInit(LPCRITICAL_SECTION cs)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(reinterpret_cast<pthread_mutex_t *>(cs->opaque), &attr);
    pthread_mutexattr_destroy(&attr);
    __atomic_store_n(&CsMagic(cs), kCsMagic, __ATOMIC_RELEASE);
}
static pthread_mutex_t *CS(LPCRITICAL_SECTION cs)
{
    if (__atomic_load_n(&CsMagic(cs), __ATOMIC_ACQUIRE) != kCsMagic)
    {
        static pthread_mutex_t s_lazyLock = PTHREAD_MUTEX_INITIALIZER;
        pthread_mutex_lock(&s_lazyLock);
        if (CsMagic(cs) != kCsMagic) CsInit(cs);
        pthread_mutex_unlock(&s_lazyLock);
    }
    return reinterpret_cast<pthread_mutex_t *>(cs->opaque);
}

extern "C" void InitializeCriticalSection(LPCRITICAL_SECTION cs) { CsInit(cs); }
extern "C" BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION cs, DWORD) { InitializeCriticalSection(cs); return TRUE; }
extern "C" void DeleteCriticalSection(LPCRITICAL_SECTION cs) { pthread_mutex_destroy(CS(cs)); CsMagic(cs) = 0; }
extern "C" void EnterCriticalSection(LPCRITICAL_SECTION cs) { pthread_mutex_lock(CS(cs)); }
extern "C" BOOL TryEnterCriticalSection(LPCRITICAL_SECTION cs) { return pthread_mutex_trylock(CS(cs)) == 0; }
extern "C" void LeaveCriticalSection(LPCRITICAL_SECTION cs) { pthread_mutex_unlock(CS(cs)); }

// ------------------------------------------------------------------------------------------------
// Oggetti attendibili: eventi, semafori, mutex, thread. Tutti condividono un unico lock+condvar globale,
// così WaitForMultipleObjects resta semplice e corretto (il motore ne usa pochi, non è un collo di bottiglia).
// ------------------------------------------------------------------------------------------------
namespace
{
enum class ObjType { Event, Semaphore, Mutex, Thread, File, Find };

struct WaitObject
{
    ObjType type;
    bool manualReset = false;
    bool signaled = false;    // evento/thread terminato
    LONG count = 0;           // semaforo
    LONG maxCount = 0;
    pthread_t owner{};        // mutex
    int recursion = 0;
    // thread
    pthread_t thread{};
    LPTHREAD_START_ROUTINE fn = nullptr;
    LPVOID arg = nullptr;
    DWORD exitCode = 259; // STILL_ACTIVE
    bool suspended = false;
    DWORD tid = 0;
    // file
    int fd = -1;
    // ricerca file
    std::vector<std::string> *findNames = nullptr;
    size_t findIndex = 0;
    std::string findDir;
};

std::mutex g_waitLock;
std::condition_variable g_waitCond;

bool IsSignaled(WaitObject *o)
{
    switch (o->type)
    {
    case ObjType::Event:
    case ObjType::Thread: return o->signaled;
    case ObjType::Semaphore: return o->count > 0;
    case ObjType::Mutex: return o->recursion == 0 || pthread_equal(o->owner, pthread_self());
    case ObjType::File:
    case ObjType::Find: return true; // su Windows un file è "segnalato" quando non ha I/O in corso
    }
    return false;
}

void Consume(WaitObject *o)
{
    switch (o->type)
    {
    case ObjType::Event: if (!o->manualReset) o->signaled = false; break;
    case ObjType::Semaphore: --o->count; break;
    case ObjType::Mutex: o->owner = pthread_self(); ++o->recursion; break;
    case ObjType::Thread:
    case ObjType::File:
    case ObjType::Find: break;
    }
}

DWORD NextThreadId()
{
    static std::atomic<DWORD> next{ 1000 };
    return next++;
}
thread_local DWORD t_threadId = 0;
thread_local WaitObject *t_selfThread = nullptr;
} // namespace

extern "C" HANDLE CreateEventA(LPSECURITY_ATTRIBUTES, BOOL manualReset, BOOL initialState, LPCSTR)
{
    auto *o = new WaitObject{ ObjType::Event };
    o->manualReset = manualReset != 0;
    o->signaled = initialState != 0;
    return o;
}

extern "C" BOOL SetEvent(HANDLE h)
{
    if (!h) return FALSE;
    { std::lock_guard<std::mutex> l(g_waitLock); static_cast<WaitObject *>(h)->signaled = true; }
    g_waitCond.notify_all();
    return TRUE;
}

extern "C" BOOL ResetEvent(HANDLE h)
{
    if (!h) return FALSE;
    std::lock_guard<std::mutex> l(g_waitLock);
    static_cast<WaitObject *>(h)->signaled = false;
    return TRUE;
}

extern "C" HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES, LONG initial, LONG maximum, LPCSTR)
{
    auto *o = new WaitObject{ ObjType::Semaphore };
    o->count = initial;
    o->maxCount = maximum;
    return o;
}

extern "C" BOOL ReleaseSemaphore(HANDLE h, LONG count, LONG *prev)
{
    auto *o = static_cast<WaitObject *>(h);
    {
        std::lock_guard<std::mutex> l(g_waitLock);
        if (prev) *prev = o->count;
        if (o->count + count > o->maxCount) return FALSE;
        o->count += count;
    }
    g_waitCond.notify_all();
    return TRUE;
}

extern "C" HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES, BOOL initialOwner, LPCSTR)
{
    auto *o = new WaitObject{ ObjType::Mutex };
    if (initialOwner) { o->owner = pthread_self(); o->recursion = 1; }
    return o;
}

extern "C" BOOL ReleaseMutex(HANDLE h)
{
    auto *o = static_cast<WaitObject *>(h);
    {
        std::lock_guard<std::mutex> l(g_waitLock);
        if (o->recursion == 0 || !pthread_equal(o->owner, pthread_self())) return FALSE;
        --o->recursion;
    }
    g_waitCond.notify_all();
    return TRUE;
}

extern "C" DWORD WaitForMultipleObjects(DWORD count, const HANDLE *handles, BOOL waitAll, DWORD ms)
{
    std::unique_lock<std::mutex> l(g_waitLock);
    auto ready = [&](DWORD *which) -> bool {
        if (waitAll)
        {
            for (DWORD i = 0; i < count; ++i)
                if (!IsSignaled(static_cast<WaitObject *>(handles[i]))) return false;
            for (DWORD i = 0; i < count; ++i) Consume(static_cast<WaitObject *>(handles[i]));
            *which = 0;
            return true;
        }
        for (DWORD i = 0; i < count; ++i)
        {
            auto *o = static_cast<WaitObject *>(handles[i]);
            if (IsSignaled(o)) { Consume(o); *which = i; return true; }
        }
        return false;
    };

    DWORD which = 0;
    if (ms == INFINITE)
    {
        g_waitCond.wait(l, [&] { return ready(&which); });
        return WAIT_OBJECT_0 + which;
    }
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    if (g_waitCond.wait_until(l, deadline, [&] { return ready(&which); }))
        return WAIT_OBJECT_0 + which;
    return WAIT_TIMEOUT;
}

extern "C" DWORD WaitForSingleObject(HANDLE h, DWORD ms)
{
    if (!h) return WAIT_FAILED;
    return WaitForMultipleObjects(1, &h, TRUE, ms);
}

extern "C" BOOL CloseHandle(HANDLE h)
{
    if (!h || h == INVALID_HANDLE_VALUE) return FALSE;
    auto *o = static_cast<WaitObject *>(h);
    if (o->type == ObjType::File) { close(o->fd); delete o; return TRUE; }
    if (o->type == ObjType::Find) { delete o->findNames; delete o; return TRUE; }
    if (o->type == ObjType::Thread)
    {
        // Come su Windows: chiudere l'handle non ferma il thread. Lo stacchiamo e lasciamo l'oggetto vivo
        // finché il thread non termina (lo libera il thread stesso, vedi ThreadEntry).
        std::lock_guard<std::mutex> l(g_waitLock);
        if (!o->signaled) { pthread_detach(o->thread); o->fn = nullptr; o->arg = reinterpret_cast<LPVOID>(1); return TRUE; }
        pthread_join(o->thread, nullptr);
    }
    delete o;
    return TRUE;
}

// ------------------------------------------------------------------------------------------------
// Thread
// ------------------------------------------------------------------------------------------------
static void *ThreadEntry(void *p)
{
    auto *o = static_cast<WaitObject *>(p);
    t_selfThread = o;
    t_threadId = o->tid;
    {
        std::unique_lock<std::mutex> l(g_waitLock);
        g_waitCond.wait(l, [&] { return !o->suspended; });
    }
    DWORD code = o->fn(o->arg);
    bool orphan;
    {
        std::lock_guard<std::mutex> l(g_waitLock);
        o->exitCode = code;
        o->signaled = true;
        orphan = o->fn == nullptr && o->arg == reinterpret_cast<LPVOID>(1);
    }
    g_waitCond.notify_all();
    if (orphan) delete o;
    return nullptr;
}

extern "C" HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T stack, LPTHREAD_START_ROUTINE fn, LPVOID arg, DWORD flags, LPDWORD id)
{
    auto *o = new WaitObject{ ObjType::Thread };
    o->fn = fn;
    o->arg = arg;
    o->suspended = (flags & CREATE_SUSPENDED) != 0;
    o->tid = NextThreadId();

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // Il motore chiede stack piccoli pensati per Win32 a 32 bit: su 64 bit raddoppiamo e imponiamo un minimo.
    size_t size = stack ? stack * (sizeof(void *) / 4) : 0;
    if (size && size < 256 * 1024) size = 256 * 1024;
#ifdef BO1_LOWMEM
    // anche lo stack deve stare in memoria bassa: il motore converte indirizzi di variabili locali in int
    // Minimo 2 MB: a 64 bit i frame sono pi� grandi, e uno stack che sfora finiva (senza pagina di guardia) sopra
    // lo stack del thread vicino ("stack smashing detected" in un altro thread). Ora sotto ogni stack c'� una
    // zona di guardia non accessibile: chi sfora va in SIGSEGV nel punto esatto.
    if (size < 2 * 1024 * 1024) size = 2 * 1024 * 1024;
    size = (size + 0xFFFF) & ~static_cast<size_t>(0xFFFF);
    const size_t guard = 0x10000;
    char *stackRes = static_cast<char *>(bo1_low_reserve(size + guard)); // la guardia resta riservata (PROT_NONE)
    void *stackMem = stackRes ? stackRes + guard : nullptr;
    if (!stackMem || !bo1_low_commit(stackMem, size)) { pthread_attr_destroy(&attr); delete o; return nullptr; }
    pthread_attr_setstack(&attr, stackMem, size);
#else
    if (size) pthread_attr_setstacksize(&attr, size);
#endif
    int r = pthread_create(&o->thread, &attr, ThreadEntry, o);
    pthread_attr_destroy(&attr);
    if (r != 0) { delete o; return nullptr; }
    if (id) *id = o->tid;
    return o;
}

extern "C" DWORD ResumeThread(HANDLE h)
{
    auto *o = static_cast<WaitObject *>(h);
    DWORD prev;
    { std::lock_guard<std::mutex> l(g_waitLock); prev = o->suspended ? 1 : 0; o->suspended = false; }
    g_waitCond.notify_all();
    return prev;
}

// Sospendere un thread arbitrario non è possibile in modo sicuro con pthread; il motore lo usa solo all'avvio
// (thread creati CREATE_SUSPENDED), caso già gestito sopra.
extern "C" DWORD SuspendThread(HANDLE) { return 0; }

extern "C" BOOL SetThreadPriority(HANDLE, int) { return TRUE; }
extern "C" int GetThreadPriority(HANDLE) { return THREAD_PRIORITY_NORMAL; }
extern "C" DWORD_PTR SetThreadAffinityMask(HANDLE, DWORD_PTR mask) { return mask ? mask : 1; }
extern "C" DWORD SetThreadIdealProcessor(HANDLE, DWORD) { return 0; }

extern "C" BOOL GetExitCodeThread(HANDLE h, LPDWORD code)
{
    std::lock_guard<std::mutex> l(g_waitLock);
    *code = static_cast<WaitObject *>(h)->exitCode;
    return TRUE;
}

extern "C" BOOL TerminateThread(HANDLE, DWORD) { return FALSE; }

extern "C" HANDLE GetCurrentThread(void) { return reinterpret_cast<HANDLE>(static_cast<intptr_t>(-2)); }
extern "C" DWORD GetCurrentThreadId(void)
{
    if (!t_threadId) t_threadId = NextThreadId(); // thread principale o thread non creati da CreateThread
    return t_threadId;
}
extern "C" HANDLE GetCurrentProcess(void) { return reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)); }
extern "C" DWORD GetCurrentProcessId(void) { return static_cast<DWORD>(getpid()); }

extern "C" void Sleep(DWORD ms)
{
    if (ms == 0) { sched_yield(); return; }
    struct timespec ts = { static_cast<time_t>(ms / 1000), static_cast<long>((ms % 1000) * 1000000L) };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
}
// Coda APC per thread: le completion routine di ReadFileEx girano alla prossima attesa "alertable",
// esattamente come su Windows.
static thread_local std::deque<std::function<void()>> t_apcQueue;

static bool RunApcs()
{
    if (t_apcQueue.empty()) return false;
    while (!t_apcQueue.empty())
    {
        auto fn = std::move(t_apcQueue.front());
        t_apcQueue.pop_front();
        fn();
    }
    return true;
}

extern "C" DWORD SleepEx(DWORD ms, BOOL alertable)
{
    if (alertable && RunApcs()) return WAIT_IO_COMPLETION;
    // Le letture sono sincrone, quindi se non c'è nulla in coda un'attesa infinita sarebbe un blocco:
    // in quel caso ritorniamo subito come se un I/O fosse terminato.
    if (alertable && ms == INFINITE) return WAIT_IO_COMPLETION;
    Sleep(ms);
    return 0;
}

extern "C" DWORD WaitForSingleObjectEx(HANDLE h, DWORD ms, BOOL alertable)
{
    if (alertable && RunApcs()) return WAIT_IO_COMPLETION;
    return WaitForSingleObject(h, ms);
}

extern "C" BOOL PulseEvent(HANDLE h)
{
    // Sveglia chi sta già aspettando e poi riporta l'evento a non segnalato.
    SetEvent(h);
    std::this_thread::yield();
    return ResetEvent(h);
}

extern "C" void ExitProcess(UINT code) { fflush(stdout); fflush(stderr); _exit(static_cast<int>(code)); }
extern "C" BOOL SwitchToThread(void) { return sched_yield() == 0; }


// ------------------------------------------------------------------------------------------------
// Percorsi: "\\" -> "/", lettere di unità tolte, maiuscole/minuscole risolte componente per componente.
// ------------------------------------------------------------------------------------------------
static bool FindCaseInsensitive(const std::string &dir, const std::string &name, std::string &found)
{
    DIR *d = opendir(dir.empty() ? "." : dir.c_str());
    if (!d) return false;
    bool ok = false;
    while (struct dirent *e = readdir(d))
    {
        if (strcasecmp(e->d_name, name.c_str()) == 0) { found = e->d_name; ok = true; break; }
    }
    closedir(d);
    return ok;
}

extern "C" const char *bo1_resolve_path(const char *winPath, char *out, size_t outSize)
{
    std::string p(winPath ? winPath : "");
    for (char &c : p) if (c == '\\') c = '/';
    if (p.size() >= 2 && p[1] == ':' && isalpha(static_cast<unsigned char>(p[0]))) p.erase(0, 2); // "C:"
    // collassa "//"
    std::string clean;
    for (size_t i = 0; i < p.size(); ++i)
        if (!(p[i] == '/' && !clean.empty() && clean.back() == '/')) clean += p[i];

    struct stat st;
    if (stat(clean.c_str(), &st) == 0 || clean.empty())
    {
        snprintf(out, outSize, "%s", clean.c_str());
        return out;
    }

    // Risolvi ogni componente senza badare alle maiuscole; quelli che non esistono restano come sono
    // (serve per i file da creare).
    std::string result = clean[0] == '/' ? "/" : "";
    size_t pos = clean[0] == '/' ? 1 : 0;
    bool missing = false;
    while (pos <= clean.size())
    {
        size_t next = clean.find('/', pos);
        if (next == std::string::npos) next = clean.size();
        std::string comp = clean.substr(pos, next - pos);
        if (!comp.empty())
        {
            std::string dir = result.empty() ? "." : result;
            std::string candidate = result + comp;
            std::string found;
            if (!missing && stat(candidate.c_str(), &st) != 0 && FindCaseInsensitive(dir, comp, found))
                candidate = result + found;
            else if (stat(candidate.c_str(), &st) != 0)
                missing = true;
            result = candidate;
            if (next < clean.size()) result += '/';
        }
        pos = next + 1;
    }
    snprintf(out, outSize, "%s", result.c_str());
    return out;
}

// Generatore del CRT di Microsoft (stato per thread, seme iniziale 1): stessa sequenza del gioco su Windows
static thread_local uint32_t t_holdrand = 1;
extern "C" int bo1_rand(void)
{
    t_holdrand = t_holdrand * 214013u + 2531011u;
    return static_cast<int>((t_holdrand >> 16) & 0x7fff);
}
extern "C" void bo1_srand(unsigned int seed) { t_holdrand = seed; }

extern "C" FILE *bo1_fopen(const char *path, const char *mode)
{
    if (!path) { errno = EINVAL; return nullptr; }
    char resolved[1024];
    bo1_resolve_path(path, resolved, sizeof(resolved));
    return (fopen)(resolved, mode);
}
extern "C" int bo1_rename(const char *from, const char *to)
{
    char a[1024], b[1024];
    bo1_resolve_path(from, a, sizeof(a));
    bo1_resolve_path(to, b, sizeof(b));
    return (rename)(a, b);
}

// ------------------------------------------------------------------------------------------------
// File
// ------------------------------------------------------------------------------------------------
static WaitObject *FileObj(HANDLE h)
{
    auto *o = static_cast<WaitObject *>(h);
    return (o && h != INVALID_HANDLE_VALUE && o->type == ObjType::File) ? o : nullptr;
}

extern "C" HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD, LPSECURITY_ATTRIBUTES, DWORD disposition, DWORD, HANDLE)
{
    char path[1024];
    bo1_resolve_path(name, path, sizeof(path));
    int flags = 0;
    if ((access & GENERIC_READ) && (access & GENERIC_WRITE)) flags = O_RDWR;
    else if (access & GENERIC_WRITE) flags = O_WRONLY;
    else flags = O_RDONLY;
    switch (disposition)
    {
    case CREATE_NEW: flags |= O_CREAT | O_EXCL; break;
    case CREATE_ALWAYS: flags |= O_CREAT | O_TRUNC; break;
    case OPEN_ALWAYS: flags |= O_CREAT; break;
    case TRUNCATE_EXISTING: flags |= O_TRUNC; break;
    default: break;
    }
    int fd = open(path, flags | O_CLOEXEC, 0644);
    if (fd < 0)
    {
        SetLastError(errno == ENOENT ? ERROR_FILE_NOT_FOUND : (errno == EACCES ? ERROR_ACCESS_DENIED : ERROR_PATH_NOT_FOUND));
        return INVALID_HANDLE_VALUE;
    }
    auto *o = new WaitObject{ ObjType::File };
    o->fd = fd;
    return o;
}

static uint64_t OverlappedOffset(LPOVERLAPPED ov) { return (static_cast<uint64_t>(ov->OffsetHigh) << 32) | ov->Offset; }

extern "C" BOOL ReadFile(HANDLE h, LPVOID buf, DWORD size, LPDWORD read, LPOVERLAPPED ov)
{
    WaitObject *o = FileObj(h);
    if (!o) { SetLastError(6); return FALSE; }
    ssize_t n = ov ? pread(o->fd, buf, size, static_cast<off_t>(OverlappedOffset(ov))) : ::read(o->fd, buf, size);
    if (n < 0) { SetLastError(5); return FALSE; }
    if (read) *read = static_cast<DWORD>(n);
    if (ov) { ov->Internal = 0; ov->InternalHigh = static_cast<ULONG_PTR>(n); if (ov->hEvent) SetEvent(ov->hEvent); }
    return TRUE;
}

extern "C" BOOL ReadFileEx(HANDLE h, LPVOID buf, DWORD size, LPOVERLAPPED ov, LPOVERLAPPED_COMPLETION_ROUTINE done)
{
    WaitObject *o = FileObj(h);
    if (!o || !ov) { SetLastError(6); return FALSE; }
    uint64_t off = OverlappedOffset(ov);
    struct stat st;
    if (fstat(o->fd, &st) == 0 && off >= static_cast<uint64_t>(st.st_size)) { SetLastError(ERROR_HANDLE_EOF); return FALSE; }
    ssize_t n = pread(o->fd, buf, size, static_cast<off_t>(off));
    DWORD err = n < 0 ? 5u : 0u;
    DWORD got = n < 0 ? 0u : static_cast<DWORD>(n);
    ov->Internal = err;
    ov->InternalHigh = got;
    if (done) t_apcQueue.emplace_back([done, err, got, ov] { done(err, got, ov); });
    return TRUE;
}

extern "C" BOOL WriteFile(HANDLE h, LPCVOID buf, DWORD size, LPDWORD written, LPOVERLAPPED ov)
{
    WaitObject *o = FileObj(h);
    if (!o) { SetLastError(6); return FALSE; }
    ssize_t n = ov ? pwrite(o->fd, buf, size, static_cast<off_t>(OverlappedOffset(ov))) : ::write(o->fd, buf, size);
    if (n < 0) { SetLastError(5); return FALSE; }
    if (written) *written = static_cast<DWORD>(n);
    return TRUE;
}

extern "C" BOOL GetOverlappedResult(HANDLE, LPOVERLAPPED ov, LPDWORD transferred, BOOL)
{
    if (transferred) *transferred = static_cast<DWORD>(ov->InternalHigh);
    return ov->Internal == 0;
}

extern "C" BOOL CancelIo(HANDLE) { t_apcQueue.clear(); return TRUE; }

extern "C" DWORD GetFileSize(HANDLE h, LPDWORD high)
{
    WaitObject *o = FileObj(h);
    struct stat st;
    if (!o || fstat(o->fd, &st) != 0) return INVALID_FILE_SIZE;
    if (high) *high = static_cast<DWORD>(static_cast<uint64_t>(st.st_size) >> 32);
    return static_cast<DWORD>(st.st_size);
}

extern "C" DWORD SetFilePointer(HANDLE h, LONG dist, LONG *distHigh, DWORD method)
{
    WaitObject *o = FileObj(h);
    if (!o) return INVALID_SET_FILE_POINTER;
    int64_t off = distHigh ? (static_cast<int64_t>(*distHigh) << 32) | static_cast<uint32_t>(dist) : dist;
    int whence = method == FILE_CURRENT ? SEEK_CUR : (method == FILE_END ? SEEK_END : SEEK_SET);
    off_t r = lseek(o->fd, static_cast<off_t>(off), whence);
    if (r < 0) return INVALID_SET_FILE_POINTER;
    if (distHigh) *distHigh = static_cast<LONG>(static_cast<int64_t>(r) >> 32);
    return static_cast<DWORD>(r);
}

extern "C" BOOL FlushFileBuffers(HANDLE h) { WaitObject *o = FileObj(h); return o && fsync(o->fd) == 0; }
extern "C" BOOL SetEndOfFile(HANDLE h)
{
    WaitObject *o = FileObj(h);
    if (!o) return FALSE;
    off_t cur = lseek(o->fd, 0, SEEK_CUR);
    return ftruncate(o->fd, cur) == 0;
}

// ------------------------------------------------------------------------------------------------
// FindFirstFile/FindNextFile (pattern con * e ?, senza distinguere maiuscole/minuscole)
// ------------------------------------------------------------------------------------------------
static void FillFindData(WaitObject *o, LPWIN32_FIND_DATAA data)
{
    const std::string &name = (*o->findNames)[o->findIndex];
    memset(data, 0, sizeof(*data));
    snprintf(data->cFileName, sizeof(data->cFileName), "%s", name.c_str());
    struct stat st;
    std::string full = o->findDir + "/" + name;
    if (stat(full.c_str(), &st) == 0)
    {
        data->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        data->nFileSizeLow = static_cast<DWORD>(st.st_size);
        data->nFileSizeHigh = static_cast<DWORD>(static_cast<uint64_t>(st.st_size) >> 32);
    }
}

extern "C" HANDLE FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA data)
{
    char path[1024];
    bo1_resolve_path(pattern, path, sizeof(path));
    std::string full(path);
    size_t slash = full.rfind('/');
    std::string dir = slash == std::string::npos ? "." : full.substr(0, slash);
    std::string mask = slash == std::string::npos ? full : full.substr(slash + 1);
    char resolvedDir[1024];
    bo1_resolve_path(dir.c_str(), resolvedDir, sizeof(resolvedDir));

    DIR *d = opendir(resolvedDir[0] ? resolvedDir : ".");
    if (!d) { SetLastError(ERROR_PATH_NOT_FOUND); return INVALID_HANDLE_VALUE; }
    auto *names = new std::vector<std::string>();
    while (struct dirent *e = readdir(d))
        if (fnmatch(mask.c_str(), e->d_name, FNM_CASEFOLD) == 0) names->push_back(e->d_name);
    closedir(d);
    if (names->empty()) { delete names; SetLastError(ERROR_FILE_NOT_FOUND); return INVALID_HANDLE_VALUE; }

    auto *o = new WaitObject{ ObjType::Find };
    o->findNames = names;
    o->findIndex = 0;
    o->findDir = resolvedDir;
    FillFindData(o, data);
    return o;
}

extern "C" BOOL FindNextFileA(HANDLE h, LPWIN32_FIND_DATAA data)
{
    auto *o = static_cast<WaitObject *>(h);
    if (!o || o->type != ObjType::Find || o->findIndex + 1 >= o->findNames->size()) { SetLastError(ERROR_NO_MORE_FILES); return FALSE; }
    ++o->findIndex;
    FillFindData(o, data);
    return TRUE;
}

extern "C" BOOL FindClose(HANDLE h) { return CloseHandle(h); }

// ------------------------------------------------------------------------------------------------
// TLS
// ------------------------------------------------------------------------------------------------
extern "C" DWORD TlsAlloc(void)
{
    pthread_key_t key;
    if (pthread_key_create(&key, nullptr) != 0) return TLS_OUT_OF_INDEXES;
    return static_cast<DWORD>(key);
}
extern "C" BOOL TlsFree(DWORD idx) { return pthread_key_delete(static_cast<pthread_key_t>(idx)) == 0; }
extern "C" LPVOID TlsGetValue(DWORD idx) { return pthread_getspecific(static_cast<pthread_key_t>(idx)); }
extern "C" BOOL TlsSetValue(DWORD idx, LPVOID v) { return pthread_setspecific(static_cast<pthread_key_t>(idx), v) == 0; }

// ------------------------------------------------------------------------------------------------
// Tempo
// ------------------------------------------------------------------------------------------------
static uint64_t MonotonicNs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec);
}

extern "C" uint64_t GetTickCount64(void) { return MonotonicNs() / 1000000ull; }
extern "C" DWORD GetTickCount(void) { return static_cast<DWORD>(GetTickCount64()); }
extern "C" DWORD timeGetTime(void) { return GetTickCount(); }
extern "C" DWORD timeBeginPeriod(UINT) { return 0; }
extern "C" DWORD timeEndPeriod(UINT) { return 0; }
extern "C" BOOL QueryPerformanceCounter(LARGE_INTEGER *c) { c->QuadPart = static_cast<int64_t>(MonotonicNs()); return TRUE; }
extern "C" BOOL QueryPerformanceFrequency(LARGE_INTEGER *f) { f->QuadPart = 1000000000ll; return TRUE; }

static void FillSystemTime(LPSYSTEMTIME st, bool local)
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t t = tv.tv_sec;
    struct tm tmv;
    if (local) localtime_r(&t, &tmv); else gmtime_r(&t, &tmv);
    st->wYear = static_cast<WORD>(tmv.tm_year + 1900);
    st->wMonth = static_cast<WORD>(tmv.tm_mon + 1);
    st->wDayOfWeek = static_cast<WORD>(tmv.tm_wday);
    st->wDay = static_cast<WORD>(tmv.tm_mday);
    st->wHour = static_cast<WORD>(tmv.tm_hour);
    st->wMinute = static_cast<WORD>(tmv.tm_min);
    st->wSecond = static_cast<WORD>(tmv.tm_sec);
    st->wMilliseconds = static_cast<WORD>(tv.tv_usec / 1000);
}
extern "C" void GetLocalTime(LPSYSTEMTIME st) { FillSystemTime(st, true); }
extern "C" void GetSystemTime(LPSYSTEMTIME st) { FillSystemTime(st, false); }
extern "C" void GetSystemTimeAsFileTime(FILETIME *ft)
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    // FILETIME: intervalli di 100 ns dal 1601-01-01
    uint64_t v = (static_cast<uint64_t>(tv.tv_sec) + 11644473600ull) * 10000000ull + static_cast<uint64_t>(tv.tv_usec) * 10ull;
    ft->dwLowDateTime = static_cast<DWORD>(v);
    ft->dwHighDateTime = static_cast<DWORD>(v >> 32);
}

// ------------------------------------------------------------------------------------------------
// Memoria virtuale. MEM_RESERVE = mmap PROT_NONE, MEM_COMMIT = mprotect sulla regione.
// ------------------------------------------------------------------------------------------------
// Regioni riservate con lo stato di ogni pagina da 4 KB (riservata o "committed"), come le vede VirtualQuery
// su Windows: il motore controlla BaseAddress/AllocationBase/State pagina per pagina (com_memory.cpp).
static const uintptr_t kVmPage = 4096;
struct VmRegion { char *base; size_t size; std::vector<uint8_t> committed; };
static std::mutex g_vmLock;
static std::vector<VmRegion> g_vmRegions;
static void VmTrack(void *p, size_t size, bool committed)
{
    std::lock_guard<std::mutex> l(g_vmLock);
    g_vmRegions.push_back({ static_cast<char *>(p), size, std::vector<uint8_t>((size + kVmPage - 1) / kVmPage, committed ? 1 : 0) });
}
static size_t VmUntrack(void *p)
{
    std::lock_guard<std::mutex> l(g_vmLock);
    for (size_t i = 0; i < g_vmRegions.size(); ++i)
        if (g_vmRegions[i].base == p)
        {
            size_t s = g_vmRegions[i].size;
            g_vmRegions.erase(g_vmRegions.begin() + static_cast<long>(i));
            return s;
        }
    return 0;
}
static VmRegion *VmFind(const void *addr) // con g_vmLock preso
{
    for (auto &r : g_vmRegions)
        if (addr >= r.base && static_cast<const char *>(addr) < r.base + r.size) return &r;
    return nullptr;
}
static void VmSetCommitted(void *addr, size_t size, bool committed)
{
    std::lock_guard<std::mutex> l(g_vmLock);
    VmRegion *r = VmFind(addr);
    if (!r) return;
    size_t first = (static_cast<char *>(addr) - r->base) / kVmPage;
    size_t last = (static_cast<char *>(addr) + size - r->base + kVmPage - 1) / kVmPage;
    for (size_t i = first; i < last && i < r->committed.size(); ++i) r->committed[i] = committed ? 1 : 0;
}

static int ToProt(DWORD protect)
{
    switch (protect & 0xff)
    {
    case PAGE_NOACCESS: return PROT_NONE;
    case PAGE_READONLY: return PROT_READ;
    case PAGE_EXECUTE_READWRITE: return PROT_READ | PROT_WRITE | PROT_EXEC;
    default: return PROT_READ | PROT_WRITE;
    }
}

// T5Jool: buffer di soli dati (vertici, indici, immagini temporanee) che non servono sotto i 2 GB: li chiede
// Z_TryVirtualAlloc (universal/com_memory.cpp) e lasciano la memoria bassa ai fastfile delle mappe
static thread_local bool g_bo1VaHigh = false;
extern "C" void bo1_va_prefer_high(int on) { g_bo1VaHigh = on != 0; }

extern "C" LPVOID VirtualAlloc(LPVOID addr, SIZE_T size, DWORD type, DWORD protect)
{
    if (!size) return nullptr;
    if (addr && (type & MEM_COMMIT) && !(type & MEM_RESERVE))
    {
        // commit dentro una regione già riservata
        uintptr_t page = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
        uintptr_t start = reinterpret_cast<uintptr_t>(addr) & ~(page - 1);
        uintptr_t end = (reinterpret_cast<uintptr_t>(addr) + size + page - 1) & ~(page - 1);
        if (mprotect(reinterpret_cast<void *>(start), end - start, ToProt(protect)) != 0) return nullptr;
        VmSetCommitted(reinterpret_cast<void *>(start), end - start, true);
        return addr;
    }
    int prot = (type & MEM_COMMIT) ? ToProt(protect) : PROT_NONE;
#ifdef BO1_LOWMEM
    if (!addr && !g_bo1VaHigh)
    {
        // a 64 bit la memoria del motore deve stare sotto i 2 GB (bo1_lowmem.h)
        void *low = bo1_low_reserve(size);
        if (!low) return nullptr;
        if (size >= (2u << 20)) // T5Jool: chi usa la memoria bassa (nel log)
            fprintf(stderr, "bo1_lowmem: VirtualAlloc da %zu MB in basso a %p (chiamante %p)\n", (size_t)(size >> 20), low,
                    __builtin_return_address(0));
        if (prot != PROT_NONE && mprotect(low, size, prot) != 0) { bo1_low_unreserve(low, size); return nullptr; }
        VmTrack(low, size, prot != PROT_NONE);
        return low;
    }
#endif
    void *p = mmap(addr, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) return nullptr;
    if (addr && p != addr) { munmap(p, size); return nullptr; } // Win32 non sposta mai una richiesta a indirizzo fisso
    VmTrack(p, size, prot != PROT_NONE);
    return p;
}

extern "C" BOOL VirtualFree(LPVOID addr, SIZE_T size, DWORD type)
{
    if (type & MEM_DECOMMIT)
    {
        // restituisce le pagine al sistema ma tiene riservato l'intervallo
        madvise(addr, size, MADV_DONTNEED);
        VmSetCommitted(addr, size, false);
        return mprotect(addr, size, PROT_NONE) == 0;
    }
    // MEM_RELEASE: Win32 vuole size == 0 e libera tutta la regione: la dimensione la teniamo in VmTrack.
    size_t len = VmUntrack(addr);
    if (!len) len = size;
    if (!len) return FALSE;
#ifdef BO1_LOWMEM
    if (bo1_low_contains(addr)) { bo1_low_unreserve(addr, len); return TRUE; }
#endif
    return munmap(addr, len) == 0;
}

extern "C" BOOL VirtualProtect(LPVOID addr, SIZE_T size, DWORD newProtect, PDWORD oldProtect)
{
    if (oldProtect) *oldProtect = PAGE_READWRITE;
    return mprotect(addr, size, ToProt(newProtect)) == 0;
}

// ------------------------------------------------------------------------------------------------
// Varie
// ------------------------------------------------------------------------------------------------
// T5Jool: log della console (Android: stderr = <home>/logs/game.txt; Linux: il terminale / il file di t5jool.sh).
// Livello da T5_LOG (lo passa il lanciatore): 0 = solo errori, avvisi e righe T5Jool; 1 = normale (predefinito):
// righe ripetute raccolte e messaggi di rumore noti limitati; 2 = dettagliato: tutto, anche nel logcat di Android.
// Prima ogni riga era una scrittura su file non bufferizzata piu' una copia nel logcat: costava a ogni frame. Ora le
// righe si accumulano in un buffer scritto ogni secondo, subito per gli errori, e dal gestore dei crash.
namespace
{
char g_logBuf[64 * 1024];
size_t g_logLen;
pthread_mutex_t g_logMx = PTHREAD_MUTEX_INITIALIZER;
long long g_logLastFlushMs;
char g_logLast[256];
int g_logRepeat;
struct LogSpam { const char *pattern; int count; };
LogSpam g_logSpam[] = {
    { "is missing TECHNIQUE_", 0 }, { "SVF_NOCLIENT, but has children", 0 }, { "Redundant asset", 0 },
    { "BO1_FAKEENTS peak", 0 }, { "couldn't find path to goal", 0 }, { "Adjusted frameEnd", 0 },
    { "BO1_PIC2D", 0 }, { "BO1_DB64", 0 }, { "Circular event buffer overflow", 0 }, { "] EV_", 0 },
    { "Events are:", 0 }, { "did not move for 1 second", 0 }, { "bo1_netring peak", 0 }, { "BO1_MUSIC", 0 },
    { "LOADOUT ISSUE", 0 }, { "TL Warning: gjk_ray_cast", 0 }, { "Field ", 0 }, { "BO1_ICON", 0 },
};

long long LogNowMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int LogLevel()
{
    static int level = -1;
    if (level < 0)
    {
        const char *e = getenv("T5_LOG");
        level = e && *e ? atoi(e) : 1;
    }
    return level;
}

bool LogImportant(const char *s)
{
    return strstr(s, "rror") || strstr(s, "RROR") || strstr(s, "WARNING") || strstr(s, "Warning:") || strstr(s, "T5Jool")
        || strstr(s, "CRASH") || strstr(s, "ASSERT") || strstr(s, "quitting") || strstr(s, "signal");
}

void LogWriteAll(const char *p, size_t n)
{
    while (n > 0)
    {
        ssize_t w = write(STDERR_FILENO, p, n);
        if (w <= 0)
            return;
        p += w;
        n -= (size_t)w;
    }
}

void LogAppendLocked(const char *s, size_t n)
{
    if (g_logLen + n > sizeof(g_logBuf))
    {
        LogWriteAll(g_logBuf, g_logLen);
        g_logLen = 0;
    }
    if (n > sizeof(g_logBuf))
        LogWriteAll(s, n);
    else
    {
        memcpy(g_logBuf + g_logLen, s, n);
        g_logLen += n;
    }
}
} // namespace

// scrive il buffer (fine frame, uscita); _crash: dal gestore dei segnali, senza lucchetto
extern "C" void bo1_log_flush(void)
{
    pthread_mutex_lock(&g_logMx);
    LogWriteAll(g_logBuf, g_logLen);
    g_logLen = 0;
    pthread_mutex_unlock(&g_logMx);
}
extern "C" void bo1_log_flush_crash(void)
{
    LogWriteAll(g_logBuf, g_logLen);
    g_logLen = 0;
}

extern "C" void OutputDebugStringA(LPCSTR s)
{
    if (!s || !*s)
        return;
    const int level = LogLevel();
    const bool important = LogImportant(s);
    if (level <= 0 && !important)
        return;
#ifdef __ANDROID__
    if (level >= 2 || (important && strstr(s, "rror")))
        __android_log_write(ANDROID_LOG_DEBUG, "BO1", s);
#endif
    pthread_mutex_lock(&g_logMx);
    if (level == 1 && !important)
    {
        // righe identiche di fila: una sola, poi "(ripetuta N volte)"
        if (!strncmp(s, g_logLast, sizeof(g_logLast) - 1))
        {
            ++g_logRepeat;
            pthread_mutex_unlock(&g_logMx);
            return;
        }
        // rumore noto: le prime 5, poi una ogni 500
        for (LogSpam &sp : g_logSpam)
            if (strstr(s, sp.pattern))
            {
                const int c = ++sp.count;
                if (c > 5 && c % 500)
                {
                    pthread_mutex_unlock(&g_logMx);
                    return;
                }
                if (c > 5)
                {
                    char note[160];
                    int n = snprintf(note, sizeof(note), "(log: \"%s\" seen %d times)\n", sp.pattern, c);
                    LogAppendLocked(note, (size_t)n);
                    pthread_mutex_unlock(&g_logMx);
                    return;
                }
                break;
            }
    }
    if (g_logRepeat > 0)
    {
        char note[64];
        int n = snprintf(note, sizeof(note), "(previous line repeated %d times)\n", g_logRepeat);
        LogAppendLocked(note, (size_t)n);
        g_logRepeat = 0;
    }
    strncpy(g_logLast, s, sizeof(g_logLast) - 1);
    LogAppendLocked(s, strlen(s));
    const long long now = LogNowMs();
    if (important || now - g_logLastFlushMs > 1000)
    {
        LogWriteAll(g_logBuf, g_logLen);
        g_logLen = 0;
        g_logLastFlushMs = now;
    }
    pthread_mutex_unlock(&g_logMx);
}

// Il vero MessageBox passa da SDL (platform/sdl/sdl_main.cpp) quando la finestra esiste; questo è il ripiego.
extern "C" __attribute__((weak)) int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type)
{
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_ERROR, "BO1", "%s: %s", caption ? caption : "", text ? text : "");
#else
    fprintf(stderr, "[%s] %s\n", caption ? caption : "", text ? text : "");
#endif
    return (type & MB_YESNO) ? IDNO : IDOK;
}

extern "C" BOOL IsDebuggerPresent(void) { return FALSE; }

extern "C" DWORD GetModuleFileNameA(HMODULE, LPSTR buf, DWORD size)
{
    if (!size) return 0;
    ssize_t n = readlink("/proc/self/exe", buf, size - 1);
    if (n < 0) n = 0;
    buf[n] = 0;
    return static_cast<DWORD>(n);
}
extern "C" HMODULE GetModuleHandleA(LPCSTR) { return reinterpret_cast<HMODULE>(static_cast<intptr_t>(0x400000)); }

extern "C" DWORD GetCurrentDirectoryA(DWORD size, LPSTR buf) { return getcwd(buf, size) ? static_cast<DWORD>(strlen(buf)) : 0; }
// Tutte le funzioni con un percorso passano da bo1_resolve_path (barre rovesciate, maiuscole diverse dal disco)
extern "C" BOOL SetCurrentDirectoryA(LPCSTR path)
{
    char p[1024];
    return chdir(bo1_resolve_path(path, p, sizeof(p))) == 0;
}
extern "C" BOOL CreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES)
{
    char p[1024];
    if (mkdir(bo1_resolve_path(path, p, sizeof(p)), 0755) == 0) return TRUE;
    SetLastError(errno == EEXIST ? ERROR_ALREADY_EXISTS : 3);
    return FALSE;
}
extern "C" BOOL DeleteFileA(LPCSTR path)
{
    char p[1024];
    return unlink(bo1_resolve_path(path, p, sizeof(p))) == 0;
}
extern "C" BOOL MoveFileA(LPCSTR from, LPCSTR to) { return bo1_rename(from, to) == 0; }
extern "C" DWORD GetFileAttributesA(LPCSTR path)
{
    char p[1024];
    struct stat st;
    if (stat(bo1_resolve_path(path, p, sizeof(p)), &st) != 0) return INVALID_FILE_ATTRIBUTES;
    return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}
extern "C" int bo1_mkdir(const char *path)
{
    char p[1024];
    return mkdir(bo1_resolve_path(path, p, sizeof(p)), 0755);
}
extern "C" int bo1_access(const char *path, int mode)
{
    char p[1024];
    return access(bo1_resolve_path(path, p, sizeof(p)), mode);
}
extern "C" DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buf, DWORD size)
{
    const char *v = getenv(name);
    if (!v) return 0;
    size_t n = strlen(v);
    if (n + 1 > size) return static_cast<DWORD>(n + 1);
    memcpy(buf, v, n + 1);
    return static_cast<DWORD>(n);
}

extern "C" void GetSystemInfo(LPSYSTEM_INFO si)
{
    memset(si, 0, sizeof(*si));
    si->dwPageSize = static_cast<DWORD>(sysconf(_SC_PAGESIZE));
    si->dwAllocationGranularity = 65536;
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    si->dwNumberOfProcessors = static_cast<DWORD>(n > 0 ? n : 1);
    si->dwActiveProcessorMask = si->dwNumberOfProcessors >= sizeof(DWORD_PTR) * 8 ? ~static_cast<DWORD_PTR>(0)
                                                                                  : ((static_cast<DWORD_PTR>(1) << si->dwNumberOfProcessors) - 1);
}

extern "C" void GlobalMemoryStatus(LPMEMORYSTATUS ms)
{
    memset(ms, 0, sizeof(*ms));
    ms->dwLength = sizeof(*ms);
    uint64_t total = static_cast<uint64_t>(sysconf(_SC_PHYS_PAGES)) * static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    uint64_t avail = static_cast<uint64_t>(sysconf(_SC_AVPHYS_PAGES)) * static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    // Il motore è nato a 32 bit: oltre 2 GB i suoi calcoli su SIZE_T a 32 bit vanno in overflow.
    const uint64_t cap = sizeof(SIZE_T) == 4 ? 0x7fffffffull : total;
    ms->dwTotalPhys = static_cast<SIZE_T>(total < cap ? total : cap);
    ms->dwAvailPhys = static_cast<SIZE_T>(avail < cap ? avail : cap);
    ms->dwTotalVirtual = ms->dwTotalPhys;
    ms->dwAvailVirtual = ms->dwAvailPhys;
}

#endif // !_WIN32

#if !defined(_WIN32)
// ------------------------------------------------------------------------------------------------
// SRWLOCK = pthread_rwlock_t (tutto a zero = PTHREAD_RWLOCK_INITIALIZER su glibc e bionic, come SRWLOCK_INIT)
// ------------------------------------------------------------------------------------------------
static_assert(sizeof(pthread_rwlock_t) <= sizeof(SRWLOCK), "SRWLOCK troppo piccolo per pthread_rwlock_t");
static pthread_rwlock_t *RW(PSRWLOCK l) { return reinterpret_cast<pthread_rwlock_t *>(l); }
extern "C" void InitializeSRWLock(PSRWLOCK l) { pthread_rwlock_init(RW(l), nullptr); }
extern "C" void AcquireSRWLockExclusive(PSRWLOCK l) { pthread_rwlock_wrlock(RW(l)); }
extern "C" void ReleaseSRWLockExclusive(PSRWLOCK l) { pthread_rwlock_unlock(RW(l)); }
extern "C" void AcquireSRWLockShared(PSRWLOCK l) { pthread_rwlock_rdlock(RW(l)); }
extern "C" void ReleaseSRWLockShared(PSRWLOCK l) { pthread_rwlock_unlock(RW(l)); }
extern "C" BOOLEAN TryAcquireSRWLockExclusive(PSRWLOCK l) { return pthread_rwlock_trywrlock(RW(l)) == 0; }

// VirtualQuery: il motore la usa solo per sapere se un indirizzo è valido e quanto è grande la regione.
extern "C" SIZE_T VirtualQuery(LPCVOID addr, PMEMORY_BASIC_INFORMATION info, SIZE_T)
{
    // la dimensione passata dal motore è quella a 32 bit (0x1C): la ignoriamo
    if (!info) return 0;
    memset(info, 0, sizeof(*info));
    char *page = reinterpret_cast<char *>(reinterpret_cast<uintptr_t>(addr) & ~(kVmPage - 1));
    std::lock_guard<std::mutex> l(g_vmLock);
    if (VmRegion *r = VmFind(addr))
    {
        size_t idx = static_cast<size_t>(page - r->base) / kVmPage;
        uint8_t state = r->committed[idx];
        size_t end = idx;
        while (end < r->committed.size() && r->committed[end] == state) ++end;
        info->BaseAddress = page;
        info->AllocationBase = r->base;
        info->AllocationProtect = PAGE_READWRITE;
        info->RegionSize = (end - idx) * kVmPage;
        info->State = state ? MEM_COMMIT : MEM_RESERVE;
        info->Protect = state ? PAGE_READWRITE : PAGE_NOACCESS;
        info->Type = 0x20000; // MEM_PRIVATE
        return sizeof(*info);
    }
    // memoria non nostra (stack, heap di libc): una pagina "committed" senza regione di appartenenza
    info->BaseAddress = page;
    info->AllocationBase = nullptr;
    info->State = MEM_COMMIT;
    info->Protect = PAGE_READWRITE;
    info->RegionSize = kVmPage;
    return sizeof(*info);
}
#endif

#if !defined(_WIN32)
#include <dlfcn.h>
#include <platform/compat/ShlObj.h>

// ------------------------------------------------------------------------------------------------
// Processo, riga di comando, nomi dei thread
// ------------------------------------------------------------------------------------------------
extern "C" BOOL DuplicateHandle(HANDLE, HANDLE src, HANDLE, HANDLE *dst, DWORD, BOOL, DWORD)
{
    // Usato solo per ottenere un handle "vero" del thread principale: un oggetto Thread mai segnalato.
    if (src == GetCurrentThread())
    {
        auto *o = new WaitObject{ ObjType::Thread };
        o->thread = pthread_self();
        o->tid = GetCurrentThreadId();
        o->fn = nullptr;
        o->arg = reinterpret_cast<LPVOID>(1); // CloseHandle non farà join
        *dst = o;
        return TRUE;
    }
    *dst = src;
    return TRUE;
}

extern "C" BOOL GetProcessAffinityMask(HANDLE, DWORD_PTR *processMask, DWORD_PTR *systemMask)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    // Il motore conta i core con una maschera a 32 bit: oltre 32 core la limitiamo.
    DWORD_PTR m = si.dwActiveProcessorMask & 0xffffffffu;
    if (processMask) *processMask = m;
    if (systemMask) *systemMask = m;
    return TRUE;
}
extern "C" DWORD GetPriorityClass(HANDLE) { return NORMAL_PRIORITY_CLASS; }
extern "C" BOOL SetPriorityClass(HANDLE, DWORD) { return TRUE; }

static std::string g_commandLine;
extern "C" void bo1_SetCommandLine(int argc, char **argv)
{
    g_commandLine.clear();
    for (int i = 0; i < argc; ++i)
    {
        if (i) g_commandLine += ' ';
        bool quote = strchr(argv[i], ' ') != nullptr;
        if (quote) g_commandLine += '"';
        g_commandLine += argv[i];
        if (quote) g_commandLine += '"';
    }
}
extern "C" LPSTR GetCommandLineA(void) { return const_cast<LPSTR>(g_commandLine.c_str()); }

extern "C" void bo1_SetCurrentThreadName(const char *name)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%s", name ? name : "");
    pthread_setname_np(pthread_self(), buf);
}

extern "C" void RaiseException(DWORD code, DWORD, DWORD, const ULONG_PTR *)
{
    fprintf(stderr, "RaiseException(0x%08x)\n", code);
    abort();
}

extern "C" HRESULT CoInitializeEx(LPVOID, DWORD) { return S_OK; }
extern "C" void CoUninitialize(void) {}

// ------------------------------------------------------------------------------------------------
// Testo: wchar_t è 32 bit su Linux/Android (16 su Windows); convertiamo da/verso UTF-8.
// ------------------------------------------------------------------------------------------------
extern "C" int WideCharToMultiByte(UINT, DWORD, const wchar_t *src, int srcLen, LPSTR dst, int dstLen, LPCSTR, BOOL *usedDef)
{
    if (usedDef) *usedDef = FALSE;
    if (srcLen < 0) srcLen = static_cast<int>(wcslen(src)) + 1;
    std::string out;
    for (int i = 0; i < srcLen; ++i)
    {
        uint32_t c = static_cast<uint32_t>(src[i]);
        if (c < 0x80) out += static_cast<char>(c);
        else if (c < 0x800) { out += static_cast<char>(0xC0 | (c >> 6)); out += static_cast<char>(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { out += static_cast<char>(0xE0 | (c >> 12)); out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)); out += static_cast<char>(0x80 | (c & 0x3F)); }
        else { out += static_cast<char>(0xF0 | (c >> 18)); out += static_cast<char>(0x80 | ((c >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)); out += static_cast<char>(0x80 | (c & 0x3F)); }
    }
    if (!dst || !dstLen) return static_cast<int>(out.size());
    int n = static_cast<int>(out.size()) < dstLen ? static_cast<int>(out.size()) : dstLen;
    memcpy(dst, out.data(), static_cast<size_t>(n));
    return n;
}

extern "C" int MultiByteToWideChar(UINT, DWORD, LPCSTR src, int srcLen, wchar_t *dst, int dstLen)
{
    if (srcLen < 0) srcLen = static_cast<int>(strlen(src)) + 1;
    std::wstring out;
    for (int i = 0; i < srcLen;)
    {
        unsigned char c = static_cast<unsigned char>(src[i]);
        uint32_t cp; int extra;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c >> 5) == 6) { cp = c & 0x1F; extra = 1; }
        else if ((c >> 4) == 14) { cp = c & 0x0F; extra = 2; }
        else { cp = c & 0x07; extra = 3; }
        ++i;
        for (int k = 0; k < extra && i < srcLen; ++k, ++i) cp = (cp << 6) | (static_cast<unsigned char>(src[i]) & 0x3F);
        out += static_cast<wchar_t>(cp);
    }
    if (!dst || !dstLen) return static_cast<int>(out.size());
    int n = static_cast<int>(out.size()) < dstLen ? static_cast<int>(out.size()) : dstLen;
    memcpy(dst, out.data(), static_cast<size_t>(n) * sizeof(wchar_t));
    return n;
}

// ------------------------------------------------------------------------------------------------
// DLL -> librerie condivise (il motore cerca solo funzioni opzionali, es. in kernel32/ntdll)
// ------------------------------------------------------------------------------------------------
extern "C" HMODULE LoadLibraryA(LPCSTR) { return nullptr; }
// GetModuleHandleA restituisce un valore finto (non è una libreria vera): niente dlsym su quello, che fa crashare.
// Le funzioni "opzionali" che il motore cerca per nome le diamo da questa tabella; per le altre nullptr, e il
// motore usa la sua strada alternativa (come su un Windows vecchio senza quella funzione).
extern "C" FARPROC GetProcAddress(HMODULE, LPCSTR name)
{
    if (!name) return nullptr;
    struct Entry { const char *name; FARPROC fn; };
    static const Entry table[] = {
        { "GlobalMemoryStatusEx", reinterpret_cast<FARPROC>(&GlobalMemoryStatusEx) },
    };
    for (const Entry &e : table)
        if (!strcmp(e.name, name)) return e.fn;
    return nullptr;
}
extern "C" BOOL FreeLibrary(HMODULE) { return TRUE; }

// ------------------------------------------------------------------------------------------------
// Cartelle utente: su Linux $XDG_DATA_HOME (o ~/.local/share), su Android la cartella dati dell'app
// (impostata dal lanciatore in BO1_DATA_DIR).
// ------------------------------------------------------------------------------------------------
extern "C" HRESULT SHGetFolderPathA(HWND, int, HANDLE, DWORD, LPSTR path)
{
    const char *base = getenv("BO1_DATA_DIR");
    std::string dir;
    if (base && *base) dir = base;
    else if (const char *xdg = getenv("XDG_DATA_HOME")) dir = xdg;
    else if (const char *home = getenv("HOME")) dir = std::string(home) + "/.local/share";
    else dir = ".";
    mkdir(dir.c_str(), 0755);
    snprintf(path, MAX_PATH, "%s", dir.c_str());
    return S_OK;
}
#endif

#if !defined(_WIN32)
extern "C" int MessageBoxW(HWND wnd, const wchar_t *text, const wchar_t *caption, UINT type)
{
    char t[2048] = {}, c[256] = {};
    if (text) WideCharToMultiByte(CP_UTF8, 0, text, -1, t, sizeof(t) - 1, nullptr, nullptr);
    if (caption) WideCharToMultiByte(CP_UTF8, 0, caption, -1, c, sizeof(c) - 1, nullptr, nullptr);
    return MessageBoxA(wnd, t, c, type);
}

// Apertura di URL: la versione SDL (sdl_main.cpp) la sovrascrive con SDL_OpenURL.
extern "C" __attribute__((weak)) int bo1_OpenURL(const char *url)
{
#ifdef __ANDROID__
    (void)url;
    return -1;
#else
    std::string cmd = std::string("xdg-open '") + url + "' >/dev/null 2>&1 &";
    return system(cmd.c_str());
#endif
}
extern "C" HINSTANCE ShellExecuteA(HWND, LPCSTR, LPCSTR file, LPCSTR, LPCSTR, int)
{
    // Win32: un valore > 32 indica successo
    return reinterpret_cast<HINSTANCE>(static_cast<intptr_t>(file && bo1_OpenURL(file) == 0 ? 33 : 2));
}

extern "C" HRESULT CLSIDFromString(const wchar_t *str, GUID *guid)
{
    unsigned int d1, d2, d3, b[8];
    if (!str || !guid) return E_INVALIDARG;
    if (swscanf(str, L"{%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x}", &d1, &d2, &d3, &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7]) != 11)
        return static_cast<HRESULT>(0x800401F3); // CO_E_CLASSSTRING
    guid->Data1 = d1;
    guid->Data2 = static_cast<uint16_t>(d2);
    guid->Data3 = static_cast<uint16_t>(d3);
    for (int i = 0; i < 8; ++i) guid->Data4[i] = static_cast<uint8_t>(b[i]);
    return S_OK;
}

extern "C" BOOL SetFileAttributesA(LPCSTR path, DWORD attrs)
{
    char p[1024];
    bo1_resolve_path(path, p, sizeof(p));
    struct stat st;
    if (stat(p, &st) != 0) return FALSE;
    mode_t mode = st.st_mode & 0777;
    mode = (attrs & FILE_ATTRIBUTE_READONLY) ? (mode & ~0222u) : (mode | 0200u);
    return chmod(p, mode) == 0;
}

extern "C" HANDLE OpenThread(DWORD, BOOL, DWORD) { return nullptr; }

extern "C" BOOL K32GetProcessMemoryInfo(HANDLE, PROCESS_MEMORY_COUNTERS *pmc, DWORD cb)
{
    memset(pmc, 0, cb);
    pmc->cb = cb;
    long rss = 0, size = 0;
    if (FILE *f = fopen("/proc/self/statm", "r"))
    {
        if (fscanf(f, "%ld %ld", &size, &rss) != 2) { size = rss = 0; }
        fclose(f);
    }
    long page = sysconf(_SC_PAGESIZE);
    pmc->WorkingSetSize = pmc->PeakWorkingSetSize = static_cast<SIZE_T>(rss * page);
    pmc->PagefileUsage = pmc->PeakPagefileUsage = static_cast<SIZE_T>(size * page);
    return TRUE;
}

extern "C" BOOL GetProcessHandleCount(HANDLE, PDWORD count)
{
    DWORD n = 0;
    if (DIR *d = opendir("/proc/self/fd"))
    {
        while (readdir(d)) ++n;
        closedir(d);
    }
    *count = n;
    return TRUE;
}
#endif

#if !defined(_WIN32)
extern "C" __attribute__((weak)) HWND GetActiveWindow(void) { return nullptr; }
extern "C" __attribute__((weak)) HWND GetDesktopWindow(void) { return nullptr; }
extern "C" __attribute__((weak)) HWND GetForegroundWindow(void) { return nullptr; }
extern "C" __attribute__((weak)) int ShowCursor(BOOL show) { return show ? 0 : -1; }
#endif

#if !defined(_WIN32)
#include <sys/resource.h>
extern "C" BOOL GlobalMemoryStatusEx(LPMEMORYSTATUSEX ms)
{
    uint64_t page = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    uint64_t total = static_cast<uint64_t>(sysconf(_SC_PHYS_PAGES)) * page;
    uint64_t avail = static_cast<uint64_t>(sysconf(_SC_AVPHYS_PAGES)) * page;
    memset(ms, 0, sizeof(*ms));
    ms->dwLength = sizeof(*ms);
    ms->ullTotalPhys = ms->ullTotalPageFile = total;
    ms->ullAvailPhys = ms->ullAvailPageFile = avail;
    // spazio di indirizzamento: 4 GB per un processo a 32 bit (come Win32 LARGEADDRESSAWARE su Windows 64)
    ms->ullTotalVirtual = sizeof(void *) == 4 ? 0xFFFFFFFFull : (1ull << 47);
    ms->ullAvailVirtual = ms->ullTotalVirtual / 2;
    ms->dwMemoryLoad = total ? static_cast<DWORD>(100 - (avail * 100 / total)) : 0;
    return TRUE;
}

extern "C" HANDLE OpenProcess(DWORD, BOOL, DWORD) { return GetCurrentProcess(); }

static void ToFileTime(uint64_t ns, FILETIME *ft)
{
    uint64_t v = ns / 100;
    ft->dwLowDateTime = static_cast<DWORD>(v);
    ft->dwHighDateTime = static_cast<DWORD>(v >> 32);
}

extern "C" BOOL GetThreadTimes(HANDLE, FILETIME *creation, FILETIME *exit, FILETIME *kernel, FILETIME *user)
{
    // Il motore lo usa solo sul thread chiamante per misurare il proprio tempo CPU.
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    if (creation) ToFileTime(0, creation);
    if (exit) ToFileTime(0, exit);
    if (kernel) ToFileTime(0, kernel);
    if (user) ToFileTime(static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec), user);
    return TRUE;
}

extern "C" BOOL GetProcessTimes(HANDLE, FILETIME *creation, FILETIME *exit, FILETIME *kernel, FILETIME *user)
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    if (creation) ToFileTime(0, creation);
    if (exit) ToFileTime(0, exit);
    if (kernel) ToFileTime((static_cast<uint64_t>(ru.ru_stime.tv_sec) * 1000000ull + static_cast<uint64_t>(ru.ru_stime.tv_usec)) * 1000ull, kernel);
    if (user) ToFileTime((static_cast<uint64_t>(ru.ru_utime.tv_sec) * 1000000ull + static_cast<uint64_t>(ru.ru_utime.tv_usec)) * 1000ull, user);
    return TRUE;
}

extern "C" BOOL GetSystemTimes(FILETIME *idle, FILETIME *kernel, FILETIME *user)
{
    // /proc/stat: cpu user nice system idle ... in jiffies
    unsigned long long u = 0, n = 0, s = 0, i = 0;
    if (FILE *f = fopen("/proc/stat", "r"))
    {
        if (fscanf(f, "cpu %llu %llu %llu %llu", &u, &n, &s, &i) != 4) u = n = s = i = 0;
        fclose(f);
    }
    uint64_t tick = 1000000000ull / static_cast<uint64_t>(sysconf(_SC_CLK_TCK));
    if (idle) ToFileTime(i * tick, idle);
    if (kernel) ToFileTime((s + i) * tick, kernel); // come Windows: il tempo kernel include l'idle
    if (user) ToFileTime((u + n) * tick, user);
    return TRUE;
}
#endif

#if !defined(_WIN32)
extern "C" __attribute__((weak)) HCURSOR LoadCursorA(HINSTANCE, LPCSTR name) { return const_cast<char *>(name); }
extern "C" __attribute__((weak)) HCURSOR SetCursor(HCURSOR c) { return c; }
#endif

#if !defined(_WIN32)
static std::mutex g_findLock;
static std::vector<HANDLE> g_findHandles;
static HANDLE FindHandleFromId(intptr_t id)
{
    std::lock_guard<std::mutex> l(g_findLock);
    return (id >= 1 && static_cast<size_t>(id) <= g_findHandles.size()) ? g_findHandles[static_cast<size_t>(id - 1)] : nullptr;
}
static void FindToCrt(const WIN32_FIND_DATAA &fd, struct _finddata64i32_t *out)
{
    memset(out, 0, sizeof(*out));
    out->attrib = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? _A_SUBDIR : _A_NORMAL;
    out->size = fd.nFileSizeLow;
    snprintf(out->name, sizeof(out->name), "%s", fd.cFileName);
}
extern "C" intptr_t _findfirst64i32(const char *spec, struct _finddata64i32_t *data)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(spec, &fd);
    if (h == INVALID_HANDLE_VALUE) { errno = ENOENT; return -1; }
    FindToCrt(fd, data);
    // Il codice decompilato salva l'handle in un int: su 64 bit un puntatore non ci sta, usiamo un indice.
    std::lock_guard<std::mutex> l(g_findLock);
    g_findHandles.push_back(h);
    return static_cast<intptr_t>(g_findHandles.size());
}
extern "C" int _findnext64i32(intptr_t handle, struct _finddata64i32_t *data)
{
    WIN32_FIND_DATAA fd;
    if (!FindNextFileA(FindHandleFromId(handle), &fd)) { errno = ENOENT; return -1; }
    FindToCrt(fd, data);
    return 0;
}
extern "C" int _findclose(intptr_t handle)
{
    HANDLE h = FindHandleFromId(handle);
    if (!h) return -1;
    {
        std::lock_guard<std::mutex> l(g_findLock);
        g_findHandles[static_cast<size_t>(handle - 1)] = nullptr;
    }
    return FindClose(h) ? 0 : -1;
}
#endif

#if !defined(_WIN32)
extern "C" BOOL TerminateProcess(HANDLE, UINT code) { ExitProcess(code); }

extern "C" DWORD FormatMessageA(DWORD flags, LPCVOID, DWORD msgId, DWORD, LPSTR buf, DWORD size, va_list *)
{
    const char *text = strerror(static_cast<int>(msgId));
    if (flags & FORMAT_MESSAGE_ALLOCATE_BUFFER)
    {
        *reinterpret_cast<LPSTR *>(buf) = strdup(text);
        return static_cast<DWORD>(strlen(text));
    }
    snprintf(buf, size, "%s", text);
    return static_cast<DWORD>(strlen(buf));
}

// Memoria "globale": usata solo per gli appunti. Un blocco = testo terminato da zero.
extern "C" LPVOID GlobalLock(HGLOBAL mem) { return mem; }
extern "C" BOOL GlobalUnlock(HGLOBAL) { return TRUE; }
extern "C" SIZE_T GlobalSize(HGLOBAL mem) { return mem ? malloc_usable_size(mem) : 0; }
extern "C" HGLOBAL GlobalAlloc(UINT, SIZE_T bytes) { return calloc(1, bytes); }
extern "C" HGLOBAL GlobalFree(HGLOBAL mem) { free(mem); return nullptr; }
extern "C" HANDLE LocalFree(HANDLE mem) { free(mem); return nullptr; }

extern "C" BOOL GetVersionExA(LPOSVERSIONINFOA info)
{
    // Ci presentiamo come Windows 7 SP1 (6.1): il motore lo usa solo per log e piccoli workaround.
    info->dwMajorVersion = 6;
    info->dwMinorVersion = 1;
    info->dwBuildNumber = 7601;
    info->dwPlatformId = 2;
    snprintf(info->szCSDVersion, sizeof(info->szCSDVersion), "Linux/Android");
    return TRUE;
}

extern "C" LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER) { return nullptr; }
extern "C" BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value) { return (value ? setenv(name, value, 1) : unsetenv(name)) == 0; }
extern "C" BOOL RemoveDirectoryA(LPCSTR path)
{
    char p[1024];
    bo1_resolve_path(path, p, sizeof(p));
    return rmdir(p) == 0;
}
extern "C" DWORD GetFullPathNameA(LPCSTR name, DWORD size, LPSTR buf, LPSTR *filePart)
{
    char p[1024], full[PATH_MAX];
    bo1_resolve_path(name, p, sizeof(p));
    if (!realpath(p, full))
    {
        if (p[0] == '/') snprintf(full, sizeof(full), "%s", p);
        else { char cwd[PATH_MAX]; if (!getcwd(cwd, sizeof(cwd))) cwd[0] = 0; snprintf(full, sizeof(full), "%s/%s", cwd, p); }
    }
    size_t n = strlen(full);
    if (n + 1 > size) return static_cast<DWORD>(n + 1);
    memcpy(buf, full, n + 1);
    if (filePart) { char *s = strrchr(buf, '/'); *filePart = s ? s + 1 : buf; }
    return static_cast<DWORD>(n);
}
extern "C" BOOL SystemTimeToFileTime(const SYSTEMTIME *st, FILETIME *ft)
{
    struct tm t = {};
    t.tm_year = st->wYear - 1900; t.tm_mon = st->wMonth - 1; t.tm_mday = st->wDay;
    t.tm_hour = st->wHour; t.tm_min = st->wMinute; t.tm_sec = st->wSecond;
    time_t secs = timegm(&t);
    uint64_t v = (static_cast<uint64_t>(secs) + 11644473600ull) * 10000000ull + static_cast<uint64_t>(st->wMilliseconds) * 10000ull;
    ft->dwLowDateTime = static_cast<DWORD>(v);
    ft->dwHighDateTime = static_cast<DWORD>(v >> 32);
    return TRUE;
}
extern "C" LONG CompareFileTime(const FILETIME *a, const FILETIME *b)
{
    uint64_t x = (static_cast<uint64_t>(a->dwHighDateTime) << 32) | a->dwLowDateTime;
    uint64_t y = (static_cast<uint64_t>(b->dwHighDateTime) << 32) | b->dwLowDateTime;
    return x < y ? -1 : (x > y ? 1 : 0);
}
#endif
#if !defined(_WIN32)
extern "C" HBRUSH CreateSolidBrush(DWORD) { return nullptr; }
#endif

#if !defined(_WIN32)
extern "C" BOOL QueryThreadCycleTime(HANDLE thread, ULONG64 *cycles)
{
    // Solo il thread chiamante (GetCurrentThread): per gli altri non c'è un orologio portabile
    struct timespec ts;
    if (thread != GetCurrentThread() || clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) { *cycles = 0; return FALSE; }
    *cycles = static_cast<ULONG64>(ts.tv_sec) * 1000000000ull + static_cast<ULONG64>(ts.tv_nsec);
    return TRUE;
}
extern "C" BOOL QueryProcessCycleTime(HANDLE, ULONG64 *cycles)
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    *cycles = static_cast<ULONG64>(ts.tv_sec) * 1000000000ull + static_cast<ULONG64>(ts.tv_nsec);
    return TRUE;
}
#endif

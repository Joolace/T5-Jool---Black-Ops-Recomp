// bo1_android.cpp - pezzi solo Android: opzioni dal lanciatore Java, log su file, gestore dei crash e
// caricamento di un driver Vulkan personalizzato (Turnip) con libadrenotools.
//
// GameActivity.java passa al gioco argomenti "--t5-<nome>=<valore>" prima di quelli del motore:
//   --t5-home=<cartella>       cartella T5Jool sulla memoria condivisa (log, crash, configurazione)
//   --t5-libdir=<cartella>     nativeLibraryDir dell'APK (librerie hook di adrenotools)
//   --t5-cache=<cartella>      memoria interna dell'app (cache degli shader di DXVK)
//   --t5-driver=<cartella>     driver Vulkan personalizzato estratto (con meta.json); assente = driver di sistema
//   --t5-marker=<file>         file "partita in corso": si cancella all'uscita pulita, se resta il lanciatore
//                              sa che il gioco e' crashato e salva logcat
//   --t5-env=<NOME=valore>     variabile d'ambiente (DXVK_HUD, ...)
// bo1_android_init le toglie da argv prima che arrivino al motore.
#if defined(__ANDROID__)

#include "bo1_android.h"
#include <platform/bo1_lowmem.h>

#include <android/log.h>
#include <jni.h>
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <link.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/system_properties.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <unwind.h>

#include <string>

#if BO1_ADRENOTOOLS
#include <adrenotools/driver.h>
#endif

namespace
{
std::string g_home, g_libDir, g_cache, g_driver, g_marker;
char g_crashPath[512];
char g_logPath[512];
int g_logFd = -1;

void LogI(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void LogI(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    __android_log_write(ANDROID_LOG_INFO, "T5Jool", buf);
    fprintf(stderr, "T5Jool: %s\n", buf);
}

void MakeDirs(const std::string &path)
{
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i)
    {
        cur += path[i];
        if (path[i] == '/' && cur.size() > 1)
            mkdir(cur.c_str(), 0775);
    }
    mkdir(path.c_str(), 0775);
}

std::string Stamp()
{
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tmv);
    return buf;
}

// ---------------------------------------------------------------------------------------------- log
// stdout e stderr del gioco (console, BO1_SCRIPT, DXVK...) in <home>/logs/game.txt; il precedente diventa
// game.prev.txt. Le righe vanno anche in logcat (tag BO1) tramite un thread che legge una pipe.
void RedirectOutput()
{
    std::string dir = g_home + "/logs";
    MakeDirs(dir);
    std::string log = dir + "/game.txt", prev = dir + "/game.prev.txt";
    rename(log.c_str(), prev.c_str());
    snprintf(g_logPath, sizeof(g_logPath), "%s", log.c_str());
    g_logFd = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0664);
    if (g_logFd < 0)
    {
        __android_log_print(ANDROID_LOG_ERROR, "T5Jool", "impossibile creare %s: %s", log.c_str(), strerror(errno));
        return;
    }
    fflush(stdout);
    fflush(stderr);
    dup2(g_logFd, STDOUT_FILENO);
    dup2(g_logFd, STDERR_FILENO);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    char model[PROP_VALUE_MAX] = "", soc[PROP_VALUE_MAX] = "", release[PROP_VALUE_MAX] = "";
    __system_property_get("ro.product.model", model);
    __system_property_get("ro.soc.model", soc);
    __system_property_get("ro.build.version.release", release);
    fprintf(stderr, "T5Jool Android - %s - device %s, SoC %s, Android %s\n", Stamp().c_str(), model, soc, release);
}

// ------------------------------------------------------------------------------------------- crash
// Il gestore scrive <home>/logs/crash-<data>.txt con segnale, registri e stack (indirizzo, libreria e
// offset: tools/android/symbolize.sh li traduce in funzione e riga con la libmain.so non strippata),
// poi lascia che Android completi il crash (tombstone e logcat).
struct sigaction g_oldActions[32];
volatile sig_atomic_t g_inCrash;

void Out(int fd, const char *s)
{
    if (fd >= 0)
        write(fd, s, strlen(s));
}

void PrintFrame(int fd, int index, uintptr_t pc)
{
    char line[512];
    Dl_info info;
    if (dladdr(reinterpret_cast<void *>(pc), &info) && info.dli_fname)
    {
        const char *lib = strrchr(info.dli_fname, '/');
        lib = lib ? lib + 1 : info.dli_fname;
        uintptr_t off = pc - reinterpret_cast<uintptr_t>(info.dli_fbase);
        if (info.dli_sname)
            snprintf(line, sizeof(line), "  #%02d pc %012lx  %s+0x%lx (%s+%lu)\n", index, (unsigned long)pc, lib,
                     (unsigned long)off, info.dli_sname, (unsigned long)(pc - reinterpret_cast<uintptr_t>(info.dli_saddr)));
        else
            snprintf(line, sizeof(line), "  #%02d pc %012lx  %s+0x%lx\n", index, (unsigned long)pc, lib, (unsigned long)off);
    }
    else
        snprintf(line, sizeof(line), "  #%02d pc %012lx  ?\n", index, (unsigned long)pc);
    Out(fd, line);
}

struct UnwindState
{
    int fd;
    int count;
};

_Unwind_Reason_Code UnwindFrame(struct _Unwind_Context *ctx, void *arg)
{
    auto *st = static_cast<UnwindState *>(arg);
    uintptr_t pc = _Unwind_GetIP(ctx);
    if (pc)
        PrintFrame(st->fd, st->count++, pc);
    return st->count < 64 ? _URC_NO_REASON : _URC_END_OF_STACK;
}

bool Readable(uintptr_t p)
{
    // senza leggere la memoria: msync fallisce con ENOMEM sulle pagine non mappate
    return p && (p & 7) == 0 && msync(reinterpret_cast<void *>(p & ~uintptr_t(4095)), 4096, MS_ASYNC) == 0;
}

const char *SignalName(int sig)
{
    switch (sig)
    {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS: return "SIGBUS";
    case SIGILL: return "SIGILL";
    case SIGFPE: return "SIGFPE";
    case SIGABRT: return "SIGABRT";
    case SIGTRAP: return "SIGTRAP";
    default: return "?";
    }
}

// T5Jool: BuildId di libmain.so (nota GNU del file ELF), scritto nel log e nei crash: tools/android/symbolize.sh
// sceglie cosi' i simboli della build giusta (out/symbols/libmain-<BuildId>.so)
char g_buildId[48] = "?";
int FindBuildId(struct dl_phdr_info *info, size_t, void *)
{
    if (!info->dlpi_name || !strstr(info->dlpi_name, "libmain.so"))
        return 0;
    for (int i = 0; i < info->dlpi_phnum; ++i)
    {
        const ElfW(Phdr) &ph = info->dlpi_phdr[i];
        if (ph.p_type != PT_NOTE)
            continue;
        const char *p = reinterpret_cast<const char *>(info->dlpi_addr + ph.p_vaddr), *end = p + ph.p_memsz;
        while (p + sizeof(ElfW(Nhdr)) <= end)
        {
            const ElfW(Nhdr) *n = reinterpret_cast<const ElfW(Nhdr) *>(p);
            const char *name = p + sizeof(ElfW(Nhdr));
            const unsigned char *desc = reinterpret_cast<const unsigned char *>(name + ((n->n_namesz + 3) & ~3u));
            if (n->n_type == NT_GNU_BUILD_ID && n->n_namesz == 4 && !memcmp(name, "GNU", 4))
            {
                char *o = g_buildId;
                for (unsigned j = 0; j < n->n_descsz && j < 20; ++j, o += 2)
                    snprintf(o, 3, "%02x", desc[j]);
                return 1;
            }
            p = reinterpret_cast<const char *>(desc) + ((n->n_descsz + 3) & ~3u);
        }
    }
    return 0;
}

void WriteCrash(int fd, int sig, siginfo_t *si, void *uctx, bool dump = false)
{
    char line[512];
    char thread[32] = "";
    pthread_getname_np(pthread_self(), thread, sizeof(thread));
    if (dump)
        snprintf(line, sizeof(line), "\n==== T5Jool stack ====\nthread \"%s\" (tid %d)\n", thread, gettid());
    else
        snprintf(line, sizeof(line), "\n==== T5Jool CRASH ====\nsignal %d (%s) code %d, fault address %p, thread \"%s\" (tid %d)\n", sig,
                 SignalName(sig), si ? si->si_code : 0, si ? si->si_addr : nullptr, thread, gettid());
    Out(fd, line);
    snprintf(line, sizeof(line), "libmain.so BuildId: %s\n", g_buildId);
    Out(fd, line);
#if defined(__aarch64__)
    if (uctx)
    {
        const mcontext_t &mc = static_cast<ucontext_t *>(uctx)->uc_mcontext;
        snprintf(line, sizeof(line), "pc %016llx  lr %016llx  sp %016llx  fp %016llx\n", (unsigned long long)mc.pc,
                 (unsigned long long)mc.regs[30], (unsigned long long)mc.sp, (unsigned long long)mc.regs[29]);
        Out(fd, line);
        for (int i = 0; i < 29; i += 4)
        {
            snprintf(line, sizeof(line), "x%-2d %016llx  x%-2d %016llx  x%-2d %016llx  x%-2d %016llx\n", i,
                     (unsigned long long)mc.regs[i], i + 1, (unsigned long long)(i + 1 < 31 ? mc.regs[i + 1] : 0), i + 2,
                     (unsigned long long)(i + 2 < 31 ? mc.regs[i + 2] : 0), i + 3, (unsigned long long)(i + 3 < 31 ? mc.regs[i + 3] : 0));
            Out(fd, line);
        }
        Out(fd, "backtrace (frame pointer):\n");
        PrintFrame(fd, 0, mc.pc);
        PrintFrame(fd, 1, mc.regs[30]);
        uintptr_t fp = mc.regs[29];
        for (int i = 2; i < 64 && Readable(fp); ++i)
        {
            const uintptr_t *frame = reinterpret_cast<const uintptr_t *>(fp);
            uintptr_t next = frame[0], ret = frame[1];
            if (!ret)
                break;
            PrintFrame(fd, i, ret);
            if (next <= fp)
                break;
            fp = next;
        }
        // T5Jool: quando pc/lr sono spazzatura (salto a un indirizzo non valido) la catena dei frame non e' affidabile:
        // gli indirizzi di ritorno in libmain.so rimasti sullo stack dicono comunque da dove arriva il salto.
        Out(fd, "registri che puntano in libmain.so:\n");
        for (int r = 0; r < 31; ++r)
        {
            Dl_info info;
            const uintptr_t w = mc.regs[r];
            if (w < 0x10000 || !dladdr(reinterpret_cast<void *>(w), &info) || !info.dli_fname
                || !strstr(info.dli_fname, "libmain.so"))
                continue;
            char line2[32];
            snprintf(line2, sizeof(line2), "  x%-2d", r);
            Out(fd, line2);
            PrintFrame(fd, r, w);
        }
        Out(fd, "stack scan (libmain.so, dal piu' recente):\n");
        const uintptr_t sp = mc.sp;
        int found = 0;
        for (uintptr_t a = sp & ~uintptr_t(7); a < sp + 32768 && found < 40; a += 8)
        {
            if ((a & 4095) == 0 && !Readable(a))
                break;
            if (a == (sp & ~uintptr_t(7)) && !Readable(a))
                break;
            const uintptr_t w = *reinterpret_cast<const uintptr_t *>(a);
            Dl_info info;
            if (w < 0x10000 || !dladdr(reinterpret_cast<void *>(w), &info) || !info.dli_fname
                || !strstr(info.dli_fname, "libmain.so"))
                continue;
            char line2[96];
            snprintf(line2, sizeof(line2), "  [sp+0x%lx]", (unsigned long)(a - sp));
            Out(fd, line2);
            PrintFrame(fd, found++, w);
        }
    }
#endif
    Out(fd, "backtrace (unwind):\n");
    UnwindState st{ fd, 0 };
    _Unwind_Backtrace(UnwindFrame, &st);
    Out(fd, "==== fine ====\n");
}

extern "C" void bo1_log_flush_crash(void); // platform/win_compat.cpp
extern "C" void bo1_log_flush(void);

void CrashHandler(int sig, siginfo_t *si, void *uctx)
{
    if (!g_inCrash)
    {
        g_inCrash = 1;
        bo1_log_flush_crash(); // le ultime righe della console ancora nel buffer
        int fd = open(g_crashPath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0664);
        WriteCrash(fd, sig, si, uctx);
        if (fd >= 0)
            close(fd);
        WriteCrash(STDERR_FILENO, sig, si, uctx); // anche in game.txt, dopo le ultime righe della console
        __android_log_print(ANDROID_LOG_FATAL, "T5Jool", "crash: segnale %d, dettagli in %s", sig, g_crashPath);
    }
    // gestore originale (debuggerd: tombstone), poi il segnale riparte con quello
    sigaction(sig, &g_oldActions[sig], nullptr);
    raise(sig);
}

// T5Jool: SIGUSR2 = scrivi lo stack di questo thread in game.txt e continua (bo1_android_dump_threads)
volatile sig_atomic_t g_dumpDone;
void DumpHandler(int sig, siginfo_t *si, void *uctx)
{
    WriteCrash(STDERR_FILENO, sig, si, uctx, true);
    g_dumpDone = 1;
}

void InstallCrashHandler()
{
    struct sigaction du{};
    du.sa_sigaction = DumpHandler;
    du.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&du.sa_mask);
    sigaction(SIGUSR2, &du, nullptr);

    std::string dir = g_home + "/logs";
    snprintf(g_crashPath, sizeof(g_crashPath), "%s/crash-%s.txt", dir.c_str(), Stamp().c_str());
    // stack alternativo: un crash per stack esaurito deve poter ancora scrivere il log
    static char altStack[256 * 1024];
    stack_t ss{};
    ss.ss_sp = altStack;
    ss.ss_size = sizeof(altStack);
    sigaltstack(&ss, nullptr);
    struct sigaction sa{};
    sa.sa_sigaction = CrashHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    for (int sig : { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP })
        sigaction(sig, &sa, &g_oldActions[sig]);
}

// -------------------------------------------------------------------------------- driver Vulkan
// T5Jool: driver Vulkan non Qualcomm (es. PanVK di Mesa per le GPU Mali): libadrenotools serve solo con il caricatore
// Vulkan di Qualcomm, quindi la libreria si apre direttamente e DXVK usa il suo vkGetInstanceProcAddr. I driver Mesa
// esportano vkGetInstanceProcAddr e/o vk_icdGetInstanceProcAddr; la versione dell'interfaccia si negozia come farebbe
// il caricatore. Senza una Mali per provare: e' la strada piu' probabile, e il log dice dove si ferma.
bool LoadDriverDirect(const std::string &path)
{
    void *h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h)
    {
        LogI("driver Vulkan %s: dlopen non riuscito (%s)", path.c_str(), dlerror());
        return false;
    }
    typedef int (*PFN_Negotiate)(uint32_t *);
    if (PFN_Negotiate neg = (PFN_Negotiate)dlsym(h, "vk_icdNegotiateLoaderICDInterfaceVersion"))
    {
        uint32_t ver = 5;
        const int r = neg(&ver);
        LogI("driver Vulkan %s: interfaccia ICD versione %u (risultato %d)", path.c_str(), ver, r);
    }
    void *gipa = dlsym(h, "vk_icdGetInstanceProcAddr");
    const char *which = "vk_icdGetInstanceProcAddr";
    if (!gipa)
    {
        gipa = dlsym(h, "vkGetInstanceProcAddr");
        which = "vkGetInstanceProcAddr";
    }
    if (!gipa)
    {
        LogI("driver Vulkan %s: nessun vkGetInstanceProcAddr esportato", path.c_str());
        return false;
    }
    char hex[32];
    snprintf(hex, sizeof(hex), "%lx", (unsigned long)reinterpret_cast<uintptr_t>(gipa));
    setenv("DXVK_ANDROID_GIPA", hex, 1);
    LogI("driver Vulkan personalizzato (caricato direttamente, %s): %s", which, path.c_str());
    return true;
}

// GPU del dispositivo dalle proprieta' di sistema (per il log e per scegliere come caricare un driver personalizzato)
std::string GpuVendorHint()
{
    char v[PROP_VALUE_MAX] = {};
    std::string out;
    for (const char *prop : { "ro.hardware.egl", "ro.hardware.vulkan", "ro.soc.manufacturer", "ro.board.platform" })
        if (__system_property_get(prop, v) > 0)
        {
            if (!out.empty())
                out += " ";
            out += v;
        }
    return out;
}

bool g_gpuMali = false;      // GPU Mali (per il messaggio se la grafica non parte)
bool g_customDriverSet = false; // l'utente ha scelto un driver personalizzato nel launcher

void LoadCustomDriver()
{
    const std::string gpuHint = GpuVendorHint();
    LogI("GPU del dispositivo: %s", gpuHint.c_str());
    std::string lower = gpuHint;
    for (char &c : lower)
        c = (char)tolower((unsigned char)c);
    g_gpuMali = lower.find("mali") != std::string::npos;
    g_customDriverSet = !g_driver.empty();
    const bool qualcomm = lower.find("adreno") != std::string::npos || lower.find("qti") != std::string::npos
        || lower.find("qcom") != std::string::npos;
    if (g_driver.empty())
    {
        LogI("driver Vulkan: sistema");
        return;
    }
#if BO1_ADRENOTOOLS
    // la cartella contiene meta.json e la libreria del driver (libvulkan_freedreno.so per Turnip)
    std::string dir = g_driver;
    if (dir.back() != '/')
        dir += '/';
    std::string lib;
    if (FILE *f = fopen((dir + "meta.json").c_str(), "r"))
    {
        char buf[4096];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        buf[n] = 0;
        if (const char *p = strstr(buf, "\"libraryName\""))
        {
            p = strchr(p + 13, '"');
            const char *e = p ? strchr(p + 1, '"') : nullptr;
            if (p && e)
                lib.assign(p + 1, e);
        }
    }
    if (lib.empty())
    {
        if (DIR *d = opendir(dir.c_str()))
        {
            while (dirent *de = readdir(d))
                if (strstr(de->d_name, ".so"))
                {
                    lib = de->d_name;
                    break;
                }
            closedir(d);
        }
    }
    if (lib.empty())
    {
        LogI("driver Vulkan personalizzato: nessuna libreria in %s, uso il driver di sistema", dir.c_str());
        return;
    }
    // Mali, PowerVR, Xclipse...: prima libadrenotools anche qui. I driver Mesa per Android (PanVK) sono un HAL Vulkan
    // come quello Qualcomm e dipendono da libhardware.so, che si apre solo dal namespace dei driver (sphal): con dlopen
    // diretto falliva ("libhardware.so not found"). Il caricatore Vulkan di sistema apre il suo HAL "vulkan.<nome>.so"
    // con android_load_sphal_library, che libadrenotools reindirizza al driver scelto, con qualunque produttore.
    if (!qualcomm && lib.find("freedreno") != std::string::npos)
    {
        // Turnip (freedreno) e' solo per Adreno: su Mali e altre GPU non parte e la grafica fallisce senza spiegazioni
        LogI("driver Vulkan personalizzato %s: e' Turnip, solo per GPU Adreno (Snapdragon). Questa GPU non e' Adreno: "
             "uso il driver di sistema. Per le GPU Mali scegli un driver PanVK nel launcher", lib.c_str());
        fprintf(stderr, "T5Jool: il driver scelto (%s) e' Turnip, solo per GPU Adreno: su questa GPU serve PanVK (Mali)\n",
                lib.c_str());
        return;
    }
    if (!qualcomm)
        LogI("driver Vulkan personalizzato su GPU non Qualcomm: provo con libadrenotools, poi caricandolo direttamente");
    std::string tmp = g_cache + "/adrenotools_tmp/";
    MakeDirs(tmp);
    std::string hooks = g_libDir + "/";
    void *h = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, tmp.c_str(), hooks.c_str(), dir.c_str(),
                                         lib.c_str(), nullptr, nullptr);
    if (!h)
    {
        LogI("driver Vulkan personalizzato %s%s non caricato con libadrenotools (%s): provo a caricarlo direttamente",
             dir.c_str(), lib.c_str(), dlerror());
        LoadDriverDirect(dir + lib);
        return;
    }
    void *gipa = dlsym(h, "vkGetInstanceProcAddr");
    if (!gipa)
    {
        LogI("driver Vulkan personalizzato senza vkGetInstanceProcAddr: uso il driver di sistema");
        return;
    }
    char hex[32];
    snprintf(hex, sizeof(hex), "%lx", (unsigned long)reinterpret_cast<uintptr_t>(gipa));
    setenv("DXVK_ANDROID_GIPA", hex, 1);
    LogI("driver Vulkan personalizzato: %s%s", dir.c_str(), lib.c_str());
#else
    LogI("driver Vulkan personalizzato chiesto, ma questa build e' senza libadrenotools");
#endif
}
} // namespace

// T5Jool: lo stack di ogni thread del processo in game.txt, uno alla volta (guardiano dell'avvio, sdl_main.cpp).
// Salta i thread di Java/ART: interessano quelli del gioco, del render, di DXVK e del driver.
void bo1_android_dump_threads()
{
    DIR *d = opendir("/proc/self/task");
    if (!d)
        return;
    const pid_t self = gettid();
    int dumped = 0;
    while (dirent *e = readdir(d))
    {
        const pid_t tid = atoi(e->d_name);
        if (tid <= 0 || tid == self || dumped >= 48)
            continue;
        char path[64], name[32] = "";
        snprintf(path, sizeof(path), "/proc/self/task/%d/comm", tid);
        if (FILE *f = fopen(path, "r"))
        {
            if (fgets(name, sizeof(name), f))
                name[strcspn(name, "\n")] = 0;
            fclose(f);
        }
        static const char *const skip[] = { "Jit thread", "Signal Catcher", "perfetto", "ADB-JDWP", "HeapTaskDaemon",
                                            "ReferenceQueueD", "FinalizerDaemon", "FinalizerWatchd", "binder:", "hwuiTask",
                                            "RenderThread", "Profile Saver", "Runtime worker", "ThreadPoolForeg" };
        bool javaThread = false;
        for (const char *k : skip)
            javaThread |= strncmp(name, k, strlen(k)) == 0;
        if (javaThread)
            continue;
        g_dumpDone = 0;
        if (syscall(SYS_tgkill, getpid(), tid, SIGUSR2) != 0)
            continue;
        for (int i = 0; i < 50 && !g_dumpDone; ++i)
            usleep(10000);
        ++dumped;
    }
    closedir(d);
    fflush(stderr);
}

// T5Jool: dimensione delle cache degli shader (file e byte, sottocartelle comprese): nel log all'avvio e all'uscita,
// per vedere se crescono davvero tra una partita e l'altra
static void DirSize(const std::string &dir, long long *files, long long *bytes, int depth = 0)
{
    DIR *d = opendir(dir.c_str());
    if (!d)
        return;
    while (dirent *e = readdir(d))
    {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        const std::string path = dir + "/" + e->d_name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode))
        {
            if (depth < 6)
                DirSize(path, files, bytes, depth + 1);
        }
        else
        {
            ++*files;
            *bytes += st.st_size;
        }
    }
    closedir(d);
}

static void ReportShaderCache(const char *when)
{
    if (g_cache.empty())
        return;
    long long df = 0, db = 0, mf = 0, mb = 0;
    DirSize(g_cache + "/dxvk_cache", &df, &db);
    DirSize(g_cache + "/mesa_shader_cache", &mf, &mb);
    fprintf(stderr, "T5Jool: cache shader (%s): DXVK %lld file %.1f MB, Mesa %lld file %.1f MB\n", when, df,
            db / 1048576.0, mf, mb / 1048576.0);
}

void bo1_android_init(int *argc, char **argv)
{
    int out = 1;
    for (int i = 1; i < *argc; ++i)
    {
        const char *a = argv[i];
        if (strncmp(a, "--t5-", 5) != 0)
        {
            argv[out++] = argv[i];
            continue;
        }
        const char *eq = strchr(a, '=');
        std::string key(a + 5, eq ? eq - (a + 5) : strlen(a + 5)), val(eq ? eq + 1 : "");
        if (key == "home") g_home = val;
        else if (key == "libdir") g_libDir = val;
        else if (key == "cache") g_cache = val;
        else if (key == "driver") g_driver = val;
        else if (key == "marker") g_marker = val;
        else if (key == "env")
        {
            const char *e = strchr(val.c_str(), '=');
            if (e)
                setenv(std::string(val.c_str(), e).c_str(), e + 1, 1);
        }
    }
    argv[out] = nullptr;
    *argc = out;

    if (g_home.empty())
        g_home = "/storage/emulated/0/T5Jool";
    MakeDirs(g_home);
    RedirectOutput();
    InstallCrashHandler();
    dl_iterate_phdr(FindBuildId, nullptr);
    fprintf(stderr, "T5Jool: libmain.so BuildId: %s\n", g_buildId);
    bo1_low_report();
    for (int i = 1; i < *argc; ++i)
        fprintf(stderr, "argv[%d] = %s\n", i, argv[i]);

    // DXVK: log su stderr (quindi in game.txt), cache degli shader nella memoria interna
    setenv("DXVK_LOG_PATH", "none", 1);
    if (!g_cache.empty())
    {
        std::string cache = g_cache + "/dxvk_cache";
        MakeDirs(cache);
        setenv("DXVK_STATE_CACHE_PATH", cache.c_str(), 0);
        // Turnip (Mesa): cache degli shader compilati su disco. Su Android HOME/XDG_CACHE_HOME non ci sono e Mesa
        // la disattiva: ogni avvio ricompilava tutto (scatti all'inizio della partita).
        std::string mesa = g_cache + "/mesa_shader_cache";
        MakeDirs(mesa);
        setenv("MESA_SHADER_CACHE_DIR", mesa.c_str(), 0);
        setenv("MESA_SHADER_CACHE_MAX_SIZE", "512M", 0);
        setenv("MESA_DISK_CACHE_SINGLE_FILE", "1", 0);
        // le build di Mesa per Android hanno la cache su disco spenta di serie (0 file dopo varie partite): false la
        // accende quando il driver e' compilato con il supporto (shader-cache-default=false)
        setenv("MESA_SHADER_CACHE_DISABLE", "false", 0);
        setenv("MESA_SHADER_CACHE_SHOW_STATS", "1", 0); // all'uscita Mesa scrive quante volte l'ha usata
        const bool writable = access(cache.c_str(), W_OK) == 0;
        fprintf(stderr, "T5Jool: cache shader DXVK %s (%s), Mesa %s\n", cache.c_str(), writable ? "scrivibile" : "NON scrivibile",
                mesa.c_str());
        ReportShaderCache("avvio");
    }
    setenv("T5JOOL_HOME", g_home.c_str(), 1);
    LoadCustomDriver();
}

const char *bo1_android_home()
{
    return g_home.c_str();
}

// SDL (libSDL2.so): ambiente JNI del thread e attivita' del gioco
extern "C" void *SDL_AndroidGetJNIEnv(void);
extern "C" void *SDL_AndroidGetActivity(void);

bool bo1_android_relaunch(const char *cmd)
{
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject activity = env ? (jobject)SDL_AndroidGetActivity() : nullptr;
    if (!activity)
        return false;
    jclass cls = env->GetObjectClass(activity);
    jmethodID m = cls ? env->GetMethodID(cls, "relaunchGame", "(Ljava/lang/String;)V") : nullptr;
    if (!m)
    {
        if (env->ExceptionCheck())
            env->ExceptionClear();
        env->DeleteLocalRef(activity);
        return false;
    }
    fprintf(stderr, "T5Jool: riavvio del gioco per la prossima mappa (memoria pulita): %s\n", cmd);
    fflush(stderr);
    jstring jcmd = env->NewStringUTF(cmd);
    env->CallVoidMethod(activity, m, jcmd);
    const bool ok = !env->ExceptionCheck();
    if (!ok)
        env->ExceptionClear();
    env->DeleteLocalRef(jcmd);
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
    return ok;
}

static volatile bool s_screenDrawn;
void bo1_android_screen_drawn()
{
    s_screenDrawn = true;
}

void bo1_android_frame_presented()
{
    static int s_presented;
    if (!s_screenDrawn || ++s_presented != 2)
        return;
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject activity = env ? (jobject)SDL_AndroidGetActivity() : nullptr;
    if (!activity)
        return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID m = cls ? env->GetMethodID(cls, "gameReady", "()V") : nullptr;
    if (m)
        env->CallVoidMethod(activity, m);
    if (env->ExceptionCheck())
        env->ExceptionClear();
    if (cls)
        env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
}

void bo1_android_clean_exit()
{
    ReportShaderCache("uscita");
    bo1_log_flush();
    if (!g_marker.empty())
        unlink(g_marker.c_str());
    fflush(stdout);
    fflush(stderr);
}

#endif // __ANDROID__

// T5Jool: testo per la finestra "grafica non partita" (nullptr = messaggio generico). Sulle Mali il driver di sistema
// non ha VK_EXT_robustness2 e DXVK non parte: serve PanVK, scelto nel launcher.
const char *bo1_android_graphics_hint()
{
    if (!g_gpuMali)
        return nullptr;
    if (!g_customDriverSet)
        return "Graphics initialization failed.\n\nYour phone has a Mali GPU. The stock Mali driver lacks a Vulkan feature "
               "T5Jool needs (VK_EXT_robustness2).\n\nInstall a PanVK driver in the launcher (Vulkan driver), for example "
               "FristOneRR PanVK for Mali-G57, G68, G77 and G78, then start the game again.";
    return "Graphics initialization failed.\n\nThe custom Vulkan driver you chose could not start the graphics on this Mali "
           "GPU. Try another PanVK driver in the launcher, and send your logs on Discord if it still fails.";
}

// sdl_main.cpp - punto d'ingresso su Linux e Android: prepara SDL e l'ambiente, poi chiama il WinMain del motore.
#if !defined(_WIN32)

#include <platform/compat/win_compat.h>
#include <SDL.h>
#include <platform/bo1_lowmem.h>
#ifdef __ANDROID__
#include <platform/android/bo1_android.h>
#endif

#include <pthread.h>

#include <string>
#include <vector>
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char *lpCmdLine, int nCmdShow);

static std::string GameDirectory(int argc, char **argv)
{
    // 1) variabile d'ambiente, 2) "+set fs_basepath <dir>" passato a mano, 3) Android: memoria esterna dell'app,
    // 4) cartella dell'eseguibile (come su Windows, dove il gioco parte dalla sua cartella).
    if (const char *env = getenv("BO1_GAME_DIR")) return env;
    for (int i = 1; i + 2 < argc; ++i)
        if (!strcmp(argv[i], "+set") && !strcasecmp(argv[i + 1], "fs_basepath")) return argv[i + 2];
#ifdef __ANDROID__
    if (const char *ext = SDL_AndroidGetExternalStoragePath()) return ext;
#endif
    char *base = SDL_GetBasePath();
    std::string dir = base ? base : ".";
    SDL_free(base);
    return dir;
}

static int GameMain(int argc, char **argv)
{

    // DXVK-native: finestre e superfici Vulkan passano da SDL2
    setenv("DXVK_WSI_DRIVER", "SDL2", 0);

    // T5Jool: server dedicato ("+set dedicated 1|2"): niente finestra, niente audio, anche su un server senza schermo
    for (int i = 1; i + 2 < argc; ++i)
        if (!strcmp(argv[i], "+set") && !strcasecmp(argv[i + 1], "dedicated") && atoi(argv[i + 2]) > 0)
        {
            setenv("SDL_VIDEODRIVER", "dummy", 1);
            setenv("SDL_AUDIODRIVER", "dummy", 1);
            setenv("T5JOOL_DEDICATED", "1", 1);
            // Ctrl+C / kill chiudono il server (il motore non gestisce i segnali e Ctrl+C veniva ignorato)
            signal(SIGINT, [](int) { _exit(0); });
            signal(SIGTERM, [](int) { _exit(0); });
            fprintf(stderr, "T5Jool: server dedicato, senza grafica ne' audio\n");
            break;
        }

    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "1");
#endif
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0"); // posizioni fisiche stile Xbox anche sui pad Nintendo
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0)
    {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    std::string dir = GameDirectory(argc, argv);
    if (chdir(dir.c_str()) != 0)
        fprintf(stderr, "Cartella del gioco non trovata: %s\n", dir.c_str());
    if (!getenv("BO1_DATA_DIR"))
    {
#ifdef __ANDROID__
        const char *pref = SDL_AndroidGetInternalStoragePath();
        if (pref) setenv("BO1_DATA_DIR", pref, 1);
#else
        char *pref = SDL_GetPrefPath("Joolace", "BO1Zombies");
        if (pref) { setenv("BO1_DATA_DIR", pref, 1); SDL_free(pref); }
#endif
    }

    // WinMain riceve la riga di comando senza il nome del programma
    std::string cmdline;
    for (int i = 1; i < argc; ++i)
    {
        if (i > 1) cmdline += ' ';
        bool quote = strchr(argv[i], ' ') != nullptr;
        if (quote) cmdline += '"';
        cmdline += argv[i];
        if (quote) cmdline += '"';
    }

    int code = WinMain(GetModuleHandleA(nullptr), nullptr, cmdline.data(), SW_SHOWNORMAL);
    SDL_Quit();
    return code;
}

#ifdef BO1_LOWMEM
// A 64 bit il motore converte anche indirizzi di variabili locali in int: tutto il gioco (SDL compreso, che
// deve restare sul thread che l'ha inizializzato) gira su un thread con lo stack sotto i 2 GB.
struct MainArgs { int argc; char **argv; int code; };
static void *GameThread(void *p)
{
    auto *a = static_cast<MainArgs *>(p);
    a->code = GameMain(a->argc, a->argv);
    return nullptr;
}
#endif


// Guardiano della RAM: se il processo supera BO1_RSS_LIMIT_MB (predefinito 4000) si ferma con abort(), cos√¨ gdb
// stampa lo stack di tutti i thread invece del SIGKILL muto del kernel (OOM killer). 0 = disattivato.
#include <thread>
#include <chrono>
static void StartRssWatchdog()
{
    long limitMb = 4000;
    if (const char *e = getenv("BO1_RSS_LIMIT_MB")) limitMb = atol(e);
    if (limitMb <= 0) return;
    std::thread([limitMb]() {
        const long page = sysconf(_SC_PAGESIZE);
        for (;;)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            FILE *f = fopen("/proc/self/statm", "r");
            if (!f) return;
            long size = 0, rss = 0;
            int n = fscanf(f, "%ld %ld", &size, &rss);
            fclose(f);
            if (n == 2 && rss * page / (1024 * 1024) > limitMb)
            {
                fprintf(stderr, "BO1_RSS: memoria residente %ld MB oltre il limite di %ld MB: abort per lo stack\n",
                        rss * page / (1024 * 1024), limitMb);
                fflush(stderr);
                abort();
            }
        }
    }).detach();
}

extern "C" void bo1_log_flush(void);

// T5Jool: guardiano dell'avvio. Se il motore non finisce Com_Init entro BO1_INIT_TIMEOUT secondi (predefinito 45,
// 0 = spento) il thread del gioco riceve SIGABRT: il gestore dei crash scrive lo stack del punto in cui Ë fermo
// (su Android logs/crash-*.txt) invece di una schermata nera senza indizi.
// Avvio multiplayer di prova (bo1_zombies 0): il guardiano resta attivo anche dopo l'avvio e scatta se nessun frame
// (Com_Frame) parte per 60 secondi, per esempio un caricamento di mappa bloccato.
extern int com_fullyInitialized;
extern volatile int g_t5Frames;
static bool s_hangWatch;
static void StartInitWatchdog(pthread_t game)
{
    long seconds = 45;
    if (const char *e = getenv("BO1_INIT_TIMEOUT")) seconds = atol(e);
    if (seconds <= 0) return;
    std::thread([seconds, game]() {
        long ms = 0;
        for (; ms < seconds * 1000 && !com_fullyInitialized; ms += 250)
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (com_fullyInitialized)
        {
            if (!s_hangWatch) return;
            int last = g_t5Frames;
            for (ms = 0; ms < 60000; ms += 250)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                if (g_t5Frames != last) { last = g_t5Frames; ms = 0; }
            }
            fprintf(stderr, "T5Jool: nessun frame da 60 s (gioco bloccato): SIGABRT al thread del gioco per lo stack\n");
        }
        else
            fprintf(stderr, "T5Jool: avvio fermo da %ld s (Com_Init non finito): SIGABRT al thread del gioco per lo stack\n", seconds);
        fflush(stderr);
#ifdef __ANDROID__
        bo1_android_dump_threads(); // anche il thread del render, di DXVK e del driver: chi aspetta chi
#endif
        pthread_kill(game, SIGABRT);
    }).detach();
}

// T5Jool: argomenti originali, per riavviare il gioco (Mods) con gli stessi argomenti
static int s_origArgc;
static char **s_origArgv;

extern "C" bool bo1_restart_game(void)
{
    bo1_log_flush();
#ifdef __ANDROID__
    return bo1_android_relaunch("");
#else
    // solo gli argomenti di partenza: senza quelli aggiunti da Versus (bo1_restart_game_cmd)
    std::vector<char *> args(s_origArgv, s_origArgv + s_origArgc);
    args.push_back(nullptr);
    execv("/proc/self/exe", args.data());
    return false; // execv torna solo se non e' riuscito
#endif
}

// T5Jool: riavvio con comandi in piu' ("set a 1;devmap mp_x" -> +set a 1 +devmap mp_x in fondo agli argomenti
// originali: l'ultimo +set vince). Versus (beta) del menu: il multiplayer parte solo da un processo nuovo.
extern "C" bool bo1_restart_game_cmd(const char *cmd)
{
    bo1_log_flush();
#ifdef __ANDROID__
    bo1_android_clean_exit();
    return bo1_android_relaunch(cmd);
#else
    static std::vector<std::string> words;
    static std::vector<char *> args;
    words.clear();
    args.clear();
    std::string all = cmd ? cmd : "";
    all += ';';
    std::string part;
    for (char c : all)
    {
        if (c != ';')
        {
            part += c;
            continue;
        }
        bool first = true;
        size_t i = 0;
        while (i < part.size())
        {
            while (i < part.size() && part[i] == ' ')
                ++i;
            size_t j = i;
            while (j < part.size() && part[j] != ' ')
                ++j;
            if (j > i)
            {
                words.push_back((first ? "+" : "") + part.substr(i, j - i));
                first = false;
            }
            i = j;
        }
        part.clear();
    }
    for (int i = 0; i < s_origArgc; ++i)
        args.push_back(s_origArgv[i]);
    for (std::string &w : words)
        args.push_back(const_cast<char *>(w.c_str()));
    args.push_back(nullptr);
    setenv("T5JOOL_ORIG_ARGC", std::to_string(s_origArgc).c_str(), 1); // per tornare poi al menu senza Versus
    execv("/proc/self/exe", args.data());
    return false;
#endif
}

// T5Jool: mod scelta nel menu (Mods) e salvata in <cartella del gioco>/t5jool_mod.txt. Si applica qui, prima che
// il motore parta: fs_game decide i file caricati (mods/<nome>/mod.ff) e la memoria si prepara all'avvio.
// Un fs_game passato a mano (riga di comando, server) ha la precedenza.
static std::vector<char *> ApplyChosenMod(int argc, char **argv)
{
    std::vector<char *> out(argv, argv + argc);
    for (int i = 1; i + 1 < argc; ++i)
        if (!strcmp(argv[i], "+set") && !strcasecmp(argv[i + 1], "fs_game"))
            return out;
    std::string file = GameDirectory(argc, argv) + "/t5jool_mod.txt";
    FILE *f = fopen(file.c_str(), "r");
    if (!f)
        return out;
    char name[64] = {};
    if (!fgets(name, sizeof(name), f))
        name[0] = 0;
    fclose(f);
    name[strcspn(name, "\r\n")] = 0;
    if (!name[0])
        return out;
    for (const char *c = name; *c; ++c)
        if (!isalnum((unsigned char)*c) && *c != '_' && *c != '-' && *c != '.')
            return out;
    std::string ff = GameDirectory(argc, argv) + "/mods/" + name + "/mod.ff";
    if (access(ff.c_str(), R_OK) != 0)
    {
        fprintf(stderr, "T5Jool: mod '%s' non trovata (%s), si gioca senza\n", name, ff.c_str());
        return out;
    }
    static std::string value;
    value = std::string("mods/") + name;
    fprintf(stderr, "T5Jool: mod %s\n", value.c_str());
    static char setWord[] = "+set", fsGame[] = "fs_game";
    out.insert(out.begin() + 1, { setWord, fsGame, const_cast<char *>(value.c_str()) });
    return out;
}

// T5Jool: salvataggio in solo. Lo script (mods/t5save, copiato nella cartella del gioco con le altre mod del port) si
// aggiunge sempre a fs_mods; da solo non cambia nulla: agisce solo quando il motore mette t5save_active 1 (ui/bo1_save.cpp).
// Non sui server dedicati. Un fs_mods passato a mano resta e t5save si aggiunge in fondo.
static void AddSaveMod(std::vector<char *> &args)
{
    const int argc = (int)args.size();
    int modsAt = -1;
    bool zombies = false; // l'ultimo +set bo1_zombies vince: con 0 (multiplayer) niente mod di Zombies
    for (int i = 1; i + 2 < argc; ++i)
        if (!strcmp(args[i], "+set") && !strcasecmp(args[i + 1], "bo1_zombies"))
            zombies = strcmp(args[i + 2], "0") != 0;
    if (!zombies)
    {
        s_hangWatch = true;
        fprintf(stderr, "T5Jool: avvio multiplayer (bo1_zombies 0): niente salvataggio in solo\n");
        return;
    }
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (!strcmp(args[i], "+set") && !strcasecmp(args[i + 1], "dedicated"))
            return;
        if (!strcmp(args[i], "+set") && !strcasecmp(args[i + 1], "fs_mods") && i + 2 < argc)
            modsAt = i + 2;
    }
    if (getenv("T5_NO_SAVE"))
        return;
    const std::string dir = GameDirectory(argc, args.data());
    if (access((dir + "/mods/t5save/maps/t5save/_hooks.gsc").c_str(), R_OK) != 0
        || access((dir + "/mods/_stack/maps/_modstack.gsc").c_str(), R_OK) != 0)
    {
        fprintf(stderr, "T5Jool: mods/t5save non trovata in %s, niente salvataggio in solo\n", dir.c_str());
        return;
    }
    static std::string value;
    if (modsAt >= 0)
    {
        value = std::string(args[modsAt]) + " t5save";
        args[modsAt] = const_cast<char *>(value.c_str());
        return;
    }
    static char setWord[] = "+set", fsMods[] = "fs_mods", name[] = "t5save";
    args.insert(args.begin() + 1, { setWord, fsMods, name });
}

int main(int argc, char **argv)
{
#ifdef __ANDROID__
    // argomenti --t5-* del lanciatore Java, log su file, gestore dei crash, driver Vulkan personalizzato
    bo1_android_init(&argc, argv);
#else
    // log scritto riga per riga anche quando stdout va su file (autotest)
    setvbuf(stdout, nullptr, _IOLBF, 0);
    atexit(bo1_log_flush); // T5Jool: log della console bufferizzato (platform/win_compat.cpp)
#endif
    s_origArgc = argc;
    s_origArgv = argv;
    if (const char *orig = getenv("T5JOOL_ORIG_ARGC")) // avviato da Versus: gli argomenti in piu' non si ripetono
    {
        const int n = atoi(orig);
        if (n > 0 && n < argc)
            s_origArgc = n;
        unsetenv("T5JOOL_ORIG_ARGC");
    }
    static std::vector<char *> args = ApplyChosenMod(argc, argv);
    AddSaveMod(args);
#ifdef BO1_LOWMEM
    // prova: +set bo1_lowmem_waste N occupa N MB di memoria bassa (riproduce un telefono con meno memoria bassa)
    for (size_t i = 1; i + 2 < args.size(); ++i)
        if (!strcmp(args[i], "+set") && !strcasecmp(args[i + 1], "bo1_lowmem_waste") && atoi(args[i + 2]) > 0)
            bo1_low_waste((size_t)atoi(args[i + 2]));
#endif
    args.push_back(nullptr);
    argc = (int)args.size() - 1;
    argv = args.data();
    bo1_SetCommandLine(argc, argv);
    StartRssWatchdog();
#ifdef BO1_LOWMEM
    if (!bo1_lowmem_init(BO1_LOWMEM_DEFAULT))
    {
        fprintf(stderr, "Impossibile riservare memoria sotto i 2 GB\n");
        return 1;
    }
    const size_t stackSize = 16u << 20;
    void *stack = bo1_low_reserve(stackSize);
    if (!stack || !bo1_low_commit(stack, stackSize)) return 1;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, stack, stackSize);
    MainArgs a{ argc, argv, 1 };
    pthread_t t;
    if (pthread_create(&t, &attr, GameThread, &a) != 0) return 1;
    pthread_attr_destroy(&attr);
    StartInitWatchdog(t);
    pthread_join(t, nullptr);
    return a.code;
#else
    return GameMain(argc, argv);
#endif
}

#endif // !_WIN32

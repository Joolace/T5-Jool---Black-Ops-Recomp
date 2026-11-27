// t5_main.cpp - T5Jool entry point for Linux and Android: SDL, the game folder, the command line,
// then the engine's WinMain. Takes the place of src/nx/nx_main.cpp and nx_display.cpp; windows and
// input go through src/platform/sdl (user32 and XInput over SDL), Direct3D 9 through the bridge
// (src/nx/dxvk/d3d9_bridge.cpp) into DXVK-native.
#include <windows.h>
#include <SDL.h>
#include <SDL_vulkan.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

#include "../nx/nx_display.h"
#include "../nx/nx_install.h"

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char *lpCmdLine, int nCmdShow);
extern "C" void nx_wincompat_init_main_thread(void);
extern "C" void nx_set_command_line(const char *cmdline);
void NxProf_SetThreadName(const char *threadName);
void Sys_BinkInitClock();
void T5_SetAppFocus(bool active);       // t5_platform.cpp
HWND T5_MainWindow();                   // platform/sdl/sdl_user32.cpp
bool T5_PumpWindowMessages();           // platform/sdl/sdl_user32.cpp: true on quit

namespace
{
std::string s_installDir;
std::string s_exePath;
bool s_quit;

std::string Absolute(const std::string &p)
{
    char buf[PATH_MAX];
    return realpath(p.c_str(), buf) ? std::string(buf) : p;
}

// the game folder: --t5-home=<dir>, else $T5JOOL_GAME_DIR / $BO1_GAME_DIR, else the working directory
std::string FindInstallDir(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
        if (!strncmp(argv[i], "--t5-home=", 10))
            return Absolute(argv[i] + 10);
    for (const char *env : { "T5JOOL_GAME_DIR", "BO1_GAME_DIR" })
        if (const char *v = getenv(env); v && *v)
            return Absolute(v);
    char cwd[PATH_MAX];
    return getcwd(cwd, sizeof(cwd)) ? std::string(cwd) : std::string(".");
}

// shader caches next to the game (DXVK and the Vulkan driver), like the Switch build's shadercache/
void ConfigureDxvk()
{
    std::string cache = s_installDir + "/shadercache";
    if (const char *c = getenv("T5JOOL_CACHE_DIR"); c && *c)
        cache = c;
    mkdir(cache.c_str(), 0777);
    setenv("DXVK_STATE_CACHE_PATH", cache.c_str(), 0);
    setenv("DXVK_SHADER_CACHE_PATH", cache.c_str(), 0);
    setenv("MESA_SHADER_CACHE_DIR", cache.c_str(), 0);
    setenv("DXVK_LOG_LEVEL", "warn", 0);
    setenv("DXVK_WSI_DRIVER", "SDL2", 0);
    std::string conf = s_installDir + "/dxvk.conf";
    struct stat st;
    if (stat(conf.c_str(), &st) == 0)
        setenv("DXVK_CONFIG_FILE", conf.c_str(), 0);
}

// "+set a b +map c" for the engine: every argument that is not one of ours, quoted when it has spaces
std::string BuildCommandLine(int argc, char **argv)
{
    std::string cmd = "allowdupe nodump";
    for (int i = 1; i < argc; ++i) {
        if (!strncmp(argv[i], "--t5-", 5))
            continue;
        cmd += ' ';
        if (strchr(argv[i], ' ')) {
            cmd += '"';
            cmd += argv[i];
            cmd += '"';
        } else {
            cmd += argv[i];
        }
    }
    return cmd;
}
}

extern "C" const char *nx_get_install_dir(void)
{
    return s_installDir.c_str();
}

// the swapchain's window for DXVK-native's SDL2 WSI (tools/nx/gen-d3d9-bridge.py passes it to CreateDevice)
void *nwindowGetDefault(void)
{
    return (void *)T5_MainWindow();
}

void T5_ShowFatalError(const char *message)
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "T5Jool", message, nullptr);
}

// Win_GetEvent, once a frame on the main thread: the SDL events become the window's Win32 messages
extern "C" bool NX_PumpAppletMessages(void)
{
    if (T5_PumpWindowMessages())
        s_quit = true;
    return s_quit;
}

void T5_RequestQuit()
{
    s_quit = true;
}

extern "C" void NX_PrepareExit(void) { fflush(stdout); fflush(stderr); }
extern "C" void NX_GlServiceRenderThread(void) {}

// SP <-> MP button: the other executable in the same folder (OpenBLOPS-NX(-MP).nro on the Switch)
extern "C" bool NX_ChainLoadSibling(const char *nroName)
{
    const bool toMp = nroName && strstr(nroName, "-MP");
    std::string dir = s_exePath.substr(0, s_exePath.find_last_of('/') + 1);
    std::string target = dir + (toMp ? "T5Jool-MP" : "T5Jool");
    fflush(stdout);
    std::string home = "--t5-home=" + s_installDir;
    char *args[] = { (char *)target.c_str(), (char *)home.c_str(), nullptr };
    execv(target.c_str(), args);
    fprintf(stderr, "T5Jool: cannot start %s\n", target.c_str());
    return false;
}

// ------------------------------------------------------------------ display (nx_display.h)
void NX_DisplayInit() {}

void NX_DisplayRenderSize(int *width, int *height)
{
    int w = 1280, h = 720;
    if (SDL_Window *win = (SDL_Window *)T5_MainWindow()) {
        SDL_Vulkan_GetDrawableSize(win, &w, &h);
    } else {
        SDL_DisplayMode mode;
        if (SDL_GetDesktopDisplayMode(0, &mode) == 0) {
            w = mode.w;
            h = mode.h;
        }
    }
    *width = w;
    *height = h;
}

bool NX_DisplayRenderSizeChanged(int width, int height)
{
    int w, h;
    NX_DisplayRenderSize(&w, &h);
    return w != width || h != height;
}

int NX_DisplayRefreshRate()
{
    SDL_DisplayMode mode;
    int display = 0;
    if (SDL_Window *win = (SDL_Window *)T5_MainWindow())
        display = SDL_GetWindowDisplayIndex(win);
    if (SDL_GetCurrentDisplayMode(display < 0 ? 0 : display, &mode) == 0 && mode.refresh_rate > 0)
        return mode.refresh_rate;
    return 60;
}

void NX_DisplayUpdate() {}

// ------------------------------------------------------------------ main
int main(int argc, char **argv)
{
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    s_exePath = n > 0 ? std::string(exe, (size_t)n) : std::string(argv[0]);

    s_installDir = FindInstallDir(argc, argv);
    if (chdir(s_installDir.c_str()) != 0) {
        fprintf(stderr, "T5Jool: game folder not found: %s\n", s_installDir.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "T5Jool",
            "Game folder not found. Start T5Jool from your Black Ops folder or pass --t5-home=<folder>.", nullptr);
        return 1;
    }

    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_SENSOR) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Vulkan_LoadLibrary(nullptr);

    ConfigureDxvk();
    Sys_BinkInitClock();
    nx_wincompat_init_main_thread();
    NxProf_SetThreadName("main");

    // Sys_Print writes every message to OutputDebugStringA and to the console buffer: keep one copy
    extern int enable_OutputDebugString;
    enable_OutputDebugString = 0;

    static std::string cmdline;
    cmdline = BuildCommandLine(argc, argv);
#if defined(KISAK_SP)
    printf("T5Jool (Campaign and Zombies) starting in %s\n", s_installDir.c_str());
#else
    printf("T5Jool (Multiplayer) starting in %s\n", s_installDir.c_str());
#endif
    printf("command line: %s\n", cmdline.c_str());
    nx_set_command_line(cmdline.c_str());

    // the engine runs its own loop and leaves through exit() (Com_Quit_f)
    int rc = WinMain((HINSTANCE)1, nullptr, (char *)cmdline.c_str(), SW_SHOWNORMAL);
    SDL_Quit();
    return rc;
}

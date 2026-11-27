// sdl_user32.cpp - User32 (finestre, messaggi, mouse, tastiera, appunti, monitor) sopra SDL2.
//
// Il motore resta com'è: crea la finestra con CreateWindowExA, la svuota con PeekMessage/DispatchMessage
// e riceve l'input in MainWndProc come WM_KEYDOWN/WM_MOUSEMOVE/WM_CHAR... Qui traduciamo gli eventi SDL
// in quei messaggi. HWND = SDL_Window*, che è anche ciò che DXVK-native (WSI SDL2) vuole come finestra.
#if !defined(_WIN32)

#include <windows.h>
#include "../t5_win32_extra.h"

#include <SDL.h>
#include "../t5_touch.h"
void T5_SetAppFocus(bool active); // t5_platform.cpp: the Bink clock stops in background
#include <SDL_vulkan.h>

#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

namespace
{
// ------------------------------------------------------------------------------------------------
// Tabella SDL_Scancode -> virtual key Windows + scancode PC (set 1) + flag "esteso"
// ------------------------------------------------------------------------------------------------
struct KeyInfo { uint8_t vk; uint8_t scan; bool ext; };

KeyInfo KeyFromScancode(SDL_Scancode sc)
{
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
    {
        static const uint8_t scans[26] = { 0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
                                           0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C };
        int i = sc - SDL_SCANCODE_A;
        return { static_cast<uint8_t>('A' + i), scans[i], false };
    }
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) return { static_cast<uint8_t>('1' + (sc - SDL_SCANCODE_1)), static_cast<uint8_t>(0x02 + (sc - SDL_SCANCODE_1)), false };
    if (sc == SDL_SCANCODE_0) return { '0', 0x0B, false };
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F10) return { static_cast<uint8_t>(VK_F1 + (sc - SDL_SCANCODE_F1)), static_cast<uint8_t>(0x3B + (sc - SDL_SCANCODE_F1)), false };
    switch (sc)
    {
    case SDL_SCANCODE_F11: return { VK_F1 + 10, 0x57, false };
    case SDL_SCANCODE_F12: return { VK_F1 + 11, 0x58, false };
    case SDL_SCANCODE_RETURN: return { VK_RETURN, 0x1C, false };
    case SDL_SCANCODE_ESCAPE: return { VK_ESCAPE, 0x01, false };
    case SDL_SCANCODE_BACKSPACE: return { VK_BACK, 0x0E, false };
    case SDL_SCANCODE_TAB: return { VK_TAB, 0x0F, false };
    case SDL_SCANCODE_SPACE: return { VK_SPACE, 0x39, false };
    case SDL_SCANCODE_MINUS: return { VK_OEM_MINUS, 0x0C, false };
    case SDL_SCANCODE_EQUALS: return { VK_OEM_PLUS, 0x0D, false };
    case SDL_SCANCODE_LEFTBRACKET: return { VK_OEM_4, 0x1A, false };
    case SDL_SCANCODE_RIGHTBRACKET: return { VK_OEM_6, 0x1B, false };
    case SDL_SCANCODE_BACKSLASH: return { VK_OEM_5, 0x2B, false };
    case SDL_SCANCODE_SEMICOLON: return { VK_OEM_1, 0x27, false };
    case SDL_SCANCODE_APOSTROPHE: return { VK_OEM_7, 0x28, false };
    case SDL_SCANCODE_GRAVE: return { VK_OEM_3, 0x29, false }; // 0x29 = tasto console (MapKey)
    case SDL_SCANCODE_COMMA: return { VK_OEM_COMMA, 0x33, false };
    case SDL_SCANCODE_PERIOD: return { VK_OEM_PERIOD, 0x34, false };
    case SDL_SCANCODE_SLASH: return { VK_OEM_2, 0x35, false };
    case SDL_SCANCODE_NONUSBACKSLASH: return { VK_OEM_102, 0x56, false };
    case SDL_SCANCODE_CAPSLOCK: return { VK_CAPITAL, 0x3A, false };
    case SDL_SCANCODE_PRINTSCREEN: return { VK_SNAPSHOT, 0x37, true };
    case SDL_SCANCODE_SCROLLLOCK: return { VK_SCROLL, 0x46, false };
    case SDL_SCANCODE_PAUSE: return { VK_PAUSE, 0x45, false };
    case SDL_SCANCODE_INSERT: return { VK_INSERT, 0x52, true };
    case SDL_SCANCODE_HOME: return { VK_HOME, 0x47, true };
    case SDL_SCANCODE_PAGEUP: return { VK_PRIOR, 0x49, true };
    case SDL_SCANCODE_DELETE: return { VK_DELETE, 0x53, true };
    case SDL_SCANCODE_END: return { VK_END, 0x4F, true };
    case SDL_SCANCODE_PAGEDOWN: return { VK_NEXT, 0x51, true };
    case SDL_SCANCODE_RIGHT: return { VK_RIGHT, 0x4D, true };
    case SDL_SCANCODE_LEFT: return { VK_LEFT, 0x4B, true };
    case SDL_SCANCODE_DOWN: return { VK_DOWN, 0x50, true };
    case SDL_SCANCODE_UP: return { VK_UP, 0x48, true };
    case SDL_SCANCODE_NUMLOCKCLEAR: return { VK_NUMLOCK, 0x45, true };
    case SDL_SCANCODE_KP_DIVIDE: return { VK_DIVIDE, 0x35, true };
    case SDL_SCANCODE_KP_MULTIPLY: return { VK_MULTIPLY, 0x37, false };
    case SDL_SCANCODE_KP_MINUS: return { VK_SUBTRACT, 0x4A, false };
    case SDL_SCANCODE_KP_PLUS: return { VK_ADD, 0x4E, false };
    case SDL_SCANCODE_KP_ENTER: return { VK_RETURN, 0x1C, true };
    case SDL_SCANCODE_KP_1: return { VK_NUMPAD0 + 1, 0x4F, false };
    case SDL_SCANCODE_KP_2: return { VK_NUMPAD0 + 2, 0x50, false };
    case SDL_SCANCODE_KP_3: return { VK_NUMPAD0 + 3, 0x51, false };
    case SDL_SCANCODE_KP_4: return { VK_NUMPAD0 + 4, 0x4B, false };
    case SDL_SCANCODE_KP_5: return { VK_NUMPAD0 + 5, 0x4C, false };
    case SDL_SCANCODE_KP_6: return { VK_NUMPAD0 + 6, 0x4D, false };
    case SDL_SCANCODE_KP_7: return { VK_NUMPAD0 + 7, 0x47, false };
    case SDL_SCANCODE_KP_8: return { VK_NUMPAD0 + 8, 0x48, false };
    case SDL_SCANCODE_KP_9: return { VK_NUMPAD0 + 9, 0x49, false };
    case SDL_SCANCODE_KP_0: return { VK_NUMPAD0, 0x52, false };
    case SDL_SCANCODE_KP_PERIOD: return { VK_DECIMAL, 0x53, false };
    case SDL_SCANCODE_APPLICATION: return { VK_APPS, 0x5D, true };
    case SDL_SCANCODE_LCTRL: return { VK_CONTROL, 0x1D, false };
    case SDL_SCANCODE_LSHIFT: return { VK_SHIFT, 0x2A, false };
    case SDL_SCANCODE_LALT: return { VK_MENU, 0x38, false };
    case SDL_SCANCODE_LGUI: return { VK_LWIN, 0x5B, true };
    case SDL_SCANCODE_RCTRL: return { VK_CONTROL, 0x1D, true };
    case SDL_SCANCODE_RSHIFT: return { VK_SHIFT, 0x36, false };
    case SDL_SCANCODE_RALT: return { VK_MENU, 0x38, true };
    case SDL_SCANCODE_RGUI: return { VK_RWIN, 0x5C, true };
    case SDL_SCANCODE_AC_BACK: return { VK_ESCAPE, 0x01, false }; // tasto "indietro" di Android
    default: return { 0, 0, false };
    }
}

SDL_Scancode ScancodeFromVk(int vk)
{
    for (int sc = 0; sc < SDL_NUM_SCANCODES; ++sc)
    {
        KeyInfo k = KeyFromScancode(static_cast<SDL_Scancode>(sc));
        if (k.vk == vk) return static_cast<SDL_Scancode>(sc);
    }
    return SDL_SCANCODE_UNKNOWN;
}

// ------------------------------------------------------------------------------------------------
// Finestre registrate
// ------------------------------------------------------------------------------------------------
struct WindowData
{
    SDL_Window *sdl = nullptr; // nullptr per le finestre "finte" (controlli della console di sistema)
    WNDPROC proc = nullptr;
    LONG style = 0;
    LONG exStyle = 0;
    std::string text;
};

std::mutex g_lock;
std::map<std::string, WNDPROC> g_classes;
std::unordered_map<HWND, WindowData> g_windows;
std::deque<MSG> g_queue;
HWND g_mainWindow = nullptr;
HWND g_focus = nullptr;
bool g_quitPosted = false;
int g_quitCode = 0;

// Cursore "virtuale" in coordinate schermo: in modalità relativa (gioco) SDL non dà una posizione assoluta,
// quindi la ricostruiamo sommando gli spostamenti; il motore la ricentra con SetCursorPos ogni frame.
int g_cursorX = 0, g_cursorY = 0;
int g_showCursorCount = 0;
bool g_relative = false;
WPARAM g_buttonState = 0;

std::string g_clipboard;

WindowData *Find(HWND wnd)
{
    auto it = g_windows.find(wnd);
    return it == g_windows.end() ? nullptr : &it->second;
}

LPARAM MakeLParam(int lo, int hi) { return static_cast<LPARAM>((static_cast<uint32_t>(hi & 0xFFFF) << 16) | static_cast<uint32_t>(lo & 0xFFFF)); }

void Post(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    MSG m{};
    m.hwnd = wnd;
    m.message = msg;
    m.wParam = wp;
    m.lParam = lp;
    m.time = SDL_GetTicks();
    m.pt.x = g_cursorX;
    m.pt.y = g_cursorY;
    g_queue.push_back(m);
}

void UpdateRelativeMode()
{
    // Cursore nascosto + finestra attiva = il gioco vuole il "mouse look": modalità relativa di SDL.
    bool want = g_showCursorCount < 0 && g_mainWindow && g_focus == g_mainWindow;
    if (want != g_relative)
    {
        g_relative = want;
        SDL_SetRelativeMouseMode(want ? SDL_TRUE : SDL_FALSE);
    }
    SDL_ShowCursor(g_showCursorCount >= 0 ? SDL_ENABLE : SDL_DISABLE);
}

// T5Jool (Android): con T5_RENDER_SCALE il gioco disegna piu' piccolo dello schermo; le coordinate del mouse/tocco
// e l'area client si riportano alla dimensione del back buffer. La finestra SDL puo' essere stata ridimensionata
// (DXVK in finestra) oppure no: il fattore si calcola ogni volta dalla dimensione che SDL riporta.
float RenderScaleEnv()
{
#ifdef __ANDROID__
    static const float s = [] {
        const char *v = getenv("T5_RENDER_SCALE");
        float f = v ? (float)atof(v) : 1.0f;
        return (f > 0.3f && f < 0.99f) ? f : 1.0f;
    }();
    return s;
#else
    return 1.0f;
#endif
}

// dimensione del back buffer come la calcola R_SetWndParms (schermo * scala, pari); false se non serve
bool RenderTargetSize(int *w, int *h)
{
    const float s = RenderScaleEnv();
    if (s >= 1.0f)
        return false;
    SDL_DisplayMode m{};
    if (SDL_GetDesktopDisplayMode(0, &m) != 0 || m.w <= 0 || m.h <= 0)
        return false;
    *w = ((int)((float)m.w * s) + 1) & ~1;
    *h = ((int)((float)m.h * s) + 1) & ~1;
    return true;
}

float InputScale(SDL_Window *win = nullptr)
{
    int tw, th;
    if (!RenderTargetSize(&tw, &th))
        return 1.0f;
    int ww = 0, wh = 0;
    if (win)
        SDL_GetWindowSize(win, &ww, &wh);
    else if (SDL_Window *f = SDL_GetMouseFocus())
        SDL_GetWindowSize(f, &ww, &wh);
    return ww > 0 ? (float)tw / (float)ww : RenderScaleEnv();
}

void WindowOrigin(HWND wnd, int *x, int *y)
{
    *x = *y = 0;
    WindowData *w = Find(wnd);
    if (w && w->sdl) SDL_GetWindowPosition(w->sdl, x, y);
}

WPARAM ButtonFlags(uint32_t sdlMask)
{
    WPARAM w = 0;
    if (sdlMask & SDL_BUTTON_LMASK) w |= MK_LBUTTON;
    if (sdlMask & SDL_BUTTON_RMASK) w |= MK_RBUTTON;
    if (sdlMask & SDL_BUTTON_MMASK) w |= MK_MBUTTON;
    if (sdlMask & SDL_BUTTON_X1MASK) w |= MK_XBUTTON1;
    if (sdlMask & SDL_BUTTON_X2MASK) w |= MK_XBUTTON2;
    return w;
}

void PostChar(HWND wnd, unsigned int ch) { Post(wnd, WM_CHAR, ch, 1); }

// Traduce un evento SDL in zero o più messaggi Win32 nella coda.

void Translate(const SDL_Event &e)
{
    HWND wnd = g_mainWindow;
    // comandi touch (Android): in partita il tocco va ai comandi e il mouse che SDL simula dal tocco si ignora
    if (BO1Touch_HandleEvent(&e))
        return;
    if ((e.type == SDL_MOUSEMOTION && e.motion.which == SDL_TOUCH_MOUSEID)
        || ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && e.button.which == SDL_TOUCH_MOUSEID))
    {
        if (BO1Touch_DropTouchMouse(&e))
            return;
    }
#ifdef __ANDROID__
    // T5Jool: mouse o tastiera veri in uso -> i comandi touch si nascondono, come con un controller
    if ((e.type == SDL_KEYDOWN && e.key.keysym.sym != SDLK_AC_BACK && e.key.keysym.scancode < SDL_SCANCODE_AUDIONEXT)
        || (e.type == SDL_MOUSEMOTION && e.motion.which != SDL_TOUCH_MOUSEID)
        || ((e.type == SDL_MOUSEBUTTONDOWN) && e.button.which != SDL_TOUCH_MOUSEID))
        BO1Touch_PhysicalPadUsed();
#endif
    switch (e.type)
    {
    case SDL_QUIT:
        if (wnd) Post(wnd, WM_CLOSE, 0, 0);
        break;

    case SDL_WINDOWEVENT:
        switch (e.window.event)
        {
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            T5_SetAppFocus(true);
            g_focus = wnd;
            Post(wnd, WM_ACTIVATE, WA_ACTIVE, 0);
            Post(wnd, WM_SETFOCUS, 0, 0);
            break;
        case SDL_WINDOWEVENT_FOCUS_LOST:
            T5_SetAppFocus(false);
            g_focus = nullptr;
            Post(wnd, WM_ACTIVATE, WA_INACTIVE, 0);
            Post(wnd, WM_KILLFOCUS, 0, 0);
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            Post(wnd, WM_ACTIVATE, static_cast<WPARAM>(WA_INACTIVE | (1 << 16)), 0);
            break;
        case SDL_WINDOWEVENT_MOVED:
            Post(wnd, WM_MOVE, 0, MakeLParam(e.window.data1, e.window.data2));
            break;
        case SDL_WINDOWEVENT_SIZE_CHANGED:
            Post(wnd, WM_SIZE, 0, MakeLParam(e.window.data1, e.window.data2));
            break;
        case SDL_WINDOWEVENT_CLOSE:
            Post(wnd, WM_CLOSE, 0, 0);
            break;
        default:
            break;
        }
        UpdateRelativeMode();
        break;

    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        KeyInfo k = KeyFromScancode(e.key.keysym.scancode);
        if (!k.vk) break;
        bool down = e.type == SDL_KEYDOWN;
        bool alt = (e.key.keysym.mod & KMOD_ALT) != 0;
        LPARAM lp = 1 | (static_cast<LPARAM>(k.scan) << 16) | (k.ext ? (1 << 24) : 0);
        if (alt) lp |= 1 << 29;
        if (!down) lp |= (1u << 30) | (1u << 31);
        else if (e.key.repeat) lp |= 1 << 30;
        UINT msg = down ? (alt || k.vk == VK_F1 + 9 ? WM_SYSKEYDOWN : WM_KEYDOWN) : (alt || k.vk == VK_F1 + 9 ? WM_SYSKEYUP : WM_KEYUP);
        Post(wnd, msg, k.vk, lp);
        // WM_CHAR che su Windows nasce da TranslateMessage e che SDL_TEXTINPUT non dà: tasti di controllo
        if (down)
        {
            if (k.vk == VK_BACK) PostChar(wnd, 8);
            else if (k.vk == VK_RETURN) PostChar(wnd, 13);
            else if (k.vk == VK_TAB) PostChar(wnd, 9);
            else if (k.vk == VK_ESCAPE) PostChar(wnd, 27);
            else if ((e.key.keysym.mod & KMOD_CTRL) && k.vk >= 'A' && k.vk <= 'Z') PostChar(wnd, static_cast<unsigned>(k.vk - 'A' + 1));
        }
        break;
    }

    case SDL_TEXTINPUT:
    {
        // UTF-8 -> caratteri: il motore gestisce solo Latin-1 in WM_CHAR
        const unsigned char *p = reinterpret_cast<const unsigned char *>(e.text.text);
        while (*p)
        {
            uint32_t cp; int extra;
            if (*p < 0x80) { cp = *p; extra = 0; }
            else if ((*p >> 5) == 6) { cp = *p & 0x1F; extra = 1; }
            else if ((*p >> 4) == 14) { cp = *p & 0x0F; extra = 2; }
            else { cp = *p & 0x07; extra = 3; }
            ++p;
            for (int i = 0; i < extra && *p; ++i, ++p) cp = (cp << 6) | (*p & 0x3F);
            if (cp >= 32 && cp < 256 && cp != 127) PostChar(wnd, cp);
        }
        break;
    }

    case SDL_MOUSEMOTION:
        // T5Jool: il mouse simulato dal tocco ha sempre una posizione vera sullo schermo, anche in modo relativo
        if (g_relative && e.motion.which != SDL_TOUCH_MOUSEID)
        {
            g_cursorX += e.motion.xrel;
            g_cursorY += e.motion.yrel;
        }
        else
        {
            int ox, oy;
            WindowOrigin(wnd, &ox, &oy);
            const float sc = InputScale(SDL_GetWindowFromID(e.motion.windowID));
            g_cursorX = ox + (int)((float)e.motion.x * sc);
            g_cursorY = oy + (int)((float)e.motion.y * sc);
        }
        g_buttonState = ButtonFlags(e.motion.state);
        {
            int ox, oy;
            WindowOrigin(wnd, &ox, &oy);
            Post(wnd, WM_MOUSEMOVE, g_buttonState, MakeLParam(g_cursorX - ox, g_cursorY - oy));
        }
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    {
        bool down = e.type == SDL_MOUSEBUTTONDOWN;
        UINT msg;
        WPARAM xbtn = 0;
        switch (e.button.button)
        {
        case SDL_BUTTON_LEFT: msg = down ? WM_LBUTTONDOWN : WM_LBUTTONUP; break;
        case SDL_BUTTON_RIGHT: msg = down ? WM_RBUTTONDOWN : WM_RBUTTONUP; break;
        case SDL_BUTTON_MIDDLE: msg = down ? WM_MBUTTONDOWN : WM_MBUTTONUP; break;
        case SDL_BUTTON_X1: msg = down ? WM_XBUTTONDOWN : WM_XBUTTONUP; xbtn = 1u << 16; break;
        default: msg = down ? WM_XBUTTONDOWN : WM_XBUTTONUP; xbtn = 2u << 16; break;
        }
        g_buttonState = ButtonFlags(SDL_GetMouseState(nullptr, nullptr));
        if (!g_relative || e.button.which == SDL_TOUCH_MOUSEID)
        {
            int bx, by;
            WindowOrigin(wnd, &bx, &by);
            const float sc = InputScale(SDL_GetWindowFromID(e.button.windowID));
            g_cursorX = bx + (int)((float)e.button.x * sc);
            g_cursorY = by + (int)((float)e.button.y * sc);
        }
        int ox, oy;
        WindowOrigin(wnd, &ox, &oy);
        Post(wnd, msg, g_buttonState | xbtn, MakeLParam(g_cursorX - ox, g_cursorY - oy));
        break;
    }

    case SDL_MOUSEWHEEL:
    {
        int delta = e.wheel.y * 120;
        if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) delta = -delta;
        if (delta) Post(wnd, WM_MOUSEWHEEL, g_buttonState | (static_cast<WPARAM>(static_cast<uint16_t>(delta)) << 16), MakeLParam(g_cursorX, g_cursorY));
        break;
    }

    case SDL_APP_WILLENTERBACKGROUND:
        g_focus = nullptr;
        Post(wnd, WM_ACTIVATE, WA_INACTIVE, 0);
        break;
    case SDL_APP_DIDENTERFOREGROUND:
        g_focus = wnd;
        Post(wnd, WM_ACTIVATE, WA_ACTIVE, 0);
        break;

    default:
        break;
    }
}

bool PumpSdl(bool wait)
{
    SDL_Event e;
    bool any = false;
    if (wait && g_queue.empty())
    {
        if (SDL_WaitEvent(&e)) { Translate(e); any = true; }
    }
    while (SDL_PollEvent(&e)) { Translate(e); any = true; }
    return any;
}

bool TakeMessage(LPMSG msg, HWND wnd, bool remove)
{
    for (auto it = g_queue.begin(); it != g_queue.end(); ++it)
    {
        if (wnd && it->hwnd != wnd) continue;
        *msg = *it;
        if (remove) g_queue.erase(it);
        return true;
    }
    return false;
}
} // namespace

// C++ linkage, as declared by src/nx/compat/windows.h

// ------------------------------------------------------------------------------------------------
// Classi e finestre
// ------------------------------------------------------------------------------------------------
WORD RegisterClassA(const WNDCLASSA *wc)
{
    std::lock_guard<std::mutex> l(g_lock);
    g_classes[wc->lpszClassName] = wc->lpfnWndProc;
    return 1;
}

WORD RegisterClassExA(const WNDCLASSEXA *wc)
{
    std::lock_guard<std::mutex> l(g_lock);
    g_classes[wc->lpszClassName] = wc->lpfnWndProc;
    return 1;
}

BOOL UnregisterClassA(LPCSTR name, HINSTANCE)
{
    std::lock_guard<std::mutex> l(g_lock);
    return g_classes.erase(name) ? TRUE : FALSE;
}

HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR title, DWORD style, int x, int y, int w, int h, HWND parent, HMENU, HINSTANCE, LPVOID)
{
    WNDPROC proc = nullptr;
    {
        std::lock_guard<std::mutex> l(g_lock);
        auto it = g_classes.find(className ? className : "");
        if (it != g_classes.end()) proc = it->second;
    }

    // Solo la finestra di gioco diventa una vera finestra SDL. Console di sistema, splash e controlli
    // ("Static", "Edit", "Button") restano finte: handle validi ma invisibili.
    bool isGame = className && strcmp(className, "CoDBlackOps") == 0 && !parent;
    HWND hwnd;
    WindowData data;
    data.proc = proc;
    data.style = static_cast<LONG>(style);
    data.exStyle = static_cast<LONG>(exStyle);
    data.text = title ? title : "";
    if (isGame)
    {
        Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI;
        flags |= (style & WS_VISIBLE) ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN;
#ifdef __ANDROID__
        flags |= SDL_WINDOW_FULLSCREEN;
#endif
        if (style & WS_POPUP) flags |= SDL_WINDOW_BORDERLESS;
        int px = x == CW_USEDEFAULT ? SDL_WINDOWPOS_CENTERED : x;
        int py = y == CW_USEDEFAULT ? SDL_WINDOWPOS_CENTERED : y;
        SDL_Window *win = SDL_CreateWindow("T5Jool", px, py, w > 0 ? w : 1280, h > 0 ? h : 720, flags);
        if (!win)
        {
            fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
            return nullptr;
        }
        data.sdl = win;
        hwnd = ((HWND)(win));
    }
    else
    {
        hwnd = ((HWND)(new char[1])); // indirizzo unico qualsiasi
    }

    {
        std::lock_guard<std::mutex> l(g_lock);
        g_windows[hwnd] = data;
        if (isGame) { g_mainWindow = hwnd; g_focus = hwnd; }
    }
    if (proc) proc(hwnd, WM_CREATE, 0, 0);
    if (isGame && proc)
    {
        // T5Jool: active from the start, as on Switch (src/nx/nx_winuser.cpp). The engine reads input
        // only while active (IN_Activate) and SDL does not always report the first focus.
        proc(hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
        proc(hwnd, WM_SETFOCUS, 0, 0);
    }
    if (isGame)
    {
#ifdef __ANDROID__
        // Android: SDL_StartTextInput apre la tastiera a schermo; i tasti fisici e il gamepad arrivano lo stesso
        SDL_StopTextInput();
#else
        SDL_StartTextInput();
#endif
        UpdateRelativeMode();
    }
    return hwnd;
}

BOOL DestroyWindow(HWND wnd)
{
    WindowData data;
    {
        std::lock_guard<std::mutex> l(g_lock);
        WindowData *w = Find(wnd);
        if (!w) return FALSE;
        data = *w;
    }
    if (data.proc) data.proc(wnd, WM_DESTROY, 0, 0);
    {
        std::lock_guard<std::mutex> l(g_lock);
        g_windows.erase(wnd);
        if (wnd == g_mainWindow) { g_mainWindow = nullptr; g_focus = nullptr; }
    }
    if (data.sdl) SDL_DestroyWindow(data.sdl);
    else delete[] (char *)wnd;
    return TRUE;
}

BOOL CloseWindow(HWND wnd)
{
    WindowData *w = Find(wnd);
    if (w && w->sdl) SDL_MinimizeWindow(w->sdl);
    return TRUE;
}

BOOL ShowWindow(HWND wnd, int cmd)
{
    WindowData *w = Find(wnd);
    if (!w || !w->sdl) return FALSE;
    switch (cmd)
    {
    case SW_HIDE: SDL_HideWindow(w->sdl); break;
    case SW_MINIMIZE: SDL_MinimizeWindow(w->sdl); break;
    case SW_RESTORE: SDL_RestoreWindow(w->sdl); SDL_ShowWindow(w->sdl); break;
    default: SDL_ShowWindow(w->sdl); SDL_RaiseWindow(w->sdl); break;
    }
    return TRUE;
}

BOOL UpdateWindow(HWND) { return TRUE; }

BOOL SetWindowPos(HWND wnd, HWND, int x, int y, int w, int h, UINT flags)
{
    WindowData *d = Find(wnd);
    if (!d || !d->sdl) return FALSE;
    if (!(flags & SWP_NOMOVE)) SDL_SetWindowPosition(d->sdl, x, y);
    if (!(flags & SWP_NOSIZE) && w > 0 && h > 0) SDL_SetWindowSize(d->sdl, w, h);
    if (flags & SWP_SHOWWINDOW) SDL_ShowWindow(d->sdl);
    return TRUE;
}

BOOL MoveWindow(HWND wnd, int x, int y, int w, int h, BOOL) { return SetWindowPos(wnd, nullptr, x, y, w, h, SWP_NOZORDER); }

BOOL GetClientRect(HWND wnd, LPRECT rc)
{
    rc->left = rc->top = 0;
    WindowData *d = Find(wnd);
    int w = 0, h = 0;
    if (d && d->sdl) SDL_Vulkan_GetDrawableSize(d->sdl, &w, &h);
    RenderTargetSize(&w, &h); // Android con scala di rendering: l'area client e' il back buffer
    rc->right = w;
    rc->bottom = h;
    return d != nullptr;
}

BOOL GetWindowRect(HWND wnd, LPRECT rc)
{
    WindowData *d = Find(wnd);
    int x = 0, y = 0, w = 0, h = 0;
    if (d && d->sdl)
    {
        SDL_GetWindowPosition(d->sdl, &x, &y);
        SDL_GetWindowSize(d->sdl, &w, &h);
    }
    rc->left = x; rc->top = y; rc->right = x + w; rc->bottom = y + h;
    return d != nullptr;
}

BOOL AdjustWindowRect(LPRECT, DWORD, BOOL) { return TRUE; }      // SDL misura già l'area client
BOOL AdjustWindowRectEx(LPRECT, DWORD, BOOL, DWORD) { return TRUE; }

BOOL ClientToScreen(HWND wnd, LPPOINT pt)
{
    int ox, oy;
    WindowOrigin(wnd, &ox, &oy);
    pt->x += ox; pt->y += oy;
    return TRUE;
}

BOOL ScreenToClient(HWND wnd, LPPOINT pt)
{
    int ox, oy;
    WindowOrigin(wnd, &ox, &oy);
    pt->x -= ox; pt->y -= oy;
    return TRUE;
}

BOOL IsWindow(HWND wnd) { std::lock_guard<std::mutex> l(g_lock); return Find(wnd) != nullptr; }
BOOL IsWindowVisible(HWND wnd)
{
    WindowData *d = Find(wnd);
    return d && d->sdl && (SDL_GetWindowFlags(d->sdl) & SDL_WINDOW_SHOWN);
}
BOOL IsIconic(HWND wnd)
{
    WindowData *d = Find(wnd);
    return d && d->sdl && (SDL_GetWindowFlags(d->sdl) & SDL_WINDOW_MINIMIZED);
}

HWND SetFocus(HWND wnd)
{
    HWND prev = g_focus;
    WindowData *d = Find(wnd);
    if (d && d->sdl) SDL_RaiseWindow(d->sdl);
    return prev;
}
HWND GetFocus(void) { return g_focus; }
BOOL SetForegroundWindow(HWND wnd) { SetFocus(wnd); return TRUE; }
HWND GetActiveWindow(void) { return g_focus; }
HWND GetForegroundWindow(void) { return g_focus; }
HWND GetDesktopWindow(void) { return nullptr; }

LONG GetWindowLongA(HWND wnd, int index)
{
    WindowData *d = Find(wnd);
    if (!d) return 0;
    if (index == GWL_STYLE) return d->style;
    if (index == GWL_EXSTYLE) return d->exStyle;
    return 0;
}

LONG SetWindowLongA(HWND wnd, int index, LONG value)
{
    WindowData *d = Find(wnd);
    if (!d) return 0;
    LONG prev = 0;
    if (index == GWL_STYLE)
    {
        prev = d->style;
        d->style = value;
        if (d->sdl) SDL_SetWindowBordered(d->sdl, (value & WS_POPUP) ? SDL_FALSE : SDL_TRUE);
    }
    else if (index == GWL_EXSTYLE)
    {
        prev = d->exStyle;
        d->exStyle = value;
    }
    return prev;
}

int GetWindowTextA(HWND wnd, LPSTR buf, int max)
{
    WindowData *d = Find(wnd);
    if (!d || max <= 0) return 0;
    snprintf(buf, static_cast<size_t>(max), "%s", d->text.c_str());
    return static_cast<int>(strlen(buf));
}

BOOL SetWindowTextA(HWND wnd, LPCSTR text)
{
    WindowData *d = Find(wnd);
    if (!d) return FALSE;
    d->text = text ? text : "";
    if (d->sdl) SDL_SetWindowTitle(d->sdl, "T5Jool"); // nome della mod
    return TRUE;
}

LRESULT DefWindowProcA(HWND, UINT, WPARAM, LPARAM) { return 0; }
LRESULT CallWindowProcA(WNDPROC proc, HWND wnd, UINT msg, WPARAM wp, LPARAM lp) { return proc ? proc(wnd, msg, wp, lp) : 0; }

LRESULT SendMessageA(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    WindowData *d = Find(wnd);
    return (d && d->proc) ? d->proc(wnd, msg, wp, lp) : 0;
}

BOOL PostMessageA(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    std::lock_guard<std::mutex> l(g_lock);
    Post(wnd, msg, wp, lp);
    return TRUE;
}

void PostQuitMessage(int code)
{
    std::lock_guard<std::mutex> l(g_lock);
    g_quitPosted = true;
    g_quitCode = code;
}

UINT RegisterWindowMessageA(LPCSTR name)
{
    static std::map<std::string, UINT> ids;
    static UINT next = 0xC000;
    std::lock_guard<std::mutex> l(g_lock);
    auto it = ids.find(name);
    if (it != ids.end()) return it->second;
    return ids[name] = next++;
}

BOOL EnumWindows(WNDENUMPROC fn, LPARAM lp)
{
    if (g_mainWindow) fn(g_mainWindow, lp);
    return TRUE;
}

HICON LoadIconA(HINSTANCE, LPCSTR) { return nullptr; }
HANDLE LoadImageA(HINSTANCE, LPCSTR, UINT, int, int, UINT) { return nullptr; }
HCURSOR LoadCursorA(HINSTANCE, LPCSTR name) { return (HCURSOR)const_cast<char *>(name); }
HCURSOR SetCursor(HCURSOR c) { return c; }

// ------------------------------------------------------------------------------------------------
// Coda messaggi
// ------------------------------------------------------------------------------------------------
BOOL PeekMessageA(LPMSG msg, HWND wnd, UINT, UINT, UINT remove)
{
    // Gli eventi SDL vanno letti dal thread che ha creato la finestra: è lo stesso che chiama PeekMessage.
    std::lock_guard<std::mutex> l(g_lock);
    if (g_queue.empty()) PumpSdl(false);
    if (TakeMessage(msg, wnd, (remove & PM_REMOVE) != 0)) return TRUE;
    if (g_quitPosted)
    {
        memset(msg, 0, sizeof(*msg));
        msg->message = WM_QUIT;
        msg->wParam = static_cast<WPARAM>(g_quitCode);
        if (remove & PM_REMOVE) g_quitPosted = false;
        return TRUE;
    }
    return FALSE;
}

BOOL GetMessageA(LPMSG msg, HWND wnd, UINT, UINT)
{
    for (;;)
    {
        {
            std::lock_guard<std::mutex> l(g_lock);
            if (g_quitPosted)
            {
                memset(msg, 0, sizeof(*msg));
                msg->message = WM_QUIT;
                msg->wParam = static_cast<WPARAM>(g_quitCode);
                g_quitPosted = false;
                return FALSE;
            }
            if (TakeMessage(msg, wnd, true)) return TRUE;
            PumpSdl(true);
            if (TakeMessage(msg, wnd, true)) return TRUE;
        }
    }
}

BOOL TranslateMessage(const MSG *) { return TRUE; } // i WM_CHAR li genera già Translate()

LRESULT DispatchMessageA(const MSG *msg)
{
    WNDPROC proc = nullptr;
    {
        std::lock_guard<std::mutex> l(g_lock);
        WindowData *d = Find(msg->hwnd);
        if (d) proc = d->proc;
    }
    return proc ? proc(msg->hwnd, msg->message, msg->wParam, msg->lParam) : 0;
}

HHOOK SetWindowsHookExA(int, HOOKPROC, HINSTANCE, DWORD) { return nullptr; }
BOOL UnhookWindowsHookEx(HHOOK) { return TRUE; }
LRESULT CallNextHookEx(HHOOK, int, WPARAM, LPARAM) { return 0; }

// ------------------------------------------------------------------------------------------------
// Monitor
// ------------------------------------------------------------------------------------------------
HMONITOR MonitorFromWindow(HWND wnd, DWORD)
{
    WindowData *d = Find(wnd);
    int idx = (d && d->sdl) ? SDL_GetWindowDisplayIndex(d->sdl) : 0;
    if (idx < 0) idx = 0;
    return reinterpret_cast<HMONITOR>(static_cast<intptr_t>(idx + 1));
}

HMONITOR MonitorFromPoint(POINT pt, DWORD)
{
    int n = SDL_GetNumVideoDisplays();
    for (int i = 0; i < n; ++i)
    {
        SDL_Rect r;
        if (SDL_GetDisplayBounds(i, &r) == 0 && pt.x >= r.x && pt.x < r.x + r.w && pt.y >= r.y && pt.y < r.y + r.h)
            return reinterpret_cast<HMONITOR>(static_cast<intptr_t>(i + 1));
    }
    return reinterpret_cast<HMONITOR>(static_cast<intptr_t>(1));
}

BOOL GetMonitorInfoA(HMONITOR mon, LPMONITORINFO info)
{
    int idx = static_cast<int>(reinterpret_cast<intptr_t>(mon)) - 1;
    SDL_Rect r{}, u{};
    if (SDL_GetDisplayBounds(idx, &r) != 0) return FALSE;
    if (SDL_GetDisplayUsableBounds(idx, &u) != 0) u = r;
    info->rcMonitor = { r.x, r.y, r.x + r.w, r.y + r.h };
    info->rcWork = { u.x, u.y, u.x + u.w, u.y + u.h };
    info->dwFlags = idx == 0 ? MONITORINFOF_PRIMARY : 0;
    // MONITORINFOEXA: nome del dispositivo, usato poi da EnumDisplaySettingsA
    if (info->cbSize >= sizeof(MONITORINFOEXA))
        snprintf(reinterpret_cast<LPMONITORINFOEXA>(info)->szDevice, 32, "\\\\.\\DISPLAY%d", idx + 1);
    return TRUE;
}

BOOL EnumDisplaySettingsA(LPCSTR device, DWORD modeNum, DEVMODEA *mode)
{
    int idx = 0;
    if (device)
    {
        const char *n = strstr(device, "DISPLAY");
        if (n) idx = atoi(n + 7) - 1;
    }
    SDL_DisplayMode m{};
    int ok;
    if (modeNum == ENUM_CURRENT_SETTINGS || modeNum == ENUM_REGISTRY_SETTINGS)
        ok = SDL_GetCurrentDisplayMode(idx, &m) == 0;
    else
        ok = static_cast<int>(modeNum) < SDL_GetNumDisplayModes(idx) && SDL_GetDisplayMode(idx, static_cast<int>(modeNum), &m) == 0;
    if (!ok) return FALSE;
    memset(mode, 0, sizeof(*mode));
    mode->dmSize = sizeof(*mode);
    mode->dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
    mode->dmBitsPerPel = 32;
    mode->dmPelsWidth = static_cast<DWORD>(m.w);
    mode->dmPelsHeight = static_cast<DWORD>(m.h);
    mode->dmDisplayFrequency = static_cast<DWORD>(m.refresh_rate ? m.refresh_rate : 60);
    return TRUE;
}

BOOL EnumDisplayMonitors(HDC, LPCRECT, MONITORENUMPROC fn, LPARAM lp)
{
    int n = SDL_GetNumVideoDisplays();
    for (int i = 0; i < n; ++i)
    {
        SDL_Rect r;
        SDL_GetDisplayBounds(i, &r);
        RECT rc = { r.x, r.y, r.x + r.w, r.y + r.h };
        if (!fn(reinterpret_cast<HMONITOR>(static_cast<intptr_t>(i + 1)), nullptr, &rc, lp)) break;
    }
    return TRUE;
}

int GetSystemMetrics(int index)
{
    SDL_DisplayMode m{};
    SDL_GetDesktopDisplayMode(0, &m);
    switch (index)
    {
    case SM_CXSCREEN: return m.w;
    case SM_CYSCREEN: return m.h;
    case SM_CMONITORS: return SDL_GetNumVideoDisplays();
    default: return 0;
    }
}

BOOL SystemParametersInfoA(UINT, UINT, PVOID pv, UINT)
{
    if (pv) memset(pv, 0, sizeof(BOOL));
    return TRUE;
}
BOOL IsProcessDPIAware(void) { return TRUE; }
BOOL SetProcessDPIAware(void) { return TRUE; }
LONG ChangeDisplaySettingsA(LPDEVMODEA, DWORD) { return 0; }
DWORD SetThreadExecutionState(DWORD flags)
{
    if (flags & ES_DISPLAY_REQUIRED) SDL_DisableScreenSaver();
    else SDL_EnableScreenSaver();
    return ES_CONTINUOUS;
}

// ------------------------------------------------------------------------------------------------
// Mouse e tastiera
// ------------------------------------------------------------------------------------------------
// mira dei comandi touch: spostamento relativo del cursore, come un mouse vero in partita
extern "C" void bo1_inject_mouse_delta(int dx, int dy)
{
    g_cursorX += dx;
    g_cursorY += dy;
}

BOOL GetCursorPos(LPPOINT pt)
{
    BO1Touch_FlushLook(); // T5Jool: the touch aim, smoothed, right before the game reads the mouse
#ifndef __ANDROID__
    // T5Jool: su Android SDL_GetGlobalMouseState non conosce il tocco (restava 0,0): vale la posizione presa dagli
    // eventi SDL_MOUSEMOTION / SDL_MOUSEBUTTONDOWN (anche quelli che SDL crea dal tocco)
    if (!g_relative)
#else
    if (false)
#endif
    {
        int gx, gy;
        SDL_GetGlobalMouseState(&gx, &gy);
        const float sc = InputScale();
        g_cursorX = (int)((float)gx * sc);
        g_cursorY = (int)((float)gy * sc);
    }
    pt->x = g_cursorX;
    pt->y = g_cursorY;
    return TRUE;
}

BOOL SetCursorPos(int x, int y)
{
    g_cursorX = x;
    g_cursorY = y;
    if (!g_relative && g_mainWindow)
    {
        int ox, oy;
        WindowOrigin(g_mainWindow, &ox, &oy);
        SDL_WarpMouseInWindow(((SDL_Window *)g_mainWindow), x - ox, y - oy);
    }
    return TRUE;
}

int ShowCursor(BOOL show)
{
    g_showCursorCount += show ? 1 : -1;
    UpdateRelativeMode();
    return g_showCursorCount;
}

BOOL ClipCursor(const RECT *) { return TRUE; }
HWND SetCapture(HWND wnd) { SDL_CaptureMouse(SDL_TRUE); return wnd; }
BOOL ReleaseCapture(void) { SDL_CaptureMouse(SDL_FALSE); return TRUE; }

SHORT GetAsyncKeyState(int vk)
{
    if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON)
    {
        Uint32 m = SDL_GetMouseState(nullptr, nullptr);
        Uint32 bit = vk == VK_LBUTTON ? SDL_BUTTON_LMASK : (vk == VK_RBUTTON ? SDL_BUTTON_RMASK : SDL_BUTTON_MMASK);
        return (m & bit) ? static_cast<SHORT>(0x8000) : 0;
    }
    const Uint8 *keys = SDL_GetKeyboardState(nullptr);
    SDL_Scancode a = SDL_SCANCODE_UNKNOWN, b = SDL_SCANCODE_UNKNOWN;
    switch (vk)
    {
    case VK_SHIFT: a = SDL_SCANCODE_LSHIFT; b = SDL_SCANCODE_RSHIFT; break;
    case VK_CONTROL: a = SDL_SCANCODE_LCTRL; b = SDL_SCANCODE_RCTRL; break;
    case VK_MENU: a = SDL_SCANCODE_LALT; b = SDL_SCANCODE_RALT; break;
    case VK_LSHIFT: a = SDL_SCANCODE_LSHIFT; break;
    case VK_RSHIFT: a = SDL_SCANCODE_RSHIFT; break;
    case VK_LCONTROL: a = SDL_SCANCODE_LCTRL; break;
    case VK_RCONTROL: a = SDL_SCANCODE_RCTRL; break;
    case VK_LMENU: a = SDL_SCANCODE_LALT; break;
    case VK_RMENU: a = SDL_SCANCODE_RALT; break;
    default: a = ScancodeFromVk(vk); break;
    }
    bool down = (a != SDL_SCANCODE_UNKNOWN && keys[a]) || (b != SDL_SCANCODE_UNKNOWN && keys[b]);
    return down ? static_cast<SHORT>(0x8000) : 0;
}

SHORT GetKeyState(int vk)
{
    if (vk == VK_CAPITAL) return (SDL_GetModState() & KMOD_CAPS) ? 1 : 0;
    if (vk == VK_NUMLOCK) return (SDL_GetModState() & KMOD_NUM) ? 1 : 0;
    return GetAsyncKeyState(vk);
}

UINT MapVirtualKeyA(UINT code, UINT mapType)
{
    switch (mapType)
    {
    case MAPVK_VK_TO_CHAR:
        if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9')) return code;
        switch (code)
        {
        case VK_SPACE: return ' ';
        case VK_OEM_1: return ';';
        case VK_OEM_PLUS: return '=';
        case VK_OEM_COMMA: return ',';
        case VK_OEM_MINUS: return '-';
        case VK_OEM_PERIOD: return '.';
        case VK_OEM_2: return '/';
        case VK_OEM_3: return '`';
        case VK_OEM_4: return '[';
        case VK_OEM_5: return '\\';
        case VK_OEM_6: return ']';
        case VK_OEM_7: return '\'';
        case VK_MULTIPLY: return '*';
        case VK_ADD: return '+';
        case VK_SUBTRACT: return '-';
        case VK_DIVIDE: return '/';
        case VK_DECIMAL: return '.';
        default: return code >= VK_NUMPAD0 && code <= VK_NUMPAD0 + 9 ? '0' + (code - VK_NUMPAD0) : 0;
        }
    case MAPVK_VK_TO_VSC:
        return KeyFromScancode(ScancodeFromVk(static_cast<int>(code))).scan;
    case MAPVK_VSC_TO_VK:
        for (int sc = 0; sc < SDL_NUM_SCANCODES; ++sc)
        {
            KeyInfo k = KeyFromScancode(static_cast<SDL_Scancode>(sc));
            if (k.vk && k.scan == code && !k.ext) return k.vk;
        }
        return 0;
    default:
        return 0;
    }
}

// ------------------------------------------------------------------------------------------------
// GDI (nessun effetto: niente console di sistema, gamma gestita dallo shader del motore)
// ------------------------------------------------------------------------------------------------
HDC GetDC(HWND) { return reinterpret_cast<HDC>(static_cast<intptr_t>(1)); }
int ReleaseDC(HWND, HDC) { return 1; }
int GetDeviceCaps(HDC, int index) { return index == LOGPIXELSY ? 96 : 0; }
HFONT CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR) { return nullptr; }
BOOL DeleteObject(HANDLE) { return TRUE; }
BOOL SetDeviceGammaRamp(HDC, LPVOID) { return FALSE; }
BOOL GetDeviceGammaRamp(HDC, LPVOID) { return FALSE; }
int MulDiv(int a, int b, int c) { return c ? static_cast<int>((static_cast<int64_t>(a) * b) / c) : -1; }

// ------------------------------------------------------------------------------------------------
// Appunti
// ------------------------------------------------------------------------------------------------
BOOL OpenClipboard(HWND) { return TRUE; }
BOOL CloseClipboard(void) { return TRUE; }
BOOL EmptyClipboard(void) { SDL_SetClipboardText(""); return TRUE; }

HANDLE GetClipboardData(UINT fmt)
{
    if (fmt != CF_TEXT || !SDL_HasClipboardText()) return nullptr;
    char *text = SDL_GetClipboardText();
    if (!text) return nullptr;
    size_t n = strlen(text) + 1;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n); // liberata dal sistema "appunti" come su Windows: la teniamo
    memcpy(mem, text, n);
    SDL_free(text);
    static HGLOBAL last = nullptr;
    if (last) GlobalFree(last);
    last = mem;
    return mem;
}

BOOL SetClipboardData(UINT fmt, HANDLE mem)
{
    if (fmt == CF_TEXT && mem) SDL_SetClipboardText(static_cast<const char *>(mem));
    return mem != nullptr;
}

// ------------------------------------------------------------------------------------------------
// Varie
// ------------------------------------------------------------------------------------------------
int MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type)
{
    SDL_Window *parent = ((SDL_Window *)g_mainWindow);
    Uint32 flags = (type & 0xF0) == MB_ICONERROR ? SDL_MESSAGEBOX_ERROR : ((type & 0xF0) == MB_ICONWARNING ? SDL_MESSAGEBOX_WARNING : SDL_MESSAGEBOX_INFORMATION);
    if ((type & 0xF) == MB_YESNO || (type & 0xF) == MB_OKCANCEL)
    {
        bool yesno = (type & 0xF) == MB_YESNO;
        SDL_MessageBoxButtonData buttons[2] = {
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, yesno ? IDYES : IDOK, yesno ? "Si" : "OK" },
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, yesno ? IDNO : IDCANCEL, yesno ? "No" : "Annulla" },
        };
        SDL_MessageBoxData data{ flags, parent, caption ? caption : "", text ? text : "", 2, buttons, nullptr };
        int id = yesno ? IDNO : IDCANCEL;
        SDL_ShowMessageBox(&data, &id);
        return id;
    }
    SDL_ShowSimpleMessageBox(flags, caption ? caption : "", text ? text : "", parent);
    return IDOK;
}

int bo1_OpenURL(const char *url) { return SDL_OpenURL(url); }



#endif // !_WIN32

// ------------------------------------------------------------------------------------------------
// T5Jool (t5_main.cpp): the game window, and the Win32 message pump the engine skips under KISAK_NX
// ------------------------------------------------------------------------------------------------
HWND T5_MainWindow()
{
    std::lock_guard<std::mutex> l(g_lock);
    return g_mainWindow;
}

// once a frame from Win_GetEvent (NX_PumpAppletMessages): every SDL event becomes a window message
// for MainWndProc. True when the window was closed.
bool T5_PumpWindowMessages()
{
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT)
            return true;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return false;
}

// ------------------------------------------------------------------------------------------------
// Remaining user32 / gdi32 / shell32 calls (as src/nx/nx_winuser.cpp has them on Switch)
// ------------------------------------------------------------------------------------------------
BOOL EnumThreadWindows(DWORD, WNDENUMPROC, LPARAM) { return TRUE; }
HBRUSH CreateSolidBrush(COLORREF) { return (HBRUSH)(uintptr_t)1; }
BOOL SetPriorityClass(HANDLE, DWORD) { return TRUE; }

int MessageBoxW(HWND wnd, const WCHAR *text, const WCHAR *caption, UINT type)
{
    char t[1024], c[256];
    unsigned i = 0;
    for (; text && text[i] && i < sizeof(t) - 1; ++i) t[i] = (char)(text[i] < 128 ? text[i] : '?');
    t[i] = 0;
    i = 0;
    for (; caption && caption[i] && i < sizeof(c) - 1; ++i) c[i] = (char)(caption[i] < 128 ? caption[i] : '?');
    c[i] = 0;
    return MessageBoxA(wnd, t, c, type);
}

HINSTANCE ShellExecuteA(HWND, LPCSTR, LPCSTR file, LPCSTR, LPCSTR, int)
{
    if (file && (!strncmp(file, "http://", 7) || !strncmp(file, "https://", 8)))
        SDL_OpenURL(file);
    return (HINSTANCE)(uintptr_t)33; // > 32: success
}

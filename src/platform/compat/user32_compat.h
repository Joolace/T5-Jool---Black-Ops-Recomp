// user32_compat.h - il pezzo di User32/GDI che il motore usa per finestra, input e console.
// Implementazione sopra SDL2 in platform/sdl/sdl_user32.cpp: HWND è un SDL_Window* (è anche ciò che
// DXVK-native si aspetta come finestra di destinazione), i messaggi WM_* sono generati dagli eventi SDL.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef LRESULT (*WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef BOOL (*WNDENUMPROC)(HWND, LPARAM);
typedef LRESULT (*HOOKPROC)(int, WPARAM, LPARAM);
typedef HANDLE HHOOK;
typedef HANDLE HMIXER;
typedef HANDLE HMIXEROBJ;

typedef struct tagMSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG, *LPMSG;
typedef struct tagWNDCLASSA {
    UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor;
    HBRUSH hbrBackground; LPCSTR lpszMenuName; LPCSTR lpszClassName;
} WNDCLASSA, WNDCLASS;
typedef struct tagWNDCLASSEXA {
    UINT cbSize, style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor;
    HBRUSH hbrBackground; LPCSTR lpszMenuName; LPCSTR lpszClassName; HICON hIconSm;
} WNDCLASSEXA, WNDCLASSEX;
typedef struct tagMONITORINFO { DWORD cbSize; RECT rcMonitor; RECT rcWork; DWORD dwFlags; } MONITORINFO, *LPMONITORINFO;
typedef BOOL (*MONITORENUMPROC)(HMONITOR, HDC, LPRECT, LPARAM);
#ifdef __cplusplus
typedef struct tagMONITORINFOEXA : public tagMONITORINFO { CHAR szDevice[32]; } MONITORINFOEXA, *LPMONITORINFOEXA; // come in winuser.h
#else
typedef struct tagMONITORINFOEXA { DWORD cbSize; RECT rcMonitor; RECT rcWork; DWORD dwFlags; CHAR szDevice[32]; } MONITORINFOEXA, *LPMONITORINFOEXA;
#endif
typedef struct _devicemodeA {
    BYTE dmDeviceName[32]; WORD dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra; DWORD dmFields;
    POINT dmPosition; DWORD dmDisplayOrientation, dmDisplayFixedOutput;
    short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate; BYTE dmFormName[32]; WORD dmLogPixels;
    DWORD dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;
} DEVMODEA, *LPDEVMODEA;
typedef DEVMODEA DEVMODE;
#define ENUM_CURRENT_SETTINGS ((DWORD)-1)
#define ENUM_REGISTRY_SETTINGS ((DWORD)-2)
#define DM_BITSPERPEL 0x00040000
#define DM_PELSWIDTH 0x00080000
#define DM_PELSHEIGHT 0x00100000
#define DM_DISPLAYFREQUENCY 0x00400000

// Messaggi
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ACTIVATEAPP 0x001C
#define WM_DISPLAYCHANGE 0x007E
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_SYSCOMMAND 0x0112
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_MOUSEWHEEL 0x020A
#define WM_XBUTTONDOWN 0x020B
#define WM_XBUTTONUP 0x020C
#define WM_POWERBROADCAST 0x0218
#define WM_USER 0x0400
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_MBUTTON 0x0010
#define MK_XBUTTON1 0x0020
#define MK_XBUTTON2 0x0040
#define WA_INACTIVE 0
#define WA_ACTIVE 1
#define PM_NOREMOVE 0
#define PM_REMOVE 1

// Stili / ShowWindow / SetWindowPos
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_VISIBLE 0x10000000L
#define WS_CAPTION 0x00C00000L
#define WS_SYSMENU 0x00080000L
#define WS_BORDER 0x00800000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_EX_TOPMOST 0x00000008L
#define WS_EX_APPWINDOW 0x00040000L
#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define GWL_WNDPROC (-4)
#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_RESTORE 9
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOACTIVATE 0x0010
#define SWP_FRAMECHANGED 0x0020
#define SWP_SHOWWINDOW 0x0040
#define HWND_TOP ((HWND)0)
#define HWND_TOPMOST ((HWND)(intptr_t)-1)
#define HWND_NOTOPMOST ((HWND)(intptr_t)-2)
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CMONITORS 80
#define MONITOR_DEFAULTTONULL 0
#define MONITOR_DEFAULTTOPRIMARY 1
#define MONITOR_DEFAULTTONEAREST 2
#define MONITORINFOF_PRIMARY 1
#define ES_CONTINUOUS 0x80000000u
#define ES_DISPLAY_REQUIRED 0x00000002u
#define ES_SYSTEM_REQUIRED 0x00000001u
#define MAPVK_VK_TO_VSC 0
#define MAPVK_VSC_TO_VK 1
#define MAPVK_VK_TO_CHAR 2
#define CW_USEDEFAULT ((int)0x80000000)
#define SPI_GETSCREENSAVEACTIVE 0x0010
#define SPI_SETSCREENSAVEACTIVE 0x0011
#define LOGPIXELSY 90

// Finestre
WORD RegisterClassA(const WNDCLASSA *wc);
WORD RegisterClassExA(const WNDCLASSEXA *wc);
#define RegisterClass RegisterClassA
#define RegisterClassEx RegisterClassExA
BOOL UnregisterClassA(LPCSTR name, HINSTANCE inst);
HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR title, DWORD style, int x, int y, int w, int h, HWND parent, HMENU menu, HINSTANCE inst, LPVOID param);
#define CreateWindowEx CreateWindowExA
#define CreateWindowA(cls, title, style, x, y, w, h, parent, menu, inst, param) CreateWindowExA(0, cls, title, style, x, y, w, h, parent, menu, inst, param)
BOOL DestroyWindow(HWND wnd);
BOOL CloseWindow(HWND wnd);
BOOL ShowWindow(HWND wnd, int cmd);
BOOL UpdateWindow(HWND wnd);
BOOL SetWindowPos(HWND wnd, HWND after, int x, int y, int w, int h, UINT flags);
BOOL MoveWindow(HWND wnd, int x, int y, int w, int h, BOOL repaint);
BOOL GetClientRect(HWND wnd, LPRECT rc);
BOOL GetWindowRect(HWND wnd, LPRECT rc);
BOOL AdjustWindowRect(LPRECT rc, DWORD style, BOOL menu);
BOOL AdjustWindowRectEx(LPRECT rc, DWORD style, BOOL menu, DWORD exStyle);
BOOL ClientToScreen(HWND wnd, LPPOINT pt);
BOOL ScreenToClient(HWND wnd, LPPOINT pt);
BOOL IsWindow(HWND wnd);
BOOL IsWindowVisible(HWND wnd);
BOOL IsIconic(HWND wnd);
HWND SetFocus(HWND wnd);
HWND GetFocus(void);
BOOL SetForegroundWindow(HWND wnd);
LONG GetWindowLongA(HWND wnd, int index);
LONG SetWindowLongA(HWND wnd, int index, LONG value);
#define GetWindowLong GetWindowLongA
#define SetWindowLong SetWindowLongA
int GetWindowTextA(HWND wnd, LPSTR buf, int max);
BOOL SetWindowTextA(HWND wnd, LPCSTR text);
#define SetWindowText SetWindowTextA
LRESULT DefWindowProcA(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
#define DefWindowProc DefWindowProcA
LRESULT CallWindowProcA(WNDPROC proc, HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT SendMessageA(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
#define SendMessage SendMessageA
BOOL PostMessageA(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);
#define PostMessage PostMessageA
void PostQuitMessage(int code);
UINT RegisterWindowMessageA(LPCSTR name);
BOOL EnumWindows(WNDENUMPROC fn, LPARAM lp);
HICON LoadIconA(HINSTANCE inst, LPCSTR name);
HANDLE LoadImageA(HINSTANCE inst, LPCSTR name, UINT type, int cx, int cy, UINT load);

// Messaggi
BOOL PeekMessageA(LPMSG msg, HWND wnd, UINT min, UINT max, UINT remove);
BOOL GetMessageA(LPMSG msg, HWND wnd, UINT min, UINT max);
#define PeekMessage PeekMessageA
#define GetMessage GetMessageA
BOOL TranslateMessage(const MSG *msg);
LRESULT DispatchMessageA(const MSG *msg);
#define DispatchMessage DispatchMessageA
HHOOK SetWindowsHookExA(int id, HOOKPROC fn, HINSTANCE mod, DWORD thread);
BOOL UnhookWindowsHookEx(HHOOK hook);
LRESULT CallNextHookEx(HHOOK hook, int code, WPARAM wp, LPARAM lp);

// Monitor / schermo
HMONITOR MonitorFromWindow(HWND wnd, DWORD flags);
HMONITOR MonitorFromPoint(POINT pt, DWORD flags);
BOOL GetMonitorInfoA(HMONITOR mon, LPMONITORINFO info);
#define GetMonitorInfo GetMonitorInfoA
BOOL EnumDisplayMonitors(HDC dc, LPCRECT clip, MONITORENUMPROC fn, LPARAM lp);
int GetSystemMetrics(int index);
// modi video del monitor (lpszDeviceName = szDevice di GetMonitorInfoA, o NULL per il principale)
BOOL EnumDisplaySettingsA(LPCSTR device, DWORD modeNum, DEVMODEA *mode);
#define EnumDisplaySettings EnumDisplaySettingsA
static inline BOOL EqualRect(const RECT *a, const RECT *b)
{
    return a->left == b->left && a->top == b->top && a->right == b->right && a->bottom == b->bottom;
}
BOOL SystemParametersInfoA(UINT action, UINT param, PVOID pv, UINT winIni);
BOOL IsProcessDPIAware(void);
BOOL SetProcessDPIAware(void);
LONG ChangeDisplaySettingsA(void *mode, DWORD flags);
DWORD SetThreadExecutionState(DWORD flags);

// Mouse / tastiera
BOOL GetCursorPos(LPPOINT pt);
BOOL SetCursorPos(int x, int y);
BOOL ClipCursor(const RECT *rc);
HWND SetCapture(HWND wnd);
BOOL ReleaseCapture(void);
SHORT GetAsyncKeyState(int vk);
SHORT GetKeyState(int vk);
UINT MapVirtualKeyA(UINT code, UINT mapType);
#define MapVirtualKey MapVirtualKeyA

// GDI minimo (console di sistema e gamma): nessun effetto
HDC GetDC(HWND wnd);
int ReleaseDC(HWND wnd, HDC dc);
int GetDeviceCaps(HDC dc, int index);
HFONT CreateFontA(int h, int w, int esc, int orient, int weight, DWORD italic, DWORD underline, DWORD strike, DWORD charset, DWORD outPrec, DWORD clipPrec, DWORD quality, DWORD pitch, LPCSTR face);
BOOL DeleteObject(HANDLE obj);
BOOL SetDeviceGammaRamp(HDC dc, LPVOID ramp);
BOOL GetDeviceGammaRamp(HDC dc, LPVOID ramp);
int MulDiv(int a, int b, int c);

// Appunti
BOOL OpenClipboard(HWND wnd);
BOOL CloseClipboard(void);
BOOL EmptyClipboard(void);
HANDLE GetClipboardData(UINT fmt);
HANDLE SetClipboardData(UINT fmt, HANDLE mem);
#define CF_TEXT 1

#ifdef __cplusplus
}
#endif

// Virtual key Windows (servono per tradurre gli eventi SDL)
#define VK_LBUTTON 0x01
#define VK_RBUTTON 0x02
#define VK_MBUTTON 0x04
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#define VK_PAUSE 0x13
#define VK_CAPITAL 0x14
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_SNAPSHOT 0x2C
#define VK_INSERT 0x2D
#define VK_DELETE 0x2E
#define VK_LWIN 0x5B
#define VK_RWIN 0x5C
#define VK_APPS 0x5D
#define VK_NUMPAD0 0x60
#define VK_MULTIPLY 0x6A
#define VK_ADD 0x6B
#define VK_SUBTRACT 0x6D
#define VK_DECIMAL 0x6E
#define VK_DIVIDE 0x6F
#define VK_F1 0x70
#define VK_NUMLOCK 0x90
#define VK_SCROLL 0x91
#define VK_LSHIFT 0xA0
#define VK_RSHIFT 0xA1
#define VK_LCONTROL 0xA2
#define VK_RCONTROL 0xA3
#define VK_LMENU 0xA4
#define VK_RMENU 0xA5
#define VK_OEM_1 0xBA
#define VK_OEM_PLUS 0xBB
#define VK_OEM_COMMA 0xBC
#define VK_OEM_MINUS 0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2 0xBF
#define VK_OEM_3 0xC0
#define VK_OEM_4 0xDB
#define VK_OEM_5 0xDC
#define VK_OEM_6 0xDD
#define VK_OEM_7 0xDE
#define VK_OEM_102 0xE2

#include "win_input.h"
#include "win_gamepad.h"
#if defined(KISAK_NX) || defined(KISAK_EXPERIMENTAL_SPLITSCREEN)
#include <qcommon/com_clients.h>
#endif
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
#include <client/splitscreen.h>
#endif

#include "win_wndproc.h"
#include <universal/assertive.h>
#include <gfx_d3d/r_dvars.h>
#include <qcommon/common.h>
#include "win_main.h"
#include <gfx_d3d/rb_backend.h>
#include "win_shared.h"
#include <client_mp/cl_input_mp.h>
#ifdef KISAK_NX2
#include <cgame_mp/cg_newDraw_mp.h>
#include <client/cl_keys.h>
#include <client_mp/cl_cgame_mp.h>
#include <nx/nx_joycon_mouse.h>
#include <ui/keycodes.h>
#include <ui/ui_main.h>
#endif

GamePadButton buttonList[32] =
{
  (GamePadButton)GPAD_X,
  (GamePadButton)3,
  (GamePadButton)GPAD_A,
  (GamePadButton)1,
  (GamePadButton)GPAD_B,
  (GamePadButton)2,
  (GamePadButton)GPAD_Y,
  (GamePadButton)4,
  (GamePadButton)GPAD_L_TRIG,
  (GamePadButton)18,
  (GamePadButton)GPAD_R_TRIG,
  (GamePadButton)19,
  (GamePadButton)GPAD_L_SHLDR,
  (GamePadButton)5,
  (GamePadButton)GPAD_R_SHLDR,
  (GamePadButton)6,
  (GamePadButton)GPAD_START,
  (GamePadButton)14,
  (GamePadButton)GPAD_BACK,
  (GamePadButton)15,
  (GamePadButton)GPAD_L3,
  (GamePadButton)16,
  (GamePadButton)GPAD_R3,
  (GamePadButton)17,
  (GamePadButton)GPAD_UP,
  (GamePadButton)20,
  (GamePadButton)GPAD_DOWN,
  (GamePadButton)21,
  (GamePadButton)GPAD_LEFT,
  (GamePadButton)22,
  (GamePadButton)GPAD_RIGHT,
  (GamePadButton)23
};



int window_center_x;
int window_center_y;

WinMouseVars_t s_wmv;

const dvar_t *in_mouse;
#ifdef KISAK_NX2
static const dvar_t *nx_joyconMouse;
#endif
//const dvar_s *gpad_present;
int in_appactive;



void __cdecl IN_StartupGamepads()
{
    GPad_InitAll();
}

void __cdecl IN_RecenterMouse()
{
#ifndef KISAK_NX
    tagRECT window_rect; // [esp+0h] [ebp-10h] BYREF

    GetWindowRect(g_wv.hWnd, &window_rect);
    window_center_x = (window_rect.left + window_rect.right) / 2;
    window_center_y = (window_rect.bottom + window_rect.top) / 2;
    SetCursorPos((window_rect.left + window_rect.right) / 2, (window_rect.bottom + window_rect.top) / 2);
#endif
}

bool __cdecl IN_IsForegroundWindow()
{
    return GetForegroundWindow() == g_wv.hWnd;
}

void __cdecl IN_ActivateMouse(int force)
{
#ifndef KISAK_NX
    if ( s_wmv.mouseInitialized )
    {
        if ( !r_fullscreen
            && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\win32\\win_input.cpp", 332, 0, "%s", "r_fullscreen") )
        {
            __debugbreak();
        }
        if ( in_mouse->current.enabled )
        {
            if ( force || !s_wmv.mouseActive )
                s_wmv.mouseActive = IN_IsForegroundWindow();
        }
        else
        {
            s_wmv.mouseActive = 0;
        }
    }
#endif
}

void __cdecl IN_DeactivateMouse()
{
#ifndef KISAK_NX
    if ( s_wmv.mouseInitialized && s_wmv.mouseActive )
    {
        s_wmv.mouseActive = 0;
        IN_DeactivateWin32Mouse();
    }
#endif
}

void IN_DeactivateWin32Mouse()
{
    IN_ShowSystemCursor(1);
}

void __cdecl IN_StartupMouse()
{
#ifndef KISAK_NX
    s_wmv.mouseInitialized = 0;
    if ( in_mouse->current.enabled )
    {
        s_wmv.mouseInitialized = 1;
        //BLOPS_NULLSUB();
    }
    else
    {
        Com_Printf(16, "Mouse control not active.\n");
    }
#endif
}

void __cdecl IN_MouseEvent(int mstate)
{
#ifndef KISAK_NX
    int diff; // [esp+0h] [ebp-8h]
    int button; // [esp+4h] [ebp-4h]

    if ( s_wmv.mouseInitialized )
    {
        diff = s_wmv.oldButtonState ^ mstate;
        if ( s_wmv.oldButtonState != mstate )
        {
            //BLOPS_NULLSUB();
            for ( button = 0; button < 5; ++button )
            {
                if ( (diff & (1 << button)) != 0 )
                    Sys_QueEvent(g_msgTime, SE_KEY, button + 200, (mstate & (1 << button)) != 0, 0, 0);
            }
            s_wmv.oldButtonState = mstate;
        }
    }
#endif
}

void __cdecl IN_SetCursorPos(unsigned int x, unsigned int y)
{
#ifndef KISAK_NX
    tagPOINT curPos; // [esp+0h] [ebp-8h] BYREF

    //curPos = (tagPOINT)__PAIR64__(y, x);
    curPos.x = x;
    curPos.y = y;
    ClientToScreen(g_wv.hWnd, &curPos);
    SetCursorPos(curPos.x, curPos.y);
    s_wmv.oldPos = curPos;
#endif
}

void __cdecl IN_ShowSystemCursor(bool show)
{
#ifndef KISAK_NX
    int actualShow; // [esp+0h] [ebp-8h]
    int desiredShow; // [esp+4h] [ebp-4h]

    g_showCursor = show;
    desiredShow = show - 1;
    for ( actualShow = ShowCursor(show); actualShow != desiredShow; actualShow = ShowCursor(actualShow < desiredShow) )
        ;
#endif
}

void __cdecl IN_Startup()
{
#ifndef KISAK_NX
    IN_StartupMouse();
    Dvar_ClearModified(in_mouse);
#endif
    IN_StartupGamepads();
}

void __cdecl IN_Shutdown()
{
    IN_DeactivateMouse();
}

void __cdecl IN_Init()
{
#ifndef KISAK_NX
    in_mouse = _Dvar_RegisterBool("in_mouse", 1, 0x21u, "Initialize the mouse drivers");
#endif
#ifdef KISAK_NX2
    // Archived, so the options menu's JOY-CON MOUSE choice persists in config.cfg.
    nx_joyconMouse = _Dvar_RegisterBool("nx_joyconMouse", 1, 0x1u, "Use a Joy-Con slid on a surface as the mouse");
#endif
    IN_Startup();
}

void __cdecl IN_Activate(int active)
{
    in_appactive = active;
    if ( active )
        IN_ActivateMouse(1);
    else
        IN_DeactivateMouse();
}

#ifdef KISAK_NX2
// The Joy-Con mouse (nx_xinput.cpp). Over a menu it is the menu's cursor, as
// on PC: it focuses what it points at, its click activates and its stick
// scrolls. The cursor shows once the Joy-Con moves and hides when the
// controller is used (UI_KeyEvent) or no menu has the keys. In game it turns
// the view through the mouse look (CL_MouseMove, the sensitivity dvar), and
// its buttons stay the controller's. nx_joyconMouse turns it off.
static void IN_JoyConMouseMove()
{
    static unsigned s_held, s_clicks;
    static int s_wheelTime, s_wheelDir;

    const clientUIActive_t *ui = CL_GetLocalClientUIGlobals(0);
    const bool menu = ui && (ui->keyCatchers & 0x10) && !UI_KeysBypassMenu(0);
    const bool enabled = nx_joyconMouse->current.enabled;
    if (!menu || !enabled)
        UI_NX_HideCursor(0);

    NxJoyConMouse mouse = {};
    const bool present = enabled && NX_JoyConMouse(&mouse);
    if (present && (mouse.dx || mouse.dy))
    {
        int width, height;
        float aspect;
        CL_GetScreenDimensions(&width, &height, &aspect);
        CL_MouseEvent(mouse.x * width / NX_JOYCON_MOUSE_WIDTH, mouse.y * height / NX_JOYCON_MOUSE_HEIGHT, mouse.dx,
                      mouse.dy);
    }
    const bool cursor = present && menu && UI_NX_CursorVisible(0);
    NX_JoyConMouseClaimClicks(cursor);

    const int time = Sys_Milliseconds();
    const unsigned pressed = cursor ? mouse.buttons & ~s_held : 0;
    s_held = mouse.buttons;
    for (int i = 0; i < 5; ++i)
    {
        const unsigned bit = 1u << i;
        if (pressed & bit)
        {
            s_clicks |= bit;
            CL_KeyEvent(0, K_MOUSE1 + i, 1, time);
        }
        else if ((s_clicks & bit) && !(mouse.buttons & bit))
        {
            s_clicks &= ~bit;
            CL_KeyEvent(0, K_MOUSE1 + i, 0, time);
        }
    }

    // One notch at most every 100 ms in the same direction.
    const int dir = cursor ? (mouse.wheel > 0) - (mouse.wheel < 0) : 0;
    if (dir && (dir != s_wheelDir || time - s_wheelTime >= 100))
    {
        const int key = dir > 0 ? K_MWHEELUP : K_MWHEELDOWN;
        CL_KeyEvent(0, key, 1, time);
        CL_KeyEvent(0, key, 0, time);
        s_wheelTime = time;
        s_wheelDir = dir;
    }
}
#endif

void __cdecl IN_Frame()
{
#ifdef KISAK_NX
    if (in_appactive && IN_IsForegroundWindow())
    {
#ifdef KISAK_NX2
        IN_JoyConMouseMove();
#endif
        IN_GamepadsMove();
    }
#else
    if ( Dvar_GetBool("ClickToContinue") )
        PostMessageA(g_wv.hWnd, 0x201u, 1u, 0);
    if ( s_wmv.mouseInitialized )
    {
        if ( in_appactive )
        {
            IN_ActivateMouse(0);
            IN_MouseMove();
            if ( IN_IsForegroundWindow() )
                IN_GamepadsMove();
        }
        else
        {
            IN_DeactivateMouse();
        }
    }
#endif
}

void IN_GamepadsMove()
{
    int ry; // [esp+0h] [ebp-2Ch]
    unsigned int butIndex; // [esp+4h] [ebp-28h]
    int controllerIndex; // [esp+8h] [ebp-24h]
    int rightTrig; // [esp+Ch] [ebp-20h]
    int lx; // [esp+14h] [ebp-18h]
    char gpadPresent; // [esp+1Bh] [ebp-11h]
    DWORD time; // [esp+1Ch] [ebp-10h]
    int rx; // [esp+20h] [ebp-Ch]
    int leftTrig; // [esp+24h] [ebp-8h]
    int ly; // [esp+28h] [ebp-4h]

    gpadPresent = 0;
    GPad_UpdateAll();
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
    CL_SplitGuestMenuFrame();
#endif
    time = Sys_Milliseconds();
#ifdef KISAK_NX
    const int menuController = Com_LocalClient_GetControllerIndex(0);
    float menuStickX = 0.0f;
    float menuStickY = 0.0f;
#endif
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
    static bool routed[GPAD_PORT_COUNT];
    static unsigned int routedButtons[GPAD_PORT_COUNT];
#endif
    for (controllerIndex = 0; controllerIndex < GPAD_PORT_COUNT; ++controllerIndex)
    {
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
        const bool active = GPad_IsActive(controllerIndex);
        gpadPresent |= active;
        const bool route = active
            && controllerIndex == Com_LocalClient_GetControllerIndex(0)
            && !CL_SplitGuestOwnsController(controllerIndex) && !CL_SplitGuestOwnsMenu();
        if (routed[controllerIndex] && !route)
        {
            // Release the host's old pad state on unplug or guest reassignment.
            for (unsigned int axis = 0; axis < 6; ++axis)
                CL_GamepadEvent(controllerIndex, axis, 0);
            for (butIndex = 0; butIndex < 0x10; ++butIndex)
                if (routedButtons[controllerIndex] & (1u << butIndex))
                    CL_GamepadButtonEventForPort(controllerIndex, buttonList[2 * butIndex + 1], 0,
                        time, buttonList[2 * butIndex]);
            routedButtons[controllerIndex] = 0;
        }
        routed[controllerIndex] = route;
        if (!route)
            continue;
#endif
        if (GPad_IsActive(controllerIndex))
        {
            gpadPresent = 1;
            const double leftX = GPad_GetStick(controllerIndex, GPAD_LX);
            const double leftY = GPad_GetStick(controllerIndex, GPAD_LY);
            lx = (int)(leftX * 65535.0);
            ly = (int)(leftY * 65535.0);
#ifdef KISAK_NX
            if (controllerIndex == menuController)
            {
                menuStickX = static_cast<float>(leftX);
                menuStickY = static_cast<float>(leftY);
            }
#endif
            rx = (int)(GPad_GetStick(controllerIndex, GPAD_RX) * 65535.0);
            ry = (int)(GPad_GetStick(controllerIndex, GPAD_RY) * 65535.0);
            leftTrig = (int)(GPad_GetButton(controllerIndex, GPAD_L_TRIG) * 65535.0);
            rightTrig = (int)(GPad_GetButton(controllerIndex, GPAD_R_TRIG) * 65535.0);
            CL_GamepadEvent(controllerIndex, 0, rx);
            CL_GamepadEvent(controllerIndex, 1u, ry);
            CL_GamepadEvent(controllerIndex, 2u, lx);
            CL_GamepadEvent(controllerIndex, 3u, ly);
            CL_GamepadEvent(controllerIndex, 5u, leftTrig);
            CL_GamepadEvent(controllerIndex, 4u, rightTrig);
            for (butIndex = 0; butIndex < 0x10; ++butIndex)
            {
                if (GPad_IsButtonPressed(controllerIndex, buttonList[2 * butIndex]))
                {
                    CL_GamepadButtonEventForPort(controllerIndex, buttonList[2 * butIndex + 1], 1, time, buttonList[2 * butIndex]);
                }
                else if (GPad_ButtonRequiresUpdates(controllerIndex, buttonList[2 * butIndex]))
                {
                    CL_GamepadButtonEventForPort(controllerIndex, buttonList[2 * butIndex + 1], 2, time, buttonList[2 * butIndex]);
                }
                else if (GPad_IsButtonReleased(controllerIndex, buttonList[2 * butIndex]))
                {
                    CL_GamepadButtonEventForPort(controllerIndex, buttonList[2 * butIndex + 1], 0, time, buttonList[2 * butIndex]);
                }
            }
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
            routedButtons[controllerIndex] = 0;
            for (butIndex = 0; butIndex < 0x10; ++butIndex)
                if (GPad_GetButton(controllerIndex, buttonList[2 * butIndex]) > 0.0)
                    routedButtons[controllerIndex] |= 1u << butIndex;
#endif
        }
    }
#ifdef KISAK_NX
    CL_GamepadUpdateMenuStick(menuStickX, menuStickY, time);
#endif
    if (gpad_present && gpad_present->current.color[0] != gpadPresent)
        Dvar_SetBool((dvar_s*)gpad_present, gpadPresent);
}

void IN_MouseMove()
{
#ifndef KISAK_NX
    int v0; // [esp+0h] [ebp-10h]
    tagPOINT curPos; // [esp+4h] [ebp-Ch] BYREF
    int dy; // [esp+Ch] [ebp-4h]

    if ( !s_wmv.mouseInitialized
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\win32\\win_input.cpp",
                    475,
                    0,
                    "%s",
                    "s_wmv.mouseInitialized") )
    {
        __debugbreak();
    }
    if ( !r_fullscreen
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\win32\\win_input.cpp", 476, 0, "%s", "r_fullscreen") )
    {
        __debugbreak();
    }
    if ( IN_IsForegroundWindow() )
    {
        GetCursorPos(&curPos);
        if ( r_fullscreen->current.enabled )
            IN_ClampMouseMove(&curPos);
        v0 = curPos.x - s_wmv.oldPos.x;
        dy = curPos.y - s_wmv.oldPos.y;
        if (curPos.x != s_wmv.oldPos.x || dy)
        {
            //BLOPS_NULLSUB();
        }
        s_wmv.oldPos = curPos;
        ScreenToClient(g_wv.hWnd, &curPos);
        g_wv.recenterMouse = CL_MouseEvent(curPos.x, curPos.y, v0, dy);
        if ( g_wv.recenterMouse && (v0 || dy) )
        {
            IN_RecenterMouse();
            s_wmv.oldPos.x = window_center_x;
            s_wmv.oldPos.y = window_center_y;
        }
    }
#endif
}

void __cdecl IN_ClampMouseMove(tagPOINT *curPos)
{
    bool isClamped; // [esp+3h] [ebp-11h]
    tagRECT rc; // [esp+4h] [ebp-10h] BYREF

    GetWindowRect(g_wv.hWnd, &rc);
    isClamped = 0;
    if ( curPos->x >= rc.left )
    {
        if ( curPos->x >= rc.right )
        {
            curPos->x = rc.right - 1;
            isClamped = 1;
        }
    }
    else
    {
        curPos->x = rc.left;
        isClamped = 1;
    }
    if ( curPos->y >= rc.top )
    {
        if ( curPos->y >= rc.bottom )
        {
            curPos->y = rc.bottom - 1;
            isClamped = 1;
        }
    }
    else
    {
        curPos->y = rc.top;
        isClamped = 1;
    }
    if ( isClamped )
        SetCursorPos(curPos->x, curPos->y);
}


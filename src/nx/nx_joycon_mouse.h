#pragma once
// The Joy-Con mouse on Switch 2 (nx_xinput.cpp).

// The pointer area the positions are given in.
constexpr int NX_JOYCON_MOUSE_WIDTH = 1920;
constexpr int NX_JOYCON_MOUSE_HEIGHT = 1080;

struct NxJoyConMouse
{
    int x, y;           // the pointer
    int dx, dy;         // movement since the previous call
    int wheel;          // scrolling since the previous call (the Joy-Con's stick), positive up
    unsigned buttons;   // held HidMouseButton bits
};

// Once a frame: the Joy-Con of controller one that last slid on a surface.
// False without a Joy-Con mouse.
bool NX_JoyConMouse(NxJoyConMouse *mouse);
// While claimed, that Joy-Con's L/ZL or R/ZR are the mouse's click and leave
// the gamepad until released.
void NX_JoyConMouseClaimClicks(bool claim);

// t5_touch.h - T5Jool touch controls (virtual gamepad). Linux builds have none; the Android
// implementation (t5_touch.cpp) comes with the Android port.
#pragma once
#include <SDL.h>
struct _XINPUT_GAMEPAD;
#ifdef T5_TOUCH
bool BO1Touch_HandleEvent(const SDL_Event *e);     // true: the touch went to the controls
bool BO1Touch_DropTouchMouse();                    // true: ignore the mouse SDL makes from touches
bool BO1Touch_GetPad(struct _XINPUT_GAMEPAD *pad); // the virtual pad; false: controls off
void BO1Touch_PhysicalPadUsed();                   // a real pad is in use: hide the controls
#else
inline bool BO1Touch_HandleEvent(const SDL_Event *) { return false; }
inline bool BO1Touch_DropTouchMouse() { return false; }
inline bool BO1Touch_GetPad(struct _XINPUT_GAMEPAD *) { return false; }
inline void BO1Touch_PhysicalPadUsed() {}
#endif

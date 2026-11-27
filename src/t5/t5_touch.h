// t5_touch.h - T5Jool touch controls (virtual gamepad). Linux builds have none; the Android
// implementation (t5_touch.cpp) comes with the Android port.
#pragma once
#include <SDL.h>
struct _XINPUT_GAMEPAD;
void BO1Touch_Init();                  // dvars bo1_touch*
void BO1Touch_Draw(int localClientNum); // from SCR_DrawScreenField: draws the controls, tracks "in game"
void BO1Touch_StartEdit();             // layout editor (move / resize the controls)
bool BO1Touch_IsEditing();
void BO1Touch_StopEdit();              // saves and leaves the editor
#ifdef T5_TOUCH
bool BO1Touch_HandleEvent(const SDL_Event *e);     // true: the touch went to the controls
bool BO1Touch_DropTouchMouse(const SDL_Event *e);  // true: ignore this mouse event SDL made from a touch
bool BO1Touch_GetPad(struct _XINPUT_GAMEPAD *pad); // the virtual pad; false: controls off
void BO1Touch_FlushLook();                         // once a frame, before the mouse is read: the smoothed touch aim
void BO1Touch_PhysicalPadUsed();                   // a real pad is in use: hide the controls
#else
inline bool BO1Touch_HandleEvent(const SDL_Event *) { return false; }
inline bool BO1Touch_DropTouchMouse(const SDL_Event *) { return false; }
inline bool BO1Touch_GetPad(struct _XINPUT_GAMEPAD *) { return false; }
inline void BO1Touch_FlushLook() {}
inline void BO1Touch_PhysicalPadUsed() {}
#endif

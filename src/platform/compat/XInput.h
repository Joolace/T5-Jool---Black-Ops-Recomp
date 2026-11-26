#pragma once
// XInput.h (Linux/Android): stessa API di XInput 1.3; l'implementazione (platform/sdl/sdl_xinput.cpp)
// legge i controller tramite SDL_GameController, quindi funziona con qualunque pad riconosciuto da SDL.
#include "win_compat.h"

#define XUSER_MAX_COUNT 4
#define XUSER_INDEX_ANY 0xFF
#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB 0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB 0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER 0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_X 0x4000
#define XINPUT_GAMEPAD_Y 0x8000
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE 7849
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD 30
#define XINPUT_FLAG_GAMEPAD 0x00000001
#define XINPUT_DEVTYPE_GAMEPAD 0x01
#define XINPUT_DEVSUBTYPE_GAMEPAD 0x01
#define ERROR_DEVICE_NOT_CONNECTED 1167L

typedef struct _XINPUT_GAMEPAD { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; } XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;
typedef struct _XINPUT_STATE { DWORD dwPacketNumber; XINPUT_GAMEPAD Gamepad; } XINPUT_STATE, *PXINPUT_STATE;
typedef struct _XINPUT_VIBRATION { WORD wLeftMotorSpeed, wRightMotorSpeed; } XINPUT_VIBRATION, *PXINPUT_VIBRATION;
typedef struct _XINPUT_CAPABILITIES { BYTE Type, SubType; WORD Flags; XINPUT_GAMEPAD Gamepad; XINPUT_VIBRATION Vibration; } XINPUT_CAPABILITIES, *PXINPUT_CAPABILITIES;

#ifdef __cplusplus
extern "C" {
#endif
DWORD XInputGetState(DWORD user, XINPUT_STATE *state);
DWORD XInputSetState(DWORD user, XINPUT_VIBRATION *vib);
DWORD XInputGetCapabilities(DWORD user, DWORD flags, XINPUT_CAPABILITIES *caps);
void XInputEnable(BOOL enable);
#ifdef __cplusplus
}
#endif

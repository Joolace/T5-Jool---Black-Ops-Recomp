// sdl_xinput.cpp - XInput 1.3 sopra SDL_GameController: il codice del gamepad del motore (win_gamepad.cpp)
// resta invariato e funziona con qualunque controller riconosciuto da SDL (Xbox, DualShock/DualSense,
// Switch Pro, pad Bluetooth su Android, i comandi integrati delle console portatili Android...).
#if !defined(_WIN32)

#include <windows.h>
#include <XInput.h>
#include "../t5_win32_extra.h"
#include <SDL.h>
#include <cstdio>
#include "../t5_touch.h"

#include <mutex>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

namespace
{
struct Pad
{
    SDL_GameController *ctrl = nullptr;
    SDL_JoystickID id = -1;
    DWORD packet = 0;
    XINPUT_GAMEPAD last{};
};

std::mutex g_padLock;
Pad g_pads[XUSER_MAX_COUNT];
bool g_enabled = true;

// T5Jool: mappature in piu' per i pad che SDL non conosce (database della comunita' "gamecontrollerdb.txt", nella
// cartella del gioco o in t5jool/), e una riga nel log per ogni pad: nome, tipo, se e' riconosciuto o ignorato.
void LoadExtraMappings()
{
    static bool done;
    if (done) return;
    done = true;
    for (const char *path : { "gamecontrollerdb.txt", "t5jool/gamecontrollerdb.txt" })
    {
        const int n = SDL_GameControllerAddMappingsFromFile(path);
        if (n > 0)
            fprintf(stderr, "T5Jool pad: %d mappature caricate da %s\n", n, path);
    }
}

const char *PadTypeName(SDL_GameController *c)
{
#if SDL_VERSION_ATLEAST(2, 0, 12)
    switch (SDL_GameControllerGetType(c))
    {
    case SDL_CONTROLLER_TYPE_XBOX360:
    case SDL_CONTROLLER_TYPE_XBOXONE: return "Xbox";
    case SDL_CONTROLLER_TYPE_PS3: return "PS3";
    case SDL_CONTROLLER_TYPE_PS4: return "PS4";
#if SDL_VERSION_ATLEAST(2, 0, 14)
    case SDL_CONTROLLER_TYPE_PS5: return "PS5";
#endif
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: return "Switch Pro";
    default: return "generico";
    }
#else
    (void)c;
    return "?";
#endif
}

// Assegna i controller collegati agli slot liberi e libera quelli scollegati.
void RefreshPads()
{
    LoadExtraMappings();
    for (Pad &p : g_pads)
    {
        if (p.ctrl && !SDL_GameControllerGetAttached(p.ctrl))
        {
            SDL_GameControllerClose(p.ctrl);
            p = Pad{};
        }
    }
    int n = SDL_NumJoysticks();
    static SDL_JoystickID s_reported[32];
    static int s_reportedCount;
    for (int i = 0; i < n; ++i)
    {
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(i);
        bool reported = false;
        for (int k = 0; k < s_reportedCount; ++k) reported |= s_reported[k] == id;
        if (!reported && s_reportedCount < 32)
        {
            s_reported[s_reportedCount++] = id;
            char guid[64];
            SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof(guid));
            const char *name = SDL_JoystickNameForIndex(i);
            if (SDL_IsGameController(i))
                fprintf(stderr, "T5Jool pad: '%s' riconosciuto (GUID %s)\n", name ? name : "?", guid);
            else
                fprintf(stderr, "T5Jool pad: '%s' ignorato, SDL non lo conosce come controller (GUID %s): serve una "
                                "mappatura in gamecontrollerdb.txt\n", name ? name : "?", guid);
        }
        if (!SDL_IsGameController(i)) continue;
        bool used = false;
        for (Pad &p : g_pads) used |= p.ctrl && p.id == id;
        if (used) continue;
        for (Pad &p : g_pads)
        {
            if (p.ctrl) continue;
            p.ctrl = SDL_GameControllerOpen(i);
            p.id = id;
            if (p.ctrl)
                fprintf(stderr, "T5Jool pad: '%s' in uso (tipo %s)\n", SDL_GameControllerName(p.ctrl) ? SDL_GameControllerName(p.ctrl) : "?",
                        PadTypeName(p.ctrl));
            break;
        }
    }
}

SHORT Axis(SDL_GameController *c, SDL_GameControllerAxis a, bool invert)
{
    int v = SDL_GameControllerGetAxis(c, a);
    if (invert) v = -v - 1; // SDL: giù = positivo; XInput: su = positivo
    if (v < -32768) v = -32768;
    if (v > 32767) v = 32767;
    return static_cast<SHORT>(v);
}

BYTE Trigger(SDL_GameController *c, SDL_GameControllerAxis a)
{
    int v = SDL_GameControllerGetAxis(c, a); // 0..32767
    return static_cast<BYTE>(v <= 0 ? 0 : (v * 255) / 32767);
}

// ------------------------------------------------------------------------------------- rimappatura
// T5_PADMAP="azione=tasto,..." (dal lanciatore Android): quale tasto fisico produce ogni comando del pad
// che il gioco vede. Azioni e tasti: a b x y lb rb lt rt ls rs start back up down left right.
// T5_PADSWAP=1 scambia le levette (mancini).
enum PadSlot { P_A, P_B, P_X, P_Y, P_LB, P_RB, P_LT, P_RT, P_LS, P_RS, P_START, P_BACK, P_UP, P_DOWN, P_LEFT, P_RIGHT, P_COUNT };
const char *const kSlotNames[P_COUNT] = { "a", "b", "x", "y", "lb", "rb", "lt", "rt", "ls", "rs", "start", "back", "up", "down", "left", "right" };
const WORD kSlotXButton[P_COUNT] = { XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
                                     XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, 0, 0,
                                     XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB, XINPUT_GAMEPAD_START,
                                     XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN,
                                     XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT };
int g_padMap[P_COUNT]; // uscita -> tasto fisico
bool g_padSwap;
bool g_padMapLoaded;

int SlotByName(const char *s, size_t n)
{
    for (int i = 0; i < P_COUNT; ++i)
        if (strlen(kSlotNames[i]) == n && !strncmp(kSlotNames[i], s, n))
            return i;
    return -1;
}

void LoadPadMap()
{
    g_padMapLoaded = true;
    for (int i = 0; i < P_COUNT; ++i)
        g_padMap[i] = i;
    if (const char *sw = getenv("T5_PADSWAP"))
        g_padSwap = *sw == '1';
    const char *m = getenv("T5_PADMAP");
    if (!m)
        return;
    while (*m)
    {
        const char *eq = strchr(m, '='), *end = strchr(m, ',');
        if (!end)
            end = m + strlen(m);
        if (eq && eq < end)
        {
            const int out = SlotByName(m, (size_t)(eq - m)), in = SlotByName(eq + 1, (size_t)(end - eq - 1));
            if (out >= 0 && in >= 0)
                g_padMap[out] = in;
        }
        m = *end ? end + 1 : end;
    }
    fprintf(stderr, "T5Jool: mappa del controller da T5_PADMAP%s\n", g_padSwap ? ", levette scambiate" : "");
}

// valore 0..255 di un tasto fisico (grilletti analogici, il resto 0 o 255)
int PhysValue(SDL_GameController *c, int slot)
{
    static const SDL_GameControllerButton kButton[P_COUNT] = {
        SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_INVALID,
        SDL_CONTROLLER_BUTTON_INVALID, SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
        SDL_CONTROLLER_BUTTON_START, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT };
    if (slot == P_LT)
        return Trigger(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    if (slot == P_RT)
        return Trigger(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    return SDL_GameControllerGetButton(c, kButton[slot]) ? 255 : 0;
}
} // namespace

DWORD XInputGetState(DWORD user, XINPUT_STATE *state)
{
    if (user >= XUSER_MAX_COUNT || !state) return ERROR_DEVICE_NOT_CONNECTED;
    std::lock_guard<std::mutex> l(g_padLock);
    RefreshPads();
    Pad &p = g_pads[user];
    // comandi touch: controller virtuale nello slot 0, unito al pad vero se c'e'
    XINPUT_GAMEPAD touch{};
    const bool touchOn = user == 0 && BO1Touch_GetPad(&touch);
    if (!p.ctrl && !touchOn) return ERROR_DEVICE_NOT_CONNECTED;

    XINPUT_GAMEPAD g{};
    if (g_enabled)
    {
        static const struct { SDL_GameControllerButton b; WORD x; } map[] = {
            { SDL_CONTROLLER_BUTTON_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP },
            { SDL_CONTROLLER_BUTTON_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN },
            { SDL_CONTROLLER_BUTTON_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT },
            { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT },
            { SDL_CONTROLLER_BUTTON_START, XINPUT_GAMEPAD_START },
            { SDL_CONTROLLER_BUTTON_BACK, XINPUT_GAMEPAD_BACK },
            { SDL_CONTROLLER_BUTTON_LEFTSTICK, XINPUT_GAMEPAD_LEFT_THUMB },
            { SDL_CONTROLLER_BUTTON_RIGHTSTICK, XINPUT_GAMEPAD_RIGHT_THUMB },
            { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, XINPUT_GAMEPAD_LEFT_SHOULDER },
            { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER },
            { SDL_CONTROLLER_BUTTON_A, XINPUT_GAMEPAD_A },
            { SDL_CONTROLLER_BUTTON_B, XINPUT_GAMEPAD_B },
            { SDL_CONTROLLER_BUTTON_X, XINPUT_GAMEPAD_X },
            { SDL_CONTROLLER_BUTTON_Y, XINPUT_GAMEPAD_Y },
        };
        if (p.ctrl)
        {
            if (!g_padMapLoaded)
                LoadPadMap();
            (void)map;
            for (int out = 0; out < P_COUNT; ++out)
            {
                const int v = PhysValue(p.ctrl, g_padMap[out]);
                if (out == P_LT)
                    g.bLeftTrigger = static_cast<BYTE>(v);
                else if (out == P_RT)
                    g.bRightTrigger = static_cast<BYTE>(v);
                else if (v > 30)
                    g.wButtons |= kSlotXButton[out];
            }
            g.sThumbLX = Axis(p.ctrl, g_padSwap ? SDL_CONTROLLER_AXIS_RIGHTX : SDL_CONTROLLER_AXIS_LEFTX, false);
            g.sThumbLY = Axis(p.ctrl, g_padSwap ? SDL_CONTROLLER_AXIS_RIGHTY : SDL_CONTROLLER_AXIS_LEFTY, true);
            g.sThumbRX = Axis(p.ctrl, g_padSwap ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX, false);
            g.sThumbRY = Axis(p.ctrl, g_padSwap ? SDL_CONTROLLER_AXIS_LEFTY : SDL_CONTROLLER_AXIS_RIGHTY, true);
            // controller vero in uso (pulsante o levetta oltre la zona morta): i comandi touch spariscono
            if (g.wButtons || g.bLeftTrigger > 40 || g.bRightTrigger > 40 || abs(g.sThumbLX) > 12000 || abs(g.sThumbLY) > 12000
                || abs(g.sThumbRX) > 12000 || abs(g.sThumbRY) > 12000)
                BO1Touch_PhysicalPadUsed();
        }
        if (touchOn)
        {
            g.wButtons |= touch.wButtons;
            if (touch.bLeftTrigger > g.bLeftTrigger) g.bLeftTrigger = touch.bLeftTrigger;
            if (touch.bRightTrigger > g.bRightTrigger) g.bRightTrigger = touch.bRightTrigger;
            if (touch.sThumbLX || touch.sThumbLY)
            {
                g.sThumbLX = touch.sThumbLX;
                g.sThumbLY = touch.sThumbLY;
            }
        }
    }
    if (memcmp(&g, &p.last, sizeof(g)) != 0) { ++p.packet; p.last = g; }
    state->dwPacketNumber = p.packet;
    state->Gamepad = g;
    return ERROR_SUCCESS;
}

// T5Jool: per bo1_feel.cpp (vibrazione e giroscopio del controller)
SDL_GameController *BO1Pad_Controller0()
{
    return g_pads[0].ctrl;
}

DWORD XInputSetState(DWORD user, XINPUT_VIBRATION *vib)
{
    if (user >= XUSER_MAX_COUNT || !vib) return ERROR_DEVICE_NOT_CONNECTED;
    std::lock_guard<std::mutex> l(g_padLock);
    Pad &p = g_pads[user];
    if (!p.ctrl) return user == 0 ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
    // Il motore aggiorna la vibrazione ogni frame: 200 ms bastano a coprire il frame successivo.
    SDL_GameControllerRumble(p.ctrl, vib->wLeftMotorSpeed, vib->wRightMotorSpeed, g_enabled ? 200 : 0);
    return ERROR_SUCCESS;
}

DWORD XInputGetCapabilities(DWORD user, DWORD, XINPUT_CAPABILITIES *caps)
{
    if (user >= XUSER_MAX_COUNT || !caps) return ERROR_DEVICE_NOT_CONNECTED;
    std::lock_guard<std::mutex> l(g_padLock);
    RefreshPads();
    XINPUT_GAMEPAD touch;
    if (!g_pads[user].ctrl && !(user == 0 && BO1Touch_GetPad(&touch))) return ERROR_DEVICE_NOT_CONNECTED;
    memset(caps, 0, sizeof(*caps));
    caps->Type = XINPUT_DEVTYPE_GAMEPAD;
    caps->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
    caps->Gamepad.wButtons = 0xF3FF;
    caps->Gamepad.bLeftTrigger = caps->Gamepad.bRightTrigger = 255;
    caps->Vibration.wLeftMotorSpeed = caps->Vibration.wRightMotorSpeed = 65535;
    return ERROR_SUCCESS;
}

void XInputEnable(BOOL enable) { g_enabled = enable != 0; }

#endif // !_WIN32

// Motion aim (CL_NX_GyroAim, cl_input_mp.cpp): the pad's turn rates in degrees per second.
// Positive yaw turns left, positive pitch tilts the view up. False without a gyro.
extern "C" bool NX_GyroRate(unsigned userIndex, float *yawRate, float *pitchRate)
{
    *yawRate = *pitchRate = 0.0f;
    if (userIndex >= XUSER_MAX_COUNT)
        return false;
    std::lock_guard<std::mutex> l(g_padLock);
    SDL_GameController *c = g_pads[userIndex].ctrl;
    if (!c || !SDL_GameControllerHasSensor(c, SDL_SENSOR_GYRO))
        return false;
    if (!SDL_GameControllerIsSensorEnabled(c, SDL_SENSOR_GYRO))
        SDL_GameControllerSetSensorEnabled(c, SDL_SENSOR_GYRO, SDL_TRUE);
    float rate[3] = {};
    if (SDL_GameControllerGetSensorData(c, SDL_SENSOR_GYRO, rate, 3) != 0)
        return false;
    const float toDeg = 57.29578f;
    *pitchRate = rate[0] * toDeg;
    *yawRate = rate[1] * toDeg;
    return true;
}

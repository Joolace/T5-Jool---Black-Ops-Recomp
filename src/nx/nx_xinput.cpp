// nx_xinput.cpp -- XInput over libnx HID. The game's existing gamepad layer
// (win_gamepad.cpp GPad_*) drives Switch controllers through this.
#include <XInput.h>
#include <switch.h>
#include <math.h>
#ifdef KISAK_NX2
#include "nx_joycon_mouse.h"
#endif

#define NX_ERROR_DEVICE_NOT_CONNECTED 1167

#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
static constexpr unsigned NX_PAD_COUNT = 2;
#else
static constexpr unsigned NX_PAD_COUNT = 1;
#endif
static PadState s_pads[NX_PAD_COUNT];
static bool s_padInit;
static u32 s_packet[NX_PAD_COUNT];
#ifdef KISAK_NX2
// Slot zero's buttons the Joy-Con mouse claims (NX_JoyConMouse), and the
// claimed ones still held, which stay off the gamepad until released.
static u64 s_mouseClaim, s_mouseClaimHeld;
#endif

// Slot zero accepts the handheld controls or controller one. Slot one reads
// controller two independently; never merge its buttons into the host pad.
// GPad_Check and XInputGetState each poll, but only read held buttons/sticks.
static bool nxPadPoll(DWORD userIndex)
{
    if (userIndex >= NX_PAD_COUNT)
        return false;
    if (!s_padInit) {
        padConfigureInput(NX_PAD_COUNT, HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&s_pads[0]);
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
        padInitialize(&s_pads[1], HidNpadIdType_No2);
#endif
        s_padInit = true;
    }
    padUpdate(&s_pads[userIndex]);
    return padIsConnected(&s_pads[userIndex]);
}

static SHORT nxStick(s32 v)
{
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    return (SHORT)v;
}

DWORD XInputGetState(DWORD userIndex, XINPUT_STATE *state)
{
    if (!state || !nxPadPoll(userIndex))
        return NX_ERROR_DEVICE_NOT_CONNECTED;

    memset(state, 0, sizeof(*state));
    XINPUT_GAMEPAD *gp = &state->Gamepad;
    u64 keys = padGetButtons(&s_pads[userIndex]);
#ifdef KISAK_NX2
    if (!userIndex) {
        s_mouseClaimHeld = (s_mouseClaim | s_mouseClaimHeld) & keys;
        keys &= ~s_mouseClaimHeld;
    }
#endif

    // Face buttons are mapped by POSITION, not by label. The game is written
    // for an Xbox pad, where A (bottom) confirms and B (right) cancels, and
    // Nintendo swaps both pairs: bottom is B, right is A, left is Y, top is X.
    // Mapping by position keeps "bottom confirms, right cancels" and every
    // other face-button binding under the thumb where the game expects it.
    // Consequence: on-screen prompts still show Xbox glyphs, so a prompt that
    // says "A" means the Switch B button and "X" means the Switch Y button.
    WORD buttons = 0;
    if (keys & HidNpadButton_B) buttons |= XINPUT_GAMEPAD_A; // bottom
    if (keys & HidNpadButton_A) buttons |= XINPUT_GAMEPAD_B; // right
    if (keys & HidNpadButton_Y) buttons |= XINPUT_GAMEPAD_X; // left
    if (keys & HidNpadButton_X) buttons |= XINPUT_GAMEPAD_Y; // top
    if (keys & HidNpadButton_Plus) buttons |= XINPUT_GAMEPAD_START;
    if (keys & HidNpadButton_Minus) buttons |= XINPUT_GAMEPAD_BACK;
    if (keys & HidNpadButton_L) buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
    if (keys & HidNpadButton_R) buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
    if (keys & HidNpadButton_StickL) buttons |= XINPUT_GAMEPAD_LEFT_THUMB;
    if (keys & HidNpadButton_StickR) buttons |= XINPUT_GAMEPAD_RIGHT_THUMB;
    if (keys & HidNpadButton_Up) buttons |= XINPUT_GAMEPAD_DPAD_UP;
    if (keys & HidNpadButton_Down) buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
    if (keys & HidNpadButton_Left) buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
    if (keys & HidNpadButton_Right) buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
    gp->wButtons = buttons;

    // ZL/ZR are digital on every Switch controller: fully released or fully
    // pulled, never in between.
    gp->bLeftTrigger = (keys & HidNpadButton_ZL) ? 255 : 0;
    gp->bRightTrigger = (keys & HidNpadButton_ZR) ? 255 : 0;

    // HID sticks span +-32767 with +Y up, the same convention as XInput.
    HidAnalogStickState l = padGetStickPos(&s_pads[userIndex], 0);
    HidAnalogStickState r = padGetStickPos(&s_pads[userIndex], 1);
    gp->sThumbLX = nxStick(l.x);
    gp->sThumbLY = nxStick(l.y);
    gp->sThumbRX = nxStick(r.x);
    gp->sThumbRY = nxStick(r.y);

    state->dwPacketNumber = ++s_packet[userIndex];
    return ERROR_SUCCESS;
}

DWORD XInputSetState(DWORD userIndex, XINPUT_VIBRATION *vibration)
{
    (void)userIndex;
    (void)vibration; // TODO: HidVibration
    return ERROR_SUCCESS;
}

DWORD XInputGetCapabilities(DWORD userIndex, DWORD, XINPUT_CAPABILITIES *caps)
{
    // GPad_Check derives GamePad::enabled from this return value, which in
    // turn fires the game's inserted/removed callbacks.
    if (!nxPadPoll(userIndex))
        return NX_ERROR_DEVICE_NOT_CONNECTED;
    if (caps) {
        memset(caps, 0, sizeof(*caps));
        caps->Type = 1;    // XINPUT_DEVTYPE_GAMEPAD
        caps->SubType = 1; // XINPUT_DEVSUBTYPE_GAMEPAD
        caps->Gamepad.wButtons = 0xFFFF;
        caps->Gamepad.bLeftTrigger = 255;
        caps->Gamepad.bRightTrigger = 255;
        caps->Gamepad.sThumbLX = 32767;
        caps->Gamepad.sThumbLY = 32767;
        caps->Gamepad.sThumbRX = 32767;
        caps->Gamepad.sThumbRY = 32767;
        caps->Vibration.wLeftMotorSpeed = 65535;
        caps->Vibration.wRightMotorSpeed = 65535;
    }
    return ERROR_SUCCESS;
}

void XInputEnable(BOOL) {}

// Motion aim (CL_NX_GyroAim, cl_input_mp.cpp). Each pad slot's six-axis
// sensors start on first use: the handheld console (slot zero only), a Pro
// Controller, and the two halves of a Joy-Con pair.
struct NxGyro
{
    HidSixAxisSensorHandle handheld, fullKey, dual[2];
    bool started;
    u32 handle;            // the sensor read last; a new one starts over
    u64 lastSample;
    float gravity[3];      // low-passed acceleration, in G
    bool haveGravity;
    float yawRate, pitchRate;
};
static NxGyro s_gyro[NX_PAD_COUNT];

static void nxGyroStart(DWORD userIndex, NxGyro &g)
{
    const HidNpadIdType id = userIndex ? HidNpadIdType_No2 : HidNpadIdType_No1;
    if (!userIndex) {
        hidGetSixAxisSensorHandles(&g.handheld, 1, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
        hidStartSixAxisSensor(g.handheld);
    }
    hidGetSixAxisSensorHandles(&g.fullKey, 1, id, HidNpadStyleTag_NpadFullKey);
    hidGetSixAxisSensorHandles(g.dual, 2, id, HidNpadStyleTag_NpadJoyDual);
    hidStartSixAxisSensor(g.fullKey);
    hidStartSixAxisSensor(g.dual[0]);
    hidStartSixAxisSensor(g.dual[1]);
    g.started = true;
}

// The pad's turn rates in degrees per second, from the samples since the last
// call (the sensor runs faster than the game). Yaw is about the world's
// vertical, so it reads the same whether the controller lies flat or the
// console stands upright: positive turns left. Positive pitch tilts the view
// up. False without a sensor (single sideways Joy-Con, nothing connected).
extern "C" bool NX_GyroRate(unsigned userIndex, float *yawRate, float *pitchRate)
{
    *yawRate = *pitchRate = 0.0f;
    if (userIndex >= NX_PAD_COUNT || !s_padInit)
        return false;
    NxGyro &g = s_gyro[userIndex];
    const PadState *pad = &s_pads[userIndex];
    if (!g.started)
        nxGyroStart(userIndex, g);

    const u32 style = padGetStyleSet(pad);
    HidSixAxisSensorHandle handle;
    if (!userIndex && (style & HidNpadStyleTag_NpadHandheld))
        handle = g.handheld;
    else if (style & HidNpadStyleTag_NpadFullKey)
        handle = g.fullKey;
    else if (style & HidNpadStyleTag_NpadJoyDual) {
        // Aim with the right hand's Joy-Con when it is there.
        const u32 attributes = padGetAttributes(pad);
        if (attributes & HidNpadAttribute_IsRightConnected)
            handle = g.dual[1];
        else if (attributes & HidNpadAttribute_IsLeftConnected)
            handle = g.dual[0];
        else
            return false;
    } else
        return false;
    if (handle.type_value != g.handle) {
        g.handle = handle.type_value;
        g.lastSample = 0;
        g.haveGravity = false;
        g.yawRate = g.pitchRate = 0.0f;
    }

    HidSixAxisSensorState states[17];
    const size_t count = hidGetSixAxisSensorStates(handle, states, 17);
    u64 newest = 0;
    for (size_t i = 0; i < count; ++i)
        if (states[i].sampling_number > newest)
            newest = states[i].sampling_number;
    if (!g.lastSample && newest)
        g.lastSample = newest - 1;   // a new sensor: only its latest sample counts

    // Oldest first: the states come newest first.
    float w[3] = {}, used = 0.0f;
    for (size_t k = count; k-- > 0;) {
        const HidSixAxisSensorState &s = states[k];
        if (s.sampling_number <= g.lastSample || !(s.attributes & HidSixAxisSensorAttribute_IsConnected))
            continue;
        const float a[3] = { s.acceleration.x, s.acceleration.y, s.acceleration.z };
        for (int j = 0; j < 3; ++j)
            g.gravity[j] = g.haveGravity ? g.gravity[j] + (a[j] - g.gravity[j]) * 0.02f : a[j];
        g.haveGravity = true;
        w[0] += s.angular_velocity.x;
        w[1] += s.angular_velocity.y;
        w[2] += s.angular_velocity.z;
        used += 1.0f;
    }
    if (newest > g.lastSample)
        g.lastSample = newest;
    if (used == 0.0f) {
        // No new sample since the last frame: hold the last rate.
        *yawRate = g.yawRate;
        *pitchRate = g.pitchRate;
        return g.haveGravity;
    }
    // Revolutions per second to degrees per second.
    for (int j = 0; j < 3; ++j)
        w[j] *= 360.0f / used;

    // X is the controller's left-right axis, so it is always pitch. Yaw is the
    // turn about "up", which lies between the controller's Y (up when the
    // console stands in the hands) and Z (up when a controller lies flat):
    // the gravity vector says how much of each. Its sign is whichever makes
    // up face out of the controller's front, as it does in any natural grip.
    // Roll (turning about the forward axis) is left out of the projection
    // and the result relaxed toward the local turn rate, so a wrist that
    // twists while it turns still turns at the expected speed.
    float yaw = w[2];
    const float gy = g.gravity[1], gz = g.gravity[2];
    const float len = sqrtf(gy * gy + gz * gz);
    if (len > 0.25f) {
        float uy = gy / len, uz = gz / len;
        if (uy + uz < 0.0f) {
            uy = -uy;
            uz = -uz;
        }
        const float world = uy * w[1] + uz * w[2];
        const float local = sqrtf(w[1] * w[1] + w[2] * w[2]);
        const float relaxed = fminf(fabsf(world) * 1.41f, local);
        yaw = world < 0.0f ? -relaxed : relaxed;
    }
    float pitch = w[0];

    // Soft tightening: rates under a degree per second (sensor noise while
    // the hands hold still) shrink toward zero instead of drifting the view.
    constexpr float kTighten = 1.0f;
    const float mag = sqrtf(yaw * yaw + pitch * pitch);
    if (mag < kTighten) {
        yaw *= mag / kTighten;
        pitch *= mag / kTighten;
    }
    g.yawRate = *yawRate = yaw;
    g.pitchRate = *pitchRate = pitch;
    return true;
}

#ifdef KISAK_NX2
// libounce's PadPointer follows the Joy-Con of controller one that last moved
// or changed a button as a mouse (HID Nmouse). By default its L/ZL or R/ZR
// give the left button.
static PadPointer s_pointer;
static bool s_pointerTried, s_pointerOpen;

bool NX_JoyConMouse(NxJoyConMouse *mouse)
{
    if (!nxPadPoll(0))
        return false;
    if (!s_pointerTried) {
        s_pointerTried = true;
        s_pointerOpen = R_SUCCEEDED(padPointerInitialize(&s_pointer, NX_JOYCON_MOUSE_WIDTH, NX_JOYCON_MOUSE_HEIGHT));
    }
    if (!s_pointerOpen)
        return false;
    padPointerUpdate(&s_pointer, &s_pads[0]);
    if (!s_pointer.active)
        return false;
    mouse->x = s_pointer.x;
    mouse->y = s_pointer.y;
    mouse->dx = s_pointer.delta_x;
    mouse->dy = s_pointer.delta_y;
    mouse->wheel = s_pointer.wheel_delta_y;
    mouse->buttons = padPointerGetButtons(&s_pointer);
    return true;
}

void NX_JoyConMouseClaimClicks(bool claim)
{
    s_mouseClaim = 0;
    if (!claim || !s_pointerOpen || !s_pointer.active)
        return;
    if (s_pointer.side == PadPointerSide_Left)
        s_mouseClaim = HidNpadButton_L | HidNpadButton_ZL;
    else if (s_pointer.side == PadPointerSide_Right)
        s_mouseClaim = HidNpadButton_R | HidNpadButton_ZR;
}
#endif

// Before exit: the next program runs in this process, so the six-axis sensors
// stop and the Joy-Con mouse gives its memory back, which HID would keep
// writing to.
extern "C" void NX_InputExit(void)
{
    for (DWORD userIndex = 0; userIndex < NX_PAD_COUNT; ++userIndex) {
        NxGyro &g = s_gyro[userIndex];
        if (!g.started)
            continue;
        if (!userIndex)
            hidStopSixAxisSensor(g.handheld);
        hidStopSixAxisSensor(g.fullKey);
        hidStopSixAxisSensor(g.dual[0]);
        hidStopSixAxisSensor(g.dual[1]);
        g.started = false;
    }
#ifdef KISAK_NX2
    if (s_pointerOpen)
        padPointerExit(&s_pointer);
    s_pointerOpen = false;
#endif
}

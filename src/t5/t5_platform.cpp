// t5_platform.cpp - T5Jool (Linux, Android): the small Switch services the engine calls
// (src/nx/nx_bink_clock.cpp, nx_load_boost.cpp, nx_update.cpp, nx_saltynx.cpp, the Joy-Con mouse),
// reimplemented or reduced to what makes sense off the Switch.
#include <binklib/bink_audio.h>
#include <switch.h>

#include "../nx/nx_load_boost.h"
#include "../nx/nx_update.h"
#include "../nx/nx_saltynx.h"
#include "../nx/nx_joycon_mouse.h"

#include <mutex>
#include <stdio.h>
#include <string.h>

// ------------------------------------------------------------------ Bink clock: stops while the app is in background
namespace
{
std::mutex s_binkMutex;
bool s_binkFocused = true;
uint64_t s_binkBackgroundAt, s_binkBackgroundTime;

uint64_t BinkSystemTime()
{
    return armTicksToNs(armGetSystemTick()) / 1000;
}
}

void Sys_BinkInitClock()
{
    std::lock_guard<std::mutex> lock(s_binkMutex);
    s_binkBackgroundAt = BinkSystemTime();
}

// called by the SDL event loop (t5_main.cpp) on focus / background changes
void T5_SetAppFocus(bool active)
{
    std::lock_guard<std::mutex> lock(s_binkMutex);
    if (active == s_binkFocused)
        return;
    const uint64_t now = BinkSystemTime();
    if (active)
        s_binkBackgroundTime += now - s_binkBackgroundAt;
    else
        s_binkBackgroundAt = now;
    s_binkFocused = active;
}

bool Sys_BinkHasFocus()
{
    std::lock_guard<std::mutex> lock(s_binkMutex);
    return s_binkFocused;
}

uint64_t Sys_BinkTimeUs()
{
    std::lock_guard<std::mutex> lock(s_binkMutex);
    return (s_binkFocused ? BinkSystemTime() : s_binkBackgroundAt) - s_binkBackgroundTime;
}

// ------------------------------------------------------------------ CPU boost while loading: the OS governs clocks here
void NX_LoadBoostInit() {}
void NX_LoadBoostSet(NxLoadPhase, bool) {}
void NX_LoadBoostReset(const char *) {}
void NX_LoadBoostUpdate() {}
void NX_LoadBoostEndBootWhenLoaded() {}
// "loading in background" lets the zone loader overlap work with the render thread; the Switch only
// allows it while boosted. Phones and PCs have the cores for it at any time.
bool NX_LoadBoostBackgroundLoading() { return true; }
void NX_LoadBoostExit() {}
bool NX_LoadBoostActive() { return false; }

// ------------------------------------------------------------------ the Switch NRO updater: T5Jool updates through its launcher
const char *NX_UpdateCurrentVersion() { return "t5jool"; }
void NX_UpdateApplyPending() {}
void NX_UpdateStartCheck() {}
void NX_UpdateStartInstall() {}
void NX_UpdateCancel() {}
void NX_UpdateStop() {}
void NX_UpdateGetInfo(NxUpdateInfo &info)
{
    memset(&info, 0, sizeof(info));
    info.phase = NxUpdatePhase::Idle;
}

// ------------------------------------------------------------------ SaltyNX (Switch FPS overlay plugin)
#ifndef KISAK_NX2
void NX_SaltyNXStart() {}
void NX_SaltyNXStop() {}
void NX_SaltyNXFrame(unsigned, unsigned) {}
#endif

// ------------------------------------------------------------------ Joy-Con as a mouse: no such thing here
bool NX_JoyConMouse(NxJoyConMouse *mouse)
{
    if (mouse)
        memset(mouse, 0, sizeof(*mouse));
    return false;
}
void NX_JoyConMouseClaimClicks(bool) {}

// ------------------------------------------------------------------ hooks of the Switch's DXVK build (Sarek, src/nx/dxvk)
// DXVK-native is a separate library here: its device lock and counters are not reachable. Every
// call on the device takes DXVK's own lock (the device is created multithreaded), which is what
// the zone loader's background texture uploads need.
extern "C" void NX_DxvkHoldDeviceLock(void *, bool) {}
extern "C" void NX_DxvkReportCounters(void *) {}
extern "C" void NX_DxvkInitThreadAffinity() {}
extern "C" void NX_DxvkSetThreadName(const char *) {}

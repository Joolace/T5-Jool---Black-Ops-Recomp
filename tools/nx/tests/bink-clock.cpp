#include "bink_audio.h"
#include <switch.h>
#include <cassert>

uint64_t ticks;
AppletFocusState focus = AppletFocusState_InFocus;
void (*callback)(AppletHookType, void *);
uint64_t armGetSystemTick() { return ticks; }
uint64_t armTicksToNs(uint64_t tick) { return tick; }
AppletFocusState appletGetFocusState() { return focus; }
void appletHook(AppletHookCookie *, void (*fn)(AppletHookType, void *), void *) { callback = fn; }

int main()
{
    ticks = 1000000;
    Sys_BinkInitClock();
    assert(Sys_BinkHasFocus() && Sys_BinkTimeUs() == 1000);
    ticks += 1000000;
    focus = AppletFocusState_OutOfFocus;
    callback(AppletHookType_OnFocusState, nullptr);
    const auto paused = Sys_BinkTimeUs();
    ticks += 60000000000;
    assert(!Sys_BinkHasFocus() && Sys_BinkTimeUs() == paused);
    callback(AppletHookType_OnFocusState, nullptr);
    focus = AppletFocusState_InFocus;
    callback(AppletHookType_OnResume, nullptr);
    assert(Sys_BinkHasFocus() && Sys_BinkTimeUs() == paused);
    ticks += 1000000;
    assert(Sys_BinkTimeUs() == paused + 1000);
    Sys_BinkInitClock();
    focus = AppletFocusState_OutOfFocus;
    callback(AppletHookType_OnFocusState, nullptr);
    ticks += 1000000000;
    focus = AppletFocusState_InFocus;
    callback(AppletHookType_OnFocusState, nullptr);
    assert(Sys_BinkTimeUs() == paused + 1000);
}

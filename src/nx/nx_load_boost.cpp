#include "nx_load_boost.h"

#include <switch.h>
#include <atomic>
#include <cstdio>
#include <mutex>

bool DB_FinishedLoadingAssets();

namespace
{
std::mutex s_mutex;
std::atomic<bool> s_active{false};
std::atomic<bool> s_retryPending{false};
bool s_initialized, s_supported, s_stopping, s_hooked;
unsigned s_phases;
u64 s_retryAfter;
bool s_bootEndPending = false;
u64 s_bootEndBy;
AppletHookCookie s_hook;

#ifdef KISAK_NX2
// On Switch 2 FastLoad drops the GPU to its minimum clock and
// CancelCpuBoostMode does not undo it: a boost ends with Normal, then cancel,
// and boosting stops when neither restores the configuration.
u32 s_boostFrom;
AppletOperationMode s_boostOperation;

u32 performanceConfiguration()
{
    u32 configuration = 0;
    appletGetCurrentPerformanceConfiguration(&configuration);
    return configuration;
}

// Waits up to 100 ms; docking also ends the boost.
bool boostRestored()
{
    for (int i = 0; i < 20; ++i)
    {
        if (appletGetOperationMode() != s_boostOperation || performanceConfiguration() == s_boostFrom)
            return true;
        svcSleepThread(5000000);
    }
    return false;
}

Result endBoost()
{
    const Result normal = appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
    if (R_SUCCEEDED(normal) && boostRestored())
        return normal;
    const Result cancel = appletCancelCpuBoostMode();
    if (R_SUCCEEDED(cancel) && boostRestored())
        return cancel;
    s_supported = false;
    return 0;
}
#endif

void applyBoost(const char *reason, bool force = false)
{
    if (!s_supported)
        return;
    const bool wanted = s_phases && !s_stopping
        && appletGetFocusState() == AppletFocusState_InFocus;
    if (wanted == s_active.load() && !force)
    {
        s_retryPending = false;
        return;
    }

    // FastLoad also lowers the GPU clock; Normal restores the system profile.
#ifdef KISAK_NX2
    if (wanted && !s_active.load())
    {
        s_boostFrom = performanceConfiguration();
        s_boostOperation = appletGetOperationMode();
    }
    Result rc = wanted ? appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad) : endBoost();
#else
    Result rc = appletSetCpuBoostMode(wanted ? ApmCpuBoostMode_FastLoad : ApmCpuBoostMode_Normal);
#endif
    if (R_FAILED(rc))
    {
        std::printf("[nx-load-boost] CPU boost %s failed: %08x (%s)\n",
                    wanted ? "enable" : "restore", unsigned(rc), reason);
        if (wanted && !s_active.load())
            s_supported = false;
        s_retryPending = !wanted && s_active.load();
        s_retryAfter = armTicksToNs(armGetSystemTick()) + 500000000ull;
        return;
    }

    s_active = wanted;
    s_retryPending = false;
    std::printf("[nx-load-boost] CPU boost %s (%s; phases=%u)\n",
                wanted ? "on" : "off", reason, s_phases);
}

void appletHookCallback(AppletHookType type, void *)
{
    if (type != AppletHookType_OnFocusState && type != AppletHookType_OnResume)
        return;
    std::lock_guard<std::mutex> lock(s_mutex);
    const bool reapply = type == AppletHookType_OnResume && s_active.load();
    applyBoost(type == AppletHookType_OnResume ? "resume" : "focus", reapply);
}
}

void NX_LoadBoostInit()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_initialized)
        return;
    s_initialized = true;
    s_supported = hosversionAtLeast(7, 0, 0);
    s_phases = unsigned(NxLoadPhase::Boot);
    if (!s_supported)
    {
        std::printf("[nx-load-boost] CPU loading boost requires Horizon 7.0.0 or later\n");
        return;
    }
    appletHook(&s_hook, appletHookCallback, nullptr);
    s_hooked = true;
    applyBoost("boot");
}

void NX_LoadBoostSet(NxLoadPhase phase, bool active)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_initialized || s_stopping)
        return;
    if (active)
        s_phases |= unsigned(phase);
    else
        s_phases &= ~unsigned(phase);
    const char *reason = phase == NxLoadPhase::Boot ? "boot"
        : phase == NxLoadPhase::Server ? "server load" : "client load";
    applyBoost(reason);
}

void NX_LoadBoostReset(const char *reason)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_phases = 0;
    s_bootEndPending = false;
    applyBoost(reason);
}

void NX_LoadBoostEndBootWhenLoaded()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_initialized || s_stopping)
        return;
    s_bootEndBy = armTicksToNs(armGetSystemTick()) + 120000000000ull;
    s_bootEndPending = true;
}

bool NX_LoadBoostBackgroundLoading()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_bootEndPending && !s_stopping
        && !(s_phases & (unsigned(NxLoadPhase::Server) | unsigned(NxLoadPhase::Client)));
}

void NX_LoadBoostUpdate()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_bootEndPending
        && (DB_FinishedLoadingAssets() || armTicksToNs(armGetSystemTick()) >= s_bootEndBy)) {
        s_bootEndPending = false;
        s_phases &= ~unsigned(NxLoadPhase::Boot);
        applyBoost("boot");
    }
    if (s_retryPending.load() && armTicksToNs(armGetSystemTick()) >= s_retryAfter)
        applyBoost("restore retry");
}

void NX_LoadBoostExit()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_stopping = true;
    s_bootEndPending = false;
    s_phases = 0;
    applyBoost("exit");
    if (s_hooked)
    {
        appletUnhook(&s_hook);
        s_hooked = false;
    }
}

bool NX_LoadBoostActive()
{
    return s_active.load();
}

// nx_display.cpp -- the default window's size and present rate.
//
// Switch 1: 1280x720, presented every swap interval.
//
// Switch 2: the window is 1920x1080 handheld and the TV's resolution docked,
// up to 2160 lines. Screen Refresh Rate (r_displayRefresh) is kept per mode,
// 120 Hz handheld and 60 Hz docked by default, and is the frame rate cap.
// 120 Hz uses nwindowSetFrameRates with VRR and caps the docked window at 1440
// lines.
// The renderer draws at an internal size that DXVK scales to the window:
// docked 1440 lines (1080 at 120 Hz), handheld 1080 (720 at 120 Hz). A change
// of mode or refresh rate restarts the renderer at the new size.
#include "nx_display.h"
#include "nx_install.h"

#include <qcommon/cmd.h>
#include <universal/dvar.h>

#include <switch.h>

#include <atomic>
#include <cstdio>

namespace
{
#ifdef KISAK_NX2
std::atomic<bool> s_highRefresh{false};
std::atomic<int> s_refreshRate{60};
bool s_presentSet, s_presentHigh;

const dvar_s *s_handheldHz, *s_dockedHz;
bool s_modeKnown, s_docked, s_rendererUp;
int s_syncedHz;

bool s_tvEventOpen, s_tvRead;
Event s_tvEvent;
s32 s_tvWidth = 3840, s_tvHeight = 2160;
u32 s_dockedWidth, s_dockedHeight;

int s_renderWidth, s_renderHeight;
bool s_restartQueued;

// r_displayRefresh in Hz, 0 before the renderer registers it. A new rate
// applies at once rather than at the next renderer restart.
int refreshSetting()
{
    static const dvar_s *refresh;
    if (!refresh)
        refresh = Dvar_FindVar("r_displayRefresh");
    if (refresh && Dvar_HasLatchedValue(refresh))
        Dvar_MakeLatchedValueCurrent((dvar_s *)refresh);
    int hz = 0;
    if (!refresh || std::sscanf(Dvar_EnumToString(refresh), "%i", &hz) != 1)
        return 0;
    return hz;
}

void setRefreshSetting(int hz)
{
    char value[16];
    std::snprintf(value, sizeof(value), "%i Hz", hz);
    Dvar_SetFromStringByName("r_displayRefresh", value);
    if (const dvar_s *refresh = Dvar_FindVar("r_displayRefresh"))
        Dvar_MakeLatchedValueCurrent((dvar_s *)refresh);
}

// The current mode's refresh rate. A rate chosen in the options becomes that
// mode's setting.
int syncRefresh()
{
    if (!s_handheldHz)
    {
        s_handheldHz = _Dvar_RegisterInt("nx_refreshHandheld", 120, 60, 120, 1u, "Handheld screen refresh rate (Hz)");
        s_dockedHz = _Dvar_RegisterInt("nx_refreshDocked", 60, 60, 120, 1u,
            "Docked screen refresh rate (Hz); 120 renders at most 1080 lines");
    }
    const bool docked = appletGetOperationMode() == AppletOperationMode_Console;
    const dvar_s *setting = docked ? s_dockedHz : s_handheldHz;
    int hz = refreshSetting();
    if (!hz)
        return 60;
    if (s_modeKnown && docked == s_docked && hz != s_syncedHz)
        Dvar_SetInt((dvar_s *)setting, hz);
    else if (hz != setting->current.integer)
    {
        setRefreshSetting(setting->current.integer);
        hz = refreshSetting();
    }
    s_modeKnown = true;
    s_docked = docked;
    s_syncedHz = hz;
    s_refreshRate.store(hz, std::memory_order_relaxed);
    s_highRefresh.store(hz > 60, std::memory_order_relaxed);
    return hz;
}

// The TV's resolution, reread when AM signals a change.
void readTvResolution()
{
    if (!s_tvEventOpen && R_SUCCEEDED(appletGetDefaultDisplayResolutionChangeEvent(&s_tvEvent)))
        s_tvEventOpen = true;
    const bool changed = s_tvEventOpen && R_SUCCEEDED(eventWait(&s_tvEvent, 0));
    if (s_tvRead && !changed)
        return;
    if (changed)
        eventClear(&s_tvEvent);
    s32 width = 0, height = 0;
    if (R_SUCCEEDED(appletGetDefaultDisplayResolution(&width, &height)) && width > 0 && height > 0)
    {
        s_tvWidth = width;
        s_tvHeight = height;
    }
    s_tvRead = true;
}

void applyWindowSize(bool high)
{
    readTvResolution();
    u32 width = (u32)s_tvWidth, height = (u32)s_tvHeight;
    const u32 maxHeight = high ? 1440 : 2160;
    if (height > maxHeight)
    {
        width = width * maxHeight / height;
        height = maxHeight;
    }
    if (width == s_dockedWidth && height == s_dockedHeight)
        return;
    if (R_SUCCEEDED(nwindowSetModeDimensions(nwindowGetDefault(), NX_SCREEN_WIDTH, NX_SCREEN_HEIGHT, width, height)))
    {
        s_dockedWidth = width;
        s_dockedHeight = height;
    }
}

// The internal size for the current mode, with the window's aspect ratio and
// never taller than the window.
void renderSize(bool high, int *width, int *height)
{
    applyWindowSize(high);
    u32 w = NX_SCREEN_WIDTH, h = NX_SCREEN_HEIGHT;
    nwindowGetDimensions(nwindowGetDefault(), &w, &h);
    const bool docked = appletGetOperationMode() == AppletOperationMode_Console;
    u32 lines = docked ? (high ? 1080 : 1440) : (high ? 720 : 1080);
    if (lines > h)
        lines = h;
    *width = (int)((((u64)w * lines / h) + 1) & ~1ull);
    *height = (int)lines;
}
#endif
}

void NX_DisplayInit()
{
#ifdef KISAK_NX2
    applyWindowSize(false);
#endif
}

void NX_DisplayRenderSize(int *width, int *height)
{
#ifdef KISAK_NX2
    renderSize(syncRefresh() > 60, width, height);
    s_renderWidth = *width;
    s_renderHeight = *height;
    s_restartQueued = false;
    s_rendererUp = true;
#else
    *width = NX_SCREEN_WIDTH;
    *height = NX_SCREEN_HEIGHT;
#endif
}

bool NX_DisplayRenderSizeChanged(int width, int height)
{
#ifdef KISAK_NX2
    s_restartQueued = false;
    int w, h;
    renderSize(syncRefresh() > 60, &w, &h);
    return w != width || h != height;
#else
    (void)width;
    (void)height;
    return false;
#endif
}

int NX_DisplayRefreshRate()
{
#ifdef KISAK_NX2
    return s_refreshRate.load(std::memory_order_relaxed);
#else
    return 60;
#endif
}

void NX_DisplayUpdate()
{
#ifdef KISAK_NX2
    if (!s_rendererUp)
        return;
    int width, height;
    renderSize(syncRefresh() > 60, &width, &height);
    if ((width != s_renderWidth || height != s_renderHeight) && !s_restartQueued)
    {
        s_restartQueued = true;
        Cbuf_AddText(0, "vid_restart\n");
    }
#endif
}

// The present rate, from DXVK's present (tools/nx/patch-dxvk-sarek.py). On
// Switch 2 every frame is presented at the refresh rate, so v-sync does not
// apply.
extern "C" Result NX_DxvkSetSyncInterval(uint32_t syncInterval)
{
    NWindow *window = nwindowGetDefault();
#ifdef KISAK_NX2
    (void)syncInterval;
    const bool high = s_highRefresh.load(std::memory_order_relaxed);
    if (s_presentSet && high == s_presentHigh)
        return 0;
    const Result rc = high ? nwindowSetFrameRates(window, 0, 0, true) : nwindowSetSwapInterval(window, 1);
    if (R_SUCCEEDED(rc))
    {
        s_presentSet = true;
        s_presentHigh = high;
    }
    return rc;
#else
    return nwindowSetSwapInterval(window, syncInterval != 0);
#endif
}

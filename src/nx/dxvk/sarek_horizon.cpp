#include "sarek_horizon.h"
#include "../nx_install.h"
#include "util/util_shared_res.h"
#include "wsi/wsi_platform.h"
#include <vulkan/vulkan.h>
#include <iterator>

extern "C" PFN_vkVoidFunction nvk_loaderless_GetInstanceProcAddr(VkInstance,const char*);
extern "C" VkResult NX_SarekCreateSurface(VkInstance instance,VkSurfaceKHR *surface) {
    auto create=reinterpret_cast<PFN_vkCreateViSurfaceNN>(nvk_loaderless_GetInstanceProcAddr(instance,"vkCreateViSurfaceNN"));
    if (!create) return VK_ERROR_EXTENSION_NOT_PRESENT;
    VkViSurfaceCreateInfoNN info{VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN};
    info.window=nwindowGetDefault();
    return create(instance,&info,nullptr,surface);
}
namespace dxvk {
HANDLE openKmtHandle(HANDLE) { return INVALID_HANDLE_VALUE; }
bool setSharedMetadata(HANDLE,void*,uint32_t) { return false; }
bool getSharedMetadata(HANDLE,void*,uint32_t,uint32_t*) { return false; }
}

namespace dxvk::wsi {
void init() {}
void quit() {}

HMONITOR getDefaultMonitor() { return reinterpret_cast<HMONITOR>(1); }
HMONITOR enumMonitors(uint32_t index) { return index == 0 ? getDefaultMonitor() : nullptr; }

bool getDisplayName(HMONITOR monitor, WCHAR (&name)[32])
{
    if (!monitor)
        return false;
    std::wcscpy(name, L"Horizon");
    return true;
}

bool getDesktopCoordinates(HMONITOR monitor, RECT *rect)
{
    return monitor && GetWindowRect(nullptr, rect);
}

void getWindowSize(HWND, uint32_t *width, uint32_t *height)
{
    u32 w = NX_SCREEN_WIDTH, h = NX_SCREEN_HEIGHT;
    nwindowGetDimensions(nwindowGetDefault(), &w, &h);
    if (width) *width = w;
    if (height) *height = h;
}

// The screen's own mode comes last.
static const WsiMode s_displayModes[] = {
#ifdef KISAK_NX2
    {960, 540, {60, 1}, 32, false},
    {960, 540, {120, 1}, 32, false},
    {1280, 720, {60, 1}, 32, false},
    {1280, 720, {120, 1}, 32, false},
    {1920, 1080, {60, 1}, 32, false},
    {1920, 1080, {120, 1}, 32, false},
#else
    {960, 540, {60, 1}, 32, false},
    {1280, 720, {60, 1}, 32, false},
#endif
};

bool getDisplayMode(HMONITOR monitor, uint32_t index, WsiMode *mode)
{
    if (!monitor || index >= std::size(s_displayModes))
        return false;
    *mode = s_displayModes[index];
    return true;
}

bool getCurrentDisplayMode(HMONITOR monitor, WsiMode *mode)
{
    if (!getDisplayMode(monitor, std::size(s_displayModes) - 1, mode))
        return false;
    getWindowSize(nullptr, &mode->width, &mode->height);
    return true;
}

// Horizon owns the display mode and native window. D3D9 renders at its
// configured back-buffer size and the Vulkan swap chain handles scaling.
void resizeWindow(HWND, DxvkWindowState *, uint32_t, uint32_t) {}
void saveWindowState(HWND, DxvkWindowState *, bool) {}
void restoreWindowState(HWND, DxvkWindowState *, bool) {}
bool setWindowMode(HMONITOR monitor, HWND, DxvkWindowState *, const WsiMode &) { return monitor != nullptr; }
bool enterFullscreenMode(HMONITOR monitor, HWND window, DxvkWindowState *, bool) { return monitor && isWindow(window); }
bool leaveFullscreenMode(HWND window, DxvkWindowState *) { return isWindow(window); }
bool restoreDisplayMode() { return true; }
HMONITOR getWindowMonitor(HWND window) { return isWindow(window) ? getDefaultMonitor() : nullptr; }
bool isWindow(HWND window) { return IsWindow(window); }
bool isMinimized(HWND) { return false; }
bool isOccluded(HWND) { return false; }
void updateFullscreenWindow(HMONITOR, HWND, bool) {}

VkResult createSurface(HWND, PFN_vkGetInstanceProcAddr, VkInstance instance, VkSurfaceKHR *surface)
{
    return NX_SarekCreateSurface(instance, surface);
}
}

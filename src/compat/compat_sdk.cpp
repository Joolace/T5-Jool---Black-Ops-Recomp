// Stand-ins for x86-only SDK libraries on 64-bit builds: dxerr (June 2010
// DirectX SDK), NVAPI and the libvpx clip encoder wrapper (src/vpx/vpx.cpp).
#include <windows.h>
#include <stdio.h>

#include <dxerr.h>
#include <nvapi/nvapi.h>

const char *DXGetErrorDescriptionA(HRESULT hr)
{
    static char buf[48];
    snprintf(buf, sizeof(buf), "HRESULT 0x%08X", (unsigned)hr);
    return buf;
}

const char *DXGetErrorStringA(HRESULT hr)
{
    return DXGetErrorDescriptionA(hr);
}

NvAPI_Status NvAPI_Initialize(void) { return NVAPI_ERROR; }
NvAPI_Status NvAPI_Unload(void) { return NVAPI_OK; }
NvAPI_Status NvAPI_GetDisplayDriverVersion(void *, NV_DISPLAY_DRIVER_VERSION *) { return NVAPI_ERROR; }
NvAPI_Status NvAPI_Stereo_CreateHandleFromIUnknown(void *, StereoHandle *handle)
{
    if (handle) *handle = NULL;
    return NVAPI_ERROR;
}
NvAPI_Status NvAPI_Stereo_DestroyHandle(StereoHandle) { return NVAPI_OK; }
NvAPI_Status NvAPI_Stereo_IsActivated(StereoHandle, NvU8 *activated)
{
    if (activated) *activated = 0;
    return NVAPI_ERROR;
}
NvAPI_Status NvAPI_Stereo_IsEnabled(NvU8 *enabled)
{
    if (enabled) *enabled = 0;
    return NVAPI_ERROR;
}
NvAPI_Status NvAPI_Stereo_SetConvergence(StereoHandle, float) { return NVAPI_ERROR; }
NvAPI_Status NvAPI_D3D9_GetTextureHandle(void *, NVDX_ObjectHandle *handle)
{
    if (handle) *handle = NVDX_OBJECT_NONE;
    return NVAPI_ERROR;
}
NvAPI_Status NvAPI_D3D9_GetCurrentZBufferHandle(void *, NVDX_ObjectHandle *handle)
{
    if (handle) *handle = NVDX_OBJECT_NONE;
    return NVAPI_ERROR;
}
NvAPI_Status NvAPI_D3D9_StretchRect(void *, NVDX_ObjectHandle, const RECT *, NVDX_ObjectHandle, const RECT *, int)
{
    return NVAPI_ERROR;
}
NvAPI_Status NvAPI_D3D_GetCurrentSLIState(void *, NV_GET_CURRENT_SLI_STATE *) { return NVAPI_ERROR; }

void __cdecl vpx_init(const char *, int, int) {}
void __cdecl vpx_encode_frame(unsigned char *, unsigned char *, unsigned char *, bool) {}
void __cdecl vpx_shutdown() {}

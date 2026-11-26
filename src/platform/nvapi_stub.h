// nvapi_stub.h - NVAPI esiste solo su Windows. NvAPI_Initialize fallisce e il motore salta stereo/SLI/INTZ.
#pragma once
#include <stdint.h>
struct IDirect3DDevice9;
struct IDirect3DTexture9;
struct IDirect3DResource9;
struct IUnknown;
typedef int NvAPI_Status;
typedef uint32_t NvU32;
typedef struct NVDX_ObjectHandle__ *NVDX_ObjectHandle;
typedef void *StereoHandle;
typedef char NvAPI_ShortString[64];
#define NVAPI_OK 0
#define NVAPI_ERROR (-1)
#define NVAPI_NO_IMPLEMENTATION (-3)
typedef struct { NvU32 version; NvU32 maxNumAFRGroups; NvU32 numAFRGroups; NvU32 currentAFRIndex; NvU32 nextFrameAFRIndex; NvU32 previousFrameAFRIndex; NvU32 bIsCurAFRGroupNew; } NV_GET_CURRENT_SLI_STATE;
typedef struct { NvU32 version; NvU32 drvVersion; NvU32 bldChangeListNum; NvAPI_ShortString szBuildBranchString; NvAPI_ShortString szAdapterString; } NV_DISPLAY_DRIVER_VERSION;
typedef uint32_t NvDisplayHandle;

static inline NvAPI_Status NvAPI_Initialize(void) { return NVAPI_NO_IMPLEMENTATION; }
static inline NvAPI_Status NvAPI_Stereo_IsEnabled(uint8_t *enabled) { if (enabled) *enabled = 0; return NVAPI_ERROR; }
static inline NvAPI_Status NvAPI_Stereo_IsActivated(StereoHandle, uint8_t *active) { if (active) *active = 0; return NVAPI_ERROR; }
static inline NvAPI_Status NvAPI_Stereo_CreateHandleFromIUnknown(void *, StereoHandle *h) { if (h) *h = nullptr; return NVAPI_ERROR; }
static inline NvAPI_Status NvAPI_Stereo_DestroyHandle(StereoHandle) { return NVAPI_OK; }
static inline NvAPI_Status NvAPI_Stereo_SetConvergence(StereoHandle, float) { return NVAPI_ERROR; }
static inline NvAPI_Status NvAPI_GetDisplayDriverVersion(NvDisplayHandle, NV_DISPLAY_DRIVER_VERSION *) { return NVAPI_ERROR; }
static inline NvAPI_Status NvAPI_D3D_GetCurrentSLIState(void *, NV_GET_CURRENT_SLI_STATE *) { return NVAPI_ERROR; }
static inline NvAPI_Status NvAPI_D3D9_GetTextureHandle(void *, NVDX_ObjectHandle *h) { if (h) *h = nullptr; return NVAPI_ERROR; }
static inline NvAPI_Status NvAPI_D3D9_GetCurrentZBufferHandle(void *, NVDX_ObjectHandle *h) { if (h) *h = nullptr; return NVAPI_ERROR; }
static inline NvAPI_Status NvAPI_D3D9_StretchRect(void *, NVDX_ObjectHandle, const void *, NVDX_ObjectHandle, const void *, int) { return NVAPI_ERROR; }

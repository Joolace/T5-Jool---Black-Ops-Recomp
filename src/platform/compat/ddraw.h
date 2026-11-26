#pragma once
// ddraw.h: DirectDraw serviva solo a stimare la memoria video; LoadLibrary("ddraw.dll") fallisce e il motore
// usa il valore di ripiego.
#include "bo1_com.h"
typedef struct _DDSCAPS2 { DWORD dwCaps, dwCaps2, dwCaps3, dwCaps4; } DDSCAPS2, *LPDDSCAPS2;
#define DDSCAPS_VIDEOMEMORY 0x00004000
#define DDSCAPS_LOCALVIDMEM 0x10000000
#define DDSCAPS_TEXTURE 0x00001000
#define DDSCL_NORMAL 0x00000008
struct IDirectDraw7 : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetAvailableVidMem(LPDDSCAPS2 caps, LPDWORD total, LPDWORD free) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND wnd, DWORD flags) = 0;
};
static const GUID IID_IDirectDraw7 = { 0x15e65ec0, 0x3b9c, 0x11d2, { 0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b } };

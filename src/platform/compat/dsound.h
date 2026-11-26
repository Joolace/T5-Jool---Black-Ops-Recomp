#pragma once
// dsound.h (Linux/Android): DirectSound serve solo alla chat vocale e all'audio dei filmati Bink.
// Offline non servono: DirectSoundCreate8/DirectSoundCaptureCreate falliscono e il motore li disattiva.
#include "bo1_com.h"
#include <platform/xaudio2/XAudio2.h> // WAVEFORMATEX

#define DS_OK S_OK
#define DSERR_INVALIDPARAM E_INVALIDARG
#define DSERR_INVALIDCALL ((HRESULT)0x88780032)
#define DSERR_PRIOLEVELNEEDED ((HRESULT)0x88780046)
#define DSERR_BUFFERLOST ((HRESULT)0x88780096)
#define DSERR_NODRIVER ((HRESULT)0x88780078)
#define DSSCL_NORMAL 1
#define DSSCL_PRIORITY 2
#define DSBPLAY_LOOPING 1
#define DSBCAPS_GLOBALFOCUS 0x8000
#define DSBCAPS_CTRLVOLUME 0x80
#define DSBCAPS_CTRLFREQUENCY 0x20
#define DSBCAPS_CTRLPAN 0x40
#define DSBCAPS_GETCURRENTPOSITION2 0x10000
#define DSBCAPS_LOCSOFTWARE 0x8
#define DSCBSTART_LOOPING 1
#define DSBVOLUME_MIN (-10000)
#define DSBVOLUME_MAX 0
#define DSBLOCK_FROMWRITECURSOR 1
#define DSBLOCK_ENTIREBUFFER 2

typedef struct _DSBUFFERDESC { DWORD dwSize, dwFlags, dwBufferBytes, dwReserved; LPWAVEFORMATEX lpwfxFormat; GUID guid3DAlgorithm; } DSBUFFERDESC, *LPDSBUFFERDESC;
typedef const DSBUFFERDESC *LPCDSBUFFERDESC;
typedef struct _DSCEFFECTDESC DSCEFFECTDESC;
typedef struct _DSCBUFFERDESC { DWORD dwSize, dwFlags, dwBufferBytes, dwReserved; LPWAVEFORMATEX lpwfxFormat; DWORD dwFXCount; DSCEFFECTDESC *lpDSCFXDesc; } DSCBUFFERDESC, *LPDSCBUFFERDESC;
typedef const DSCBUFFERDESC *LPCDSCBUFFERDESC;

struct IDirectSoundBuffer : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetCurrentPosition(LPDWORD play, LPDWORD write) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD pos) = 0;
    virtual HRESULT STDMETHODCALLTYPE Lock(DWORD off, DWORD bytes, LPVOID *p1, LPDWORD n1, LPVOID *p2, LPDWORD n2, DWORD flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE Unlock(LPVOID p1, DWORD n1, LPVOID p2, DWORD n2) = 0;
    virtual HRESULT STDMETHODCALLTYPE Play(DWORD reserved, DWORD priority, DWORD flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVolume(LONG v) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPan(LONG p) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFrequency(DWORD f) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStatus(LPDWORD s) = 0;
    virtual HRESULT STDMETHODCALLTYPE Restore() = 0;
};
typedef IDirectSoundBuffer *LPDIRECTSOUNDBUFFER;
struct IDirectSoundCaptureBuffer : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetCurrentPosition(LPDWORD capture, LPDWORD read) = 0;
    virtual HRESULT STDMETHODCALLTYPE Lock(DWORD off, DWORD bytes, LPVOID *p1, LPDWORD n1, LPVOID *p2, LPDWORD n2, DWORD flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE Unlock(LPVOID p1, DWORD n1, LPVOID p2, DWORD n2) = 0;
    virtual HRESULT STDMETHODCALLTYPE Start(DWORD flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
};
typedef IDirectSoundCaptureBuffer *LPDIRECTSOUNDCAPTUREBUFFER;
struct IDirectSound8 : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE CreateSoundBuffer(LPCDSBUFFERDESC desc, LPDIRECTSOUNDBUFFER *buf, IUnknown *outer) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND wnd, DWORD level) = 0;
};
typedef IDirectSound8 IDirectSound;
typedef IDirectSound8 *LPDIRECTSOUND8;
typedef IDirectSound8 *LPDIRECTSOUND;
struct IDirectSoundCapture : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE CreateCaptureBuffer(LPCDSCBUFFERDESC desc, LPDIRECTSOUNDCAPTUREBUFFER *buf, IUnknown *outer) = 0;
};
typedef IDirectSoundCapture *LPDIRECTSOUNDCAPTURE;
typedef IDirectSoundCapture IDirectSoundCapture8;
typedef IDirectSoundCapture *LPDIRECTSOUNDCAPTURE8;

static inline HRESULT DirectSoundCreate8(const GUID *, LPDIRECTSOUND8 *out, IUnknown *) { if (out) *out = nullptr; return DSERR_NODRIVER; }
static inline HRESULT DirectSoundCaptureCreate(const GUID *, LPDIRECTSOUNDCAPTURE *out, IUnknown *) { if (out) *out = nullptr; return DSERR_NODRIVER; }
#define DirectSoundCaptureCreate8 DirectSoundCaptureCreate

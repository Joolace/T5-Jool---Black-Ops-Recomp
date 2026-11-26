// bink.h (Linux/Android) - sottoinsieme dell'API Bink 1.9 usato dal motore.
// La libreria Bink originale esiste solo per Windows x86. Qui la sostituisce bink_shim.cpp:
// per ora BinkOpen fallisce in modo pulito (il motore salta il filmato), in seguito si può
// decodificare il .bik con FFmpeg (libavcodec ha il decoder "binkvideo"/"binkaudio").
#pragma once

#include <stdint.h>

typedef uint32_t U32;
typedef int32_t S32;
typedef uint16_t U16;
typedef int16_t S16;
typedef uint8_t U8;
typedef int8_t S8;
typedef float F32;
typedef uintptr_t UINTa;
#define PTR4
#define RADLINK
#define RADEXPLINK
#define __RADNT__ 1 // stesso ramo di Windows desktop: texture D3D9 (fornite da DXVK)
#define RADEXPFUNC extern "C"
#define RADDEFFUNC extern "C"
#define RADDEFSTART
#define RADDEFEND
#define RADLINKCALL

typedef struct BINKRECT { S32 Left, Top, Width, Height; } BINKRECT;
#define BINKMAXDIRTYRECTS 8

typedef struct BINKPLANE { S32 Allocate; void *Buffer; U32 BufferPitch; } BINKPLANE;
typedef struct BINKFRAMEPLANESET { BINKPLANE YPlane, cRPlane, cBPlane, APlane; } BINKFRAMEPLANESET;
#define BINKMAXFRAMEBUFFERS 2
typedef struct BINKFRAMEBUFFERS {
    S32 TotalFrames;
    U32 YABufferWidth, YABufferHeight, cRcBBufferWidth, cRcBBufferHeight;
    U32 FrameNum;
    BINKFRAMEPLANESET Frames[BINKMAXFRAMEBUFFERS];
} BINKFRAMEBUFFERS;

// Solo i campi pubblici della struttura originale (il motore non tocca quelli interni).
typedef struct BINK {
    U32 Width, Height, Frames, FrameNum, LastFrameNum;
    U32 FrameRate, FrameRateDiv;
    U32 ReadError, OpenFlags, BinkType;
    U32 Size, FrameSize, SndSize, FrameChangePercent;
    BINKRECT FrameRects[BINKMAXDIRTYRECTS];
    S32 NumRects;
    BINKFRAMEBUFFERS *FrameBuffers;
    S32 Paused;
    void *impl; // stato del decoder (bink_shim.cpp)
} BINK, *HBINK;

typedef struct BINKREALTIME {
    U32 FrameNum, FrameRate, FrameRateDiv, Frames, FramesTime, FramesVideoDecompTime, FramesAudioDecompTime;
    U32 FramesReadTime, FramesIdleReadTime, FramesThreadReadTime, FramesBlitTime, ReadBufferSize, ReadBufferUsed, FramesDataRate;
} BINKREALTIME;

typedef void *BINKSNDOPEN;
typedef BINKSNDOPEN (*BINKSNDSYSOPEN)(UINTa param);
typedef void *(*BINKMEMALLOC)(U32 bytes);
typedef void (*BINKMEMFREE)(void *ptr);

#define BINKSNDTRACK 0x00004000L
#define BINKNOFRAMEBUFFERS 0x00000400L
#define BINKNOSKIP 0x00080000L
#define BINKALPHA 0x00100000L
#define BINKIOSIZE 0x01000000L
#define BINKFROMMEMORY 0x04000000L
#define BINKSURFACEFAST 0x00000000L
#define BINKBGIOSUSPEND 1
#define BINKBGIORESUME 2
#define BINKBGIOWAIT 0x80000000

RADEXPFUNC HBINK BinkOpen(const char *name, U32 flags);
RADEXPFUNC void BinkClose(HBINK bnk);
RADEXPFUNC char *BinkGetError(void);
RADEXPFUNC S32 BinkDoFrame(HBINK bnk);
RADEXPFUNC void BinkNextFrame(HBINK bnk);
RADEXPFUNC S32 BinkWait(HBINK bnk);
RADEXPFUNC S32 BinkPause(HBINK bnk, S32 pause);
RADEXPFUNC S32 BinkShouldSkip(HBINK bink);
RADEXPFUNC S32 BinkGetRects(HBINK bnk, U32 flags);
RADEXPFUNC void BinkSetVolume(HBINK bnk, U32 trackid, S32 volume);
RADEXPFUNC void BinkSetSoundTrack(U32 total_tracks, U32 *tracks);
RADEXPFUNC S32 BinkSetSoundSystem(BINKSNDSYSOPEN open, UINTa param);
RADEXPFUNC BINKSNDOPEN BinkOpenDirectSound(UINTa param);
RADEXPFUNC void BinkSetMemory(BINKMEMALLOC a, BINKMEMFREE f);
RADEXPFUNC void BinkSetIOSize(U32 iosize);
RADEXPFUNC void BinkRegisterFrameBuffers(HBINK bink, BINKFRAMEBUFFERS *fbset);
RADEXPFUNC void BinkGetFrameBuffersInfo(HBINK bink, BINKFRAMEBUFFERS *fbset);
RADEXPFUNC void BinkGetRealtime(HBINK bink, BINKREALTIME *run, U32 frames);
RADEXPFUNC S32 BinkControlBackgroundIO(HBINK bink, U32 control);

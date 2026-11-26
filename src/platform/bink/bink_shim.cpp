// bink_shim.cpp - Bink sostituito: nessun video per ora. BinkOpen restituisce NULL e il motore
// (R_Cinematic_BinkOpen) tratta il filmato come mancante e prosegue.
#include "bink.h"
#include <string.h>

// Errore sempre vuoto: il motore controlla BinkGetError() con un assert prima di ogni apertura
// (R_Cinematic_CheckBinkError) e gestisce da solo l'apertura fallita ("not playing movie").
static char g_binkError[128] = "";

extern "C" HBINK BinkOpen(const char *, U32) { return nullptr; }
extern "C" void BinkClose(HBINK) {}
extern "C" char *BinkGetError(void) { return g_binkError; }
extern "C" S32 BinkDoFrame(HBINK) { return 0; }
extern "C" void BinkNextFrame(HBINK) {}
extern "C" S32 BinkWait(HBINK) { return 0; }
extern "C" S32 BinkPause(HBINK b, S32 pause) { if (b) b->Paused = pause; return pause; }
extern "C" S32 BinkShouldSkip(HBINK) { return 0; }
extern "C" S32 BinkGetRects(HBINK, U32) { return 0; }
extern "C" void BinkSetVolume(HBINK, U32, S32) {}
extern "C" void BinkSetSoundTrack(U32, U32 *) {}
extern "C" S32 BinkSetSoundSystem(BINKSNDSYSOPEN, UINTa) { return 1; }
extern "C" BINKSNDOPEN BinkOpenDirectSound(UINTa) { return nullptr; }
extern "C" void BinkSetMemory(BINKMEMALLOC, BINKMEMFREE) {}
extern "C" void BinkSetIOSize(U32) {}
extern "C" void BinkRegisterFrameBuffers(HBINK b, BINKFRAMEBUFFERS *fb) { if (b) b->FrameBuffers = fb; }
extern "C" void BinkGetFrameBuffersInfo(HBINK, BINKFRAMEBUFFERS *fb) { if (fb) memset(fb, 0, sizeof(*fb)); }
extern "C" void BinkGetRealtime(HBINK, BINKREALTIME *rt, U32) { if (rt) memset(rt, 0, sizeof(*rt)); }
extern "C" S32 BinkControlBackgroundIO(HBINK, U32) { return 0; }

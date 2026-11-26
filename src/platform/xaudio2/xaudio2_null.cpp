// xaudio2_null.cpp - XAudio2 "spento": usato quando la build non ha FAudio (opzione BO1_WITH_FAUDIO=OFF).
// XAudio2Create fallisce, il motore trova zero dispositivi audio e prosegue senza suono.
#if !defined(_WIN32)
#include "XAudio2.h"

HRESULT XAudio2Create(IXAudio2 **out, UINT32, XAUDIO2_PROCESSOR)
{
    if (out) *out = nullptr;
    return XAUDIO2_E_DEVICE_INVALIDATED;
}
#endif

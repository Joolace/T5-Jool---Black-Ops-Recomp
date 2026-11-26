// XAPO.h - interfacce XAPO (effetti audio) di XAudio2 2.7, per Linux/Android.
#pragma once

#include <platform/compat/bo1_com.h>
#include "XAudio2.h"

#define XAPO_MIN_CHANNELS 1
#define XAPO_MAX_CHANNELS 64
#define XAPO_MIN_FRAMERATE 1000
#define XAPO_MAX_FRAMERATE 200000
#define XAPO_REGISTRATION_STRING_LENGTH 256

#define XAPO_FLAG_CHANNELS_MUST_MATCH 0x00000001
#define XAPO_FLAG_FRAMERATE_MUST_MATCH 0x00000002
#define XAPO_FLAG_BITSPERSAMPLE_MUST_MATCH 0x00000004
#define XAPO_FLAG_BUFFERCOUNT_MUST_MATCH 0x00000008
#define XAPO_FLAG_INPLACE_REQUIRED 0x00000020
#define XAPO_FLAG_INPLACE_SUPPORTED 0x00000010

typedef struct XAPO_REGISTRATION_PROPERTIES {
    GUID clsid;
    WCHAR FriendlyName[XAPO_REGISTRATION_STRING_LENGTH];
    WCHAR CopyrightInfo[XAPO_REGISTRATION_STRING_LENGTH];
    UINT32 MajorVersion;
    UINT32 MinorVersion;
    UINT32 Flags;
    UINT32 MinInputBufferCount;
    UINT32 MaxInputBufferCount;
    UINT32 MinOutputBufferCount;
    UINT32 MaxOutputBufferCount;
} XAPO_REGISTRATION_PROPERTIES;

typedef struct XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS {
    const WAVEFORMATEX *pFormat;
    UINT32 MaxFrameCount;
} XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS;

typedef enum XAPO_BUFFER_FLAGS { XAPO_BUFFER_SILENT, XAPO_BUFFER_VALID } XAPO_BUFFER_FLAGS;

typedef struct XAPO_PROCESS_BUFFER_PARAMETERS {
    void *pBuffer;
    XAPO_BUFFER_FLAGS BufferFlags;
    UINT32 ValidFrameCount;
} XAPO_PROCESS_BUFFER_PARAMETERS;

struct IXAPO : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetRegistrationProperties(XAPO_REGISTRATION_PROPERTIES **ppRegistrationProperties) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsInputFormatSupported(const WAVEFORMATEX *pOutputFormat, const WAVEFORMATEX *pRequestedInputFormat, WAVEFORMATEX **ppSupportedInputFormat) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsOutputFormatSupported(const WAVEFORMATEX *pInputFormat, const WAVEFORMATEX *pRequestedOutputFormat, WAVEFORMATEX **ppSupportedOutputFormat) = 0;
    virtual HRESULT STDMETHODCALLTYPE Initialize(const void *pData, UINT32 DataByteSize) = 0;
    virtual void STDMETHODCALLTYPE Reset() = 0;
    virtual HRESULT STDMETHODCALLTYPE LockForProcess(UINT32 InputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pInputLockedParameters,
        UINT32 OutputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pOutputLockedParameters) = 0;
    virtual void STDMETHODCALLTYPE UnlockForProcess() = 0;
    virtual void STDMETHODCALLTYPE Process(UINT32 InputProcessParameterCount, const XAPO_PROCESS_BUFFER_PARAMETERS *pInputProcessParameters,
        UINT32 OutputProcessParameterCount, XAPO_PROCESS_BUFFER_PARAMETERS *pOutputProcessParameters, BOOL IsEnabled) = 0;
    virtual UINT32 STDMETHODCALLTYPE CalcInputFrames(UINT32 OutputFrameCount) = 0;
    virtual UINT32 STDMETHODCALLTYPE CalcOutputFrames(UINT32 InputFrameCount) = 0;
};

struct IXAPOParameters : public IUnknown
{
    virtual void STDMETHODCALLTYPE SetParameters(const void *pParameters, UINT32 ParameterByteSize) = 0;
    virtual void STDMETHODCALLTYPE GetParameters(void *pParameters, UINT32 ParameterByteSize) = 0;
};

BO1_DEFINE_UUID(IXAPO, 0xA90BC001, 0xE897, 0xE897, 0x55, 0xE4, 0x9E, 0x47, 0x00, 0x00, 0x00, 0x00);
BO1_DEFINE_UUID(IXAPOParameters, 0xA90BC001, 0xE897, 0xE897, 0x55, 0xE4, 0x9E, 0x47, 0x00, 0x00, 0x00, 0x01);

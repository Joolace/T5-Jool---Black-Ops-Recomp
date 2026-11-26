// XAudio2.h - interfacce XAudio2 2.7 (DirectX SDK giugno 2010) per Linux/Android.
// Solo dichiarazioni: l'implementazione (xaudio2_faudio.cpp) inoltra tutto a FAudio,
// che riproduce XAudio2 2.7 in C e gira sopra SDL (anche su Android).
#pragma once

#include <platform/compat/bo1_com.h>

#pragma pack(push, 1)

#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_ADPCM 2
#define WAVE_FORMAT_IEEE_FLOAT 3
#define WAVE_FORMAT_XMA2 0x166
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

typedef struct tWAVEFORMATEX {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *LPWAVEFORMATEX;

typedef struct {
    WAVEFORMATEX Format;
    union { WORD wValidBitsPerSample; WORD wSamplesPerBlock; WORD wReserved; } Samples;
    DWORD dwChannelMask;
    GUID SubFormat;
} WAVEFORMATEXTENSIBLE;

typedef struct adpcmcoef_tag { short iCoef1; short iCoef2; } ADPCMCOEFSET;
typedef struct adpcmwaveformat_tag {
    WAVEFORMATEX wfx;
    WORD wSamplesPerBlock;
    WORD wNumCoef;
    ADPCMCOEFSET aCoef[1];
} ADPCMWAVEFORMAT;

#pragma pack(pop)

#define XAUDIO2_MAX_BUFFER_BYTES 0x80000000
#define XAUDIO2_MAX_QUEUED_BUFFERS 64
#define XAUDIO2_MAX_FREQ_RATIO 1024.0f
#define XAUDIO2_DEFAULT_FREQ_RATIO 2.0f
#define XAUDIO2_COMMIT_NOW 0
#define XAUDIO2_COMMIT_ALL 0
#define XAUDIO2_NO_LOOP_REGION 0
#define XAUDIO2_LOOP_INFINITE 255
#define XAUDIO2_DEFAULT_CHANNELS 0
#define XAUDIO2_DEFAULT_SAMPLERATE 0
#define XAUDIO2_DEBUG_ENGINE 0x0001
#define XAUDIO2_VOICE_NOPITCH 0x0002
#define XAUDIO2_VOICE_NOSRC 0x0004
#define XAUDIO2_VOICE_USEFILTER 0x0008
#define XAUDIO2_VOICE_MUSIC 0x0010
#define XAUDIO2_PLAY_TAILS 0x0020
#define XAUDIO2_END_OF_STREAM 0x0040
#define XAUDIO2_SEND_USEFILTER 0x0080
#define XAUDIO2_VOICE_NOSAMPLESPLAYED 0x0100

typedef uint32_t XAUDIO2_PROCESSOR;
#define XAUDIO2_ANY_PROCESSOR 0xffffffff
#define XAUDIO2_DEFAULT_PROCESSOR XAUDIO2_ANY_PROCESSOR
#define XAUDIO2_WINDOWS_PROCESSOR_SPECIFIER 0xffffffff

typedef enum XAUDIO2_DEVICE_ROLE {
    NotDefaultDevice = 0x0, DefaultConsoleDevice = 0x1, DefaultMultimediaDevice = 0x2,
    DefaultCommunicationsDevice = 0x4, DefaultGameDevice = 0x8, GlobalDefaultDevice = 0xf,
    InvalidDeviceRole = ~GlobalDefaultDevice
} XAUDIO2_DEVICE_ROLE;

typedef struct XAUDIO2_DEVICE_DETAILS {
    WCHAR DeviceID[256];
    WCHAR DisplayName[256];
    XAUDIO2_DEVICE_ROLE Role;
    WAVEFORMATEXTENSIBLE OutputFormat;
} XAUDIO2_DEVICE_DETAILS;

typedef struct XAUDIO2_VOICE_DETAILS { UINT32 CreationFlags; UINT32 InputChannels; UINT32 InputSampleRate; } XAUDIO2_VOICE_DETAILS;

struct IXAudio2Voice;
typedef struct XAUDIO2_SEND_DESCRIPTOR { UINT32 Flags; IXAudio2Voice *pOutputVoice; } XAUDIO2_SEND_DESCRIPTOR;
typedef struct XAUDIO2_VOICE_SENDS { UINT32 SendCount; XAUDIO2_SEND_DESCRIPTOR *pSends; } XAUDIO2_VOICE_SENDS;
typedef struct XAUDIO2_EFFECT_DESCRIPTOR { IUnknown *pEffect; BOOL InitialState; UINT32 OutputChannels; } XAUDIO2_EFFECT_DESCRIPTOR;
typedef struct XAUDIO2_EFFECT_CHAIN { UINT32 EffectCount; XAUDIO2_EFFECT_DESCRIPTOR *pEffectDescriptors; } XAUDIO2_EFFECT_CHAIN;

typedef enum XAUDIO2_FILTER_TYPE { LowPassFilter, BandPassFilter, HighPassFilter, NotchFilter } XAUDIO2_FILTER_TYPE;
typedef struct XAUDIO2_FILTER_PARAMETERS { XAUDIO2_FILTER_TYPE Type; float Frequency; float OneOverQ; } XAUDIO2_FILTER_PARAMETERS;

typedef struct XAUDIO2_BUFFER {
    UINT32 Flags;
    UINT32 AudioBytes;
    const BYTE *pAudioData;
    UINT32 PlayBegin;
    UINT32 PlayLength;
    UINT32 LoopBegin;
    UINT32 LoopLength;
    UINT32 LoopCount;
    void *pContext;
} XAUDIO2_BUFFER;

typedef struct XAUDIO2_BUFFER_WMA { const UINT32 *pDecodedPacketCumulativeBytes; UINT32 PacketCount; } XAUDIO2_BUFFER_WMA;
typedef struct XAUDIO2_VOICE_STATE { void *pCurrentBufferContext; UINT32 BuffersQueued; UINT64 SamplesPlayed; } XAUDIO2_VOICE_STATE;

typedef struct XAUDIO2_PERFORMANCE_DATA {
    UINT64 AudioCyclesSinceLastQuery, TotalCyclesSinceLastQuery;
    UINT32 MinimumCyclesPerQuantum, MaximumCyclesPerQuantum, MemoryUsageInBytes, CurrentLatencyInSamples, GlitchesSinceEngineStarted;
    UINT32 ActiveSourceVoiceCount, TotalSourceVoiceCount, ActiveSubmixVoiceCount, ActiveResamplerCount, ActiveMatrixMixCount;
    UINT32 ActiveXmaSourceVoices, ActiveXmaStreams;
} XAUDIO2_PERFORMANCE_DATA;

typedef struct XAUDIO2_DEBUG_CONFIGURATION {
    UINT32 TraceMask, BreakMask; BOOL LogThreadID, LogFileline, LogFunctionName, LogTiming;
} XAUDIO2_DEBUG_CONFIGURATION;

struct IXAudio2EngineCallback
{
    virtual void STDMETHODCALLTYPE OnProcessingPassStart() = 0;
    virtual void STDMETHODCALLTYPE OnProcessingPassEnd() = 0;
    virtual void STDMETHODCALLTYPE OnCriticalError(HRESULT Error) = 0;
};

struct IXAudio2VoiceCallback
{
    virtual void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 BytesRequired) = 0;
    virtual void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() = 0;
    virtual void STDMETHODCALLTYPE OnStreamEnd() = 0;
    virtual void STDMETHODCALLTYPE OnBufferStart(void *pBufferContext) = 0;
    virtual void STDMETHODCALLTYPE OnBufferEnd(void *pBufferContext) = 0;
    virtual void STDMETHODCALLTYPE OnLoopEnd(void *pBufferContext) = 0;
    virtual void STDMETHODCALLTYPE OnVoiceError(void *pBufferContext, HRESULT Error) = 0;
};

struct IXAudio2Voice
{
    virtual void STDMETHODCALLTYPE GetVoiceDetails(XAUDIO2_VOICE_DETAILS *pVoiceDetails) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOutputVoices(const XAUDIO2_VOICE_SENDS *pSendList) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEffectChain(const XAUDIO2_EFFECT_CHAIN *pEffectChain) = 0;
    virtual HRESULT STDMETHODCALLTYPE EnableEffect(UINT32 EffectIndex, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual HRESULT STDMETHODCALLTYPE DisableEffect(UINT32 EffectIndex, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetEffectState(UINT32 EffectIndex, BOOL *pEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEffectParameters(UINT32 EffectIndex, const void *pParameters, UINT32 ParametersByteSize, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetEffectParameters(UINT32 EffectIndex, void *pParameters, UINT32 ParametersByteSize) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *pParameters) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, const XAUDIO2_FILTER_PARAMETERS *pParameters, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetOutputFilterParameters(IXAudio2Voice *pDestinationVoice, XAUDIO2_FILTER_PARAMETERS *pParameters) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVolume(float Volume, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetVolume(float *pVolume) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetChannelVolumes(UINT32 Channels, const float *pVolumes, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetChannelVolumes(UINT32 Channels, float *pVolumes) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float *pLevelMatrix, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetOutputMatrix(IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, float *pLevelMatrix) = 0;
    virtual void STDMETHODCALLTYPE DestroyVoice() = 0;
};

struct IXAudio2SourceVoice : public IXAudio2Voice
{
    virtual HRESULT STDMETHODCALLTYPE Start(UINT32 Flags = 0, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop(UINT32 Flags = 0, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual HRESULT STDMETHODCALLTYPE SubmitSourceBuffer(const XAUDIO2_BUFFER *pBuffer, const XAUDIO2_BUFFER_WMA *pBufferWMA = NULL) = 0;
    virtual HRESULT STDMETHODCALLTYPE FlushSourceBuffers() = 0;
    virtual HRESULT STDMETHODCALLTYPE Discontinuity() = 0;
    virtual HRESULT STDMETHODCALLTYPE ExitLoop(UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetState(XAUDIO2_VOICE_STATE *pVoiceState) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFrequencyRatio(float Ratio, UINT32 OperationSet = XAUDIO2_COMMIT_NOW) = 0;
    virtual void STDMETHODCALLTYPE GetFrequencyRatio(float *pRatio) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetSourceSampleRate(UINT32 NewSourceSampleRate) = 0;
};

struct IXAudio2SubmixVoice : public IXAudio2Voice {};
struct IXAudio2MasteringVoice : public IXAudio2Voice {};

struct IXAudio2 : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetDeviceCount(UINT32 *pCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceDetails(UINT32 Index, XAUDIO2_DEVICE_DETAILS *pDeviceDetails) = 0;
    virtual HRESULT STDMETHODCALLTYPE Initialize(UINT32 Flags = 0, XAUDIO2_PROCESSOR XAudio2Processor = XAUDIO2_DEFAULT_PROCESSOR) = 0;
    virtual HRESULT STDMETHODCALLTYPE RegisterForCallbacks(IXAudio2EngineCallback *pCallback) = 0;
    virtual void STDMETHODCALLTYPE UnregisterForCallbacks(IXAudio2EngineCallback *pCallback) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSourceVoice(IXAudio2SourceVoice **ppSourceVoice, const WAVEFORMATEX *pSourceFormat, UINT32 Flags = 0,
        float MaxFrequencyRatio = XAUDIO2_DEFAULT_FREQ_RATIO, IXAudio2VoiceCallback *pCallback = NULL,
        const XAUDIO2_VOICE_SENDS *pSendList = NULL, const XAUDIO2_EFFECT_CHAIN *pEffectChain = NULL) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateSubmixVoice(IXAudio2SubmixVoice **ppSubmixVoice, UINT32 InputChannels, UINT32 InputSampleRate,
        UINT32 Flags = 0, UINT32 ProcessingStage = 0, const XAUDIO2_VOICE_SENDS *pSendList = NULL, const XAUDIO2_EFFECT_CHAIN *pEffectChain = NULL) = 0;
    virtual HRESULT STDMETHODCALLTYPE CreateMasteringVoice(IXAudio2MasteringVoice **ppMasteringVoice, UINT32 InputChannels = XAUDIO2_DEFAULT_CHANNELS,
        UINT32 InputSampleRate = XAUDIO2_DEFAULT_SAMPLERATE, UINT32 Flags = 0, UINT32 DeviceIndex = 0, const XAUDIO2_EFFECT_CHAIN *pEffectChain = NULL) = 0;
    virtual HRESULT STDMETHODCALLTYPE StartEngine() = 0;
    virtual void STDMETHODCALLTYPE StopEngine() = 0;
    virtual HRESULT STDMETHODCALLTYPE CommitChanges(UINT32 OperationSet) = 0;
    virtual void STDMETHODCALLTYPE GetPerformanceData(XAUDIO2_PERFORMANCE_DATA *pPerfData) = 0;
    virtual void STDMETHODCALLTYPE SetDebugConfiguration(const XAUDIO2_DEBUG_CONFIGURATION *pDebugConfiguration, void *pReserved = NULL) = 0;
};

HRESULT XAudio2Create(IXAudio2 **ppXAudio2, UINT32 Flags = 0, XAUDIO2_PROCESSOR XAudio2Processor = XAUDIO2_DEFAULT_PROCESSOR);

#define XAUDIO2_E_INVALID_CALL ((HRESULT)0x88960001)
#define XAUDIO2_E_XMA_DECODER_ERROR ((HRESULT)0x88960002)
#define XAUDIO2_E_XAPO_CREATION_FAILED ((HRESULT)0x88960003)
#define XAUDIO2_E_DEVICE_INVALIDATED ((HRESULT)0x88960004)

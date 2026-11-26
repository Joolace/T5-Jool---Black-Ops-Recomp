// xaudio2_sdl.cpp - XAudio2 2.7 ridotto all'osso, sopra l'audio di SDL2 (Linux e Android).
//
// Il motore usa XAudio2 in modo semplice e prevedibile (snd_driver_xaudio2.cpp):
//   - voci sorgente PCM (8/16/24/32 bit, float) e MS-ADPCM, con frequency ratio (pitch) e looping;
//   - tre bus (submix) a 48 kHz: radverb (4 canali, stage 1) -> novoice (stage 2) -> master bus (stage 3);
//   - una mastering voice; matrici d'uscita impostate a mano su ogni send;
//   - effetti XAPO propri (SDXA2Effect) su ogni voce e ogni bus, che vogliono blocchi da 480 frame float.
// Qui c'è esattamente questo: un grafo di voci elaborato a quanti di 10 ms (480 frame a 48 kHz) in ordine
// sorgenti -> submix per stage -> master, come fa XAudio2. L'xWMA (suoni caricati: spari, coltello, zombie)
// viene decodificato in PCM 16 bit con FFmpeg (xwma_ffmpeg.cpp) al momento del SubmitSourceBuffer.
//
// Le callback delle voci (OnBufferEnd ecc.) vengono chiamate dal thread audio FUORI dal lock del mixer:
// il motore dentro OnBufferEnd prende i suoi lock dello streaming e il thread di gioco può star chiamando
// noi tenendoli, quindi chiamarle sotto il nostro lock rischierebbe un deadlock.
#if !defined(_WIN32)

#include "XAudio2.h"
#include "XAPO.h"
#include "xwma_decode.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#ifndef BO1_XAUDIO2_NO_SDL
#include <SDL.h>
#endif

// le macro min/max di windows.h (bo1_compat.h) non servono qui e romperebbero std::min/std::max
#undef min
#undef max

// T5Jool: suono aggiunto al mix del motore (musica del menu su Android, dove AAudio apre un solo dispositivo).
// Chiamata dal thread audio con il buffer float a 48 kHz e "channels" canali interlacciati.
extern "C" void (*volatile g_bo1ExtraMix)(float *buf, unsigned frames, unsigned channels) = nullptr;

namespace
{
constexpr UINT32 kMixRate = 48000;
constexpr UINT32 kQuantum = 480; // 10 ms: gli effetti del motore (SDXA2Effect) lo pretendono esattamente

struct Engine;
struct VoiceCore;

// ------------------------------------------------------------------------------------------------
// Decodifica dei formati sorgente
// ------------------------------------------------------------------------------------------------
struct SourceFormat
{
    WORD tag = 0;       // WAVE_FORMAT_PCM / ADPCM / IEEE_FLOAT / altro (non supportato -> silenzio)
    WORD channels = 1;
    UINT32 rate = 44100;
    WORD bits = 16;
    WORD blockAlign = 0;      // ADPCM
    WORD samplesPerBlock = 0; // ADPCM
    std::vector<ADPCMCOEFSET> coefs;
    // xWMA: la voce suona PCM 16 bit decodificato; questi sono i parametri del formato originale
    bool wma = false;
    UINT32 wmaBlockAlign = 0;
    UINT32 wmaAvgBytes = 0;

    bool Supported() const
    {
        if (tag == WAVE_FORMAT_PCM) return bits == 8 || bits == 16 || bits == 24 || bits == 32;
        if (tag == WAVE_FORMAT_IEEE_FLOAT) return bits == 32;
        if (tag == WAVE_FORMAT_ADPCM) return blockAlign > 7 * channels && samplesPerBlock >= 2 && !coefs.empty();
        return false;
    }

    UINT32 FrameCount(UINT32 bytes) const
    {
        if (tag == WAVE_FORMAT_ADPCM)
        {
            UINT32 blocks = bytes / blockAlign;
            UINT32 rest = bytes % blockAlign;
            UINT32 frames = blocks * samplesPerBlock;
            if (rest >= 7u * channels) frames += (rest - 7u * channels) * 2u / channels + 2u;
            return frames;
        }
        // Il motore crea le voci PCM con nBlockAlign = 0 (bug del codice originale, tollerato da XAudio2):
        // la dimensione di un frame la calcoliamo noi.
        UINT32 frameBytes = channels * (bits / 8u);
        return frameBytes ? bytes / frameBytes : 0;
    }
};

const int kAdaptation[16] = { 230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230 };

// Decodifica un blocco MS-ADPCM in campioni interleaved a 16 bit. Restituisce i frame prodotti.
UINT32 DecodeAdpcmBlock(const SourceFormat &fmt, const BYTE *src, UINT32 bytes, std::vector<int16_t> &out)
{
    const int ch = fmt.channels;
    if (bytes < 7u * ch) return 0;
    UINT32 frames = std::min<UINT32>(fmt.samplesPerBlock, (bytes - 7u * ch) * 2u / ch + 2u);
    out.assign(static_cast<size_t>(frames) * ch, 0);

    int pred[8], delta[8], s1[8], s2[8];
    const BYTE *p = src;
    for (int c = 0; c < ch; ++c) pred[c] = std::min<int>(*p++, static_cast<int>(fmt.coefs.size()) - 1);
    for (int c = 0; c < ch; ++c) { delta[c] = static_cast<int16_t>(p[0] | (p[1] << 8)); p += 2; }
    for (int c = 0; c < ch; ++c) { s1[c] = static_cast<int16_t>(p[0] | (p[1] << 8)); p += 2; }
    for (int c = 0; c < ch; ++c) { s2[c] = static_cast<int16_t>(p[0] | (p[1] << 8)); p += 2; }
    for (int c = 0; c < ch; ++c)
    {
        out[c] = static_cast<int16_t>(s2[c]);
        if (frames > 1) out[ch + c] = static_cast<int16_t>(s1[c]);
    }

    const BYTE *end = src + bytes;
    UINT32 sample = 2u * ch; // indice nel buffer interleaved
    bool high = true;
    while (sample < frames * static_cast<UINT32>(ch) && p < end)
    {
        int c = static_cast<int>(sample % ch);
        int nib = high ? (*p >> 4) : (*p & 0x0F);
        if (!high) ++p;
        high = !high;

        int signedNib = nib >= 8 ? nib - 16 : nib;
        const ADPCMCOEFSET &co = fmt.coefs[pred[c]];
        int predicted = (s1[c] * co.iCoef1 + s2[c] * co.iCoef2) / 256;
        int v = predicted + signedNib * delta[c];
        v = std::max(-32768, std::min(32767, v));
        s2[c] = s1[c];
        s1[c] = v;
        delta[c] = std::max(16, (kAdaptation[nib] * delta[c]) / 256);
        out[sample++] = static_cast<int16_t>(v);
    }
    return frames;
}

// ------------------------------------------------------------------------------------------------
// Voci
// ------------------------------------------------------------------------------------------------
struct Send
{
    VoiceCore *dest;
    std::vector<float> matrix; // [destCh * srcCh], livello di src S in dest D = matrix[D * srcCh + S]
};

struct EffectSlot
{
    IXAPO *xapo = nullptr;
    IXAPOParameters *params = nullptr;
    BOOL enabled = TRUE;
    UINT32 channels = 0;
};

enum class Kind { Source, Submix, Master };

struct VoiceCore
{
    Engine *engine = nullptr;
    Kind kind = Kind::Source;
    IXAudio2Voice *iface = nullptr;
    UINT32 channels = 0;   // canali in ingresso (= canali dopo SRC per le sorgenti)
    UINT32 stage = 0;      // solo submix
    UINT32 flags = 0;
    float volume = 1.0f;
    std::vector<Send> sends;
    std::vector<EffectSlot> effects;
    std::vector<float> accum; // submix/master: ingresso del quanto corrente [kQuantum * channels]
};

struct QueuedBuffer
{
    XAUDIO2_BUFFER desc;
    UINT32 frames = 0;      // frame totali del buffer (dopo PlayBegin/PlayLength)
    UINT32 begin = 0;       // primo frame da suonare
    UINT32 loopsLeft = 0;
    bool started = false;
    std::shared_ptr<const std::vector<int16_t>> pcm; // xWMA decodificato (tiene vivi i dati di desc)
};

struct SourceState
{
    SourceFormat fmt;
    IXAudio2VoiceCallback *callback = nullptr;
    float maxRatio = 2.0f;
    float ratio = 1.0f;
    bool running = false;
    std::deque<QueuedBuffer> queue;
    double pos = 0.0;        // posizione (frame) dentro il buffer in testa
    double samplesPlayed = 0.0;
    // cache ADPCM: blocco decodificato
    const BYTE *cacheData = nullptr;
    UINT32 cacheBlock = 0xFFFFFFFFu;
    std::vector<int16_t> cacheSamples;
};

// ------------------------------------------------------------------------------------------------
// Motore
// ------------------------------------------------------------------------------------------------
struct Engine final : public IXAudio2
{
    std::atomic<ULONG> refs{ 1 };
    std::recursive_mutex lock;
    std::vector<VoiceCore *> sources;
    std::vector<VoiceCore *> submixes; // ordinati per stage
    VoiceCore *master = nullptr;
    std::map<IXAudio2Voice *, VoiceCore *> lookup;
    std::map<VoiceCore *, SourceState *> sourceState;
    bool started = true;
    UINT32 deviceChannels = 2;
    // uscita del quanto in eccesso rispetto a quanto chiesto da SDL
    std::vector<float> pending;
    size_t pendingPos = 0;
    std::vector<std::function<void()>> deferred; // callback da chiamare fuori dal lock
#ifndef BO1_XAUDIO2_NO_SDL
    SDL_AudioDeviceID device = 0;
#endif

    Engine();
    ~Engine();

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **out) override
    {
        if (!out) return E_POINTER;
        *out = this;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ULONG n = --refs;
        if (!n) delete this;
        return n;
    }

    // IXAudio2
    HRESULT STDMETHODCALLTYPE GetDeviceCount(UINT32 *count) override { *count = 1; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDeviceDetails(UINT32 index, XAUDIO2_DEVICE_DETAILS *d) override;
    HRESULT STDMETHODCALLTYPE Initialize(UINT32, XAUDIO2_PROCESSOR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE RegisterForCallbacks(IXAudio2EngineCallback *) override { return S_OK; }
    void STDMETHODCALLTYPE UnregisterForCallbacks(IXAudio2EngineCallback *) override {}
    HRESULT STDMETHODCALLTYPE CreateSourceVoice(IXAudio2SourceVoice **voice, const WAVEFORMATEX *fmt, UINT32 flags, float maxRatio,
                                                IXAudio2VoiceCallback *cb, const XAUDIO2_VOICE_SENDS *sends, const XAUDIO2_EFFECT_CHAIN *fx) override;
    HRESULT STDMETHODCALLTYPE CreateSubmixVoice(IXAudio2SubmixVoice **voice, UINT32 channels, UINT32 rate, UINT32 flags, UINT32 stage,
                                                const XAUDIO2_VOICE_SENDS *sends, const XAUDIO2_EFFECT_CHAIN *fx) override;
    HRESULT STDMETHODCALLTYPE CreateMasteringVoice(IXAudio2MasteringVoice **voice, UINT32 channels, UINT32 rate, UINT32 flags, UINT32 device,
                                                   const XAUDIO2_EFFECT_CHAIN *fx) override;
    HRESULT STDMETHODCALLTYPE StartEngine() override;
    void STDMETHODCALLTYPE StopEngine() override;
    HRESULT STDMETHODCALLTYPE CommitChanges(UINT32) override { return S_OK; }
    void STDMETHODCALLTYPE GetPerformanceData(XAUDIO2_PERFORMANCE_DATA *p) override { memset(p, 0, sizeof(*p)); }
    void STDMETHODCALLTYPE SetDebugConfiguration(const XAUDIO2_DEBUG_CONFIGURATION *, void *) override {}

    // mixer
    VoiceCore *Find(IXAudio2Voice *v)
    {
        auto it = lookup.find(v);
        return it == lookup.end() ? nullptr : it->second;
    }
    HRESULT SetupSends(VoiceCore *v, const XAUDIO2_VOICE_SENDS *sends);
    HRESULT SetupEffects(VoiceCore *v, const XAUDIO2_EFFECT_CHAIN *fx);
    void ReleaseEffects(VoiceCore *v);
    void Destroy(VoiceCore *v);
    void RenderQuantum(float *out);
    void RenderSource(VoiceCore *v, SourceState *s, float *dst);
    void RunEffects(VoiceCore *v, float *buf);
    void MixToSends(VoiceCore *v, const float *buf);
    bool FetchFrame(SourceState *s, size_t queueIndex, int64_t frame, float *out);
    void AdvanceSource(VoiceCore *v, SourceState *s, double step);
    void Render(float *out, UINT32 frames);
    void FlushDeferred();
    bool OpenDevice();
    void CloseDevice();
};

// ------------------------------------------------------------------------------------------------
// Interfacce delle voci: inoltrano tutto al motore
// ------------------------------------------------------------------------------------------------
template <class Iface>
struct VoiceImpl : public Iface
{
    VoiceCore core;

    void STDMETHODCALLTYPE GetVoiceDetails(XAUDIO2_VOICE_DETAILS *d) override
    {
        d->CreationFlags = core.flags;
        d->InputChannels = core.channels;
        d->InputSampleRate = kMixRate;
        if (core.kind == Kind::Source)
        {
            std::lock_guard<std::recursive_mutex> l(core.engine->lock);
            d->InputSampleRate = core.engine->sourceState[&core]->fmt.rate;
            d->InputChannels = core.engine->sourceState[&core]->fmt.channels;
        }
    }
    HRESULT STDMETHODCALLTYPE SetOutputVoices(const XAUDIO2_VOICE_SENDS *sends) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        return core.engine->SetupSends(&core, sends);
    }
    HRESULT STDMETHODCALLTYPE SetEffectChain(const XAUDIO2_EFFECT_CHAIN *fx) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        core.engine->ReleaseEffects(&core);
        return core.engine->SetupEffects(&core, fx);
    }
    HRESULT STDMETHODCALLTYPE EnableEffect(UINT32 i, UINT32) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        if (i >= core.effects.size()) return E_INVALIDARG;
        core.effects[i].enabled = TRUE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DisableEffect(UINT32 i, UINT32) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        if (i >= core.effects.size()) return E_INVALIDARG;
        core.effects[i].enabled = FALSE;
        return S_OK;
    }
    void STDMETHODCALLTYPE GetEffectState(UINT32 i, BOOL *enabled) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        *enabled = i < core.effects.size() ? core.effects[i].enabled : FALSE;
    }
    HRESULT STDMETHODCALLTYPE SetEffectParameters(UINT32 i, const void *params, UINT32 size, UINT32) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        if (i >= core.effects.size() || !core.effects[i].params) return E_INVALIDARG;
        core.effects[i].params->SetParameters(params, size);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetEffectParameters(UINT32 i, void *params, UINT32 size) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        if (i >= core.effects.size() || !core.effects[i].params) return E_INVALIDARG;
        core.effects[i].params->GetParameters(params, size);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS *, UINT32) override { return S_OK; }
    void STDMETHODCALLTYPE GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *p) override { p->Type = LowPassFilter; p->Frequency = 1.0f; p->OneOverQ = 1.0f; }
    HRESULT STDMETHODCALLTYPE SetOutputFilterParameters(IXAudio2Voice *, const XAUDIO2_FILTER_PARAMETERS *, UINT32) override { return S_OK; }
    void STDMETHODCALLTYPE GetOutputFilterParameters(IXAudio2Voice *, XAUDIO2_FILTER_PARAMETERS *p) override { p->Type = LowPassFilter; p->Frequency = 1.0f; p->OneOverQ = 1.0f; }
    HRESULT STDMETHODCALLTYPE SetVolume(float v, UINT32) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        core.volume = v;
        return S_OK;
    }
    void STDMETHODCALLTYPE GetVolume(float *v) override { *v = core.volume; }
    HRESULT STDMETHODCALLTYPE SetChannelVolumes(UINT32, const float *, UINT32) override { return S_OK; }
    void STDMETHODCALLTYPE GetChannelVolumes(UINT32 n, float *v) override { for (UINT32 i = 0; i < n; ++i) v[i] = 1.0f; }

    HRESULT STDMETHODCALLTYPE SetOutputMatrix(IXAudio2Voice *dest, UINT32 srcCh, UINT32 dstCh, const float *m, UINT32) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        Send *send = nullptr;
        if (!dest && core.sends.size() == 1) send = &core.sends[0];
        for (Send &s : core.sends)
            if (s.dest && s.dest->iface == dest) send = &s;
        if (!send) return XAUDIO2_E_INVALID_CALL;
        UINT32 inCh = OutputChannels();
        if (srcCh != inCh || dstCh != send->dest->channels) return XAUDIO2_E_INVALID_CALL;
        send->matrix.assign(m, m + static_cast<size_t>(srcCh) * dstCh);
        return S_OK;
    }
    void STDMETHODCALLTYPE GetOutputMatrix(IXAudio2Voice *dest, UINT32 srcCh, UINT32 dstCh, float *m) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        for (Send &s : core.sends)
            if (s.dest && s.dest->iface == dest && s.matrix.size() == static_cast<size_t>(srcCh) * dstCh)
                std::copy(s.matrix.begin(), s.matrix.end(), m);
    }
    void STDMETHODCALLTYPE DestroyVoice() override { core.engine->Destroy(&core); }

    // Canali che escono dalla voce (dopo gli effetti)
    UINT32 OutputChannels() const
    {
        return core.effects.empty() ? core.channels : core.effects.back().channels;
    }
};

struct SourceVoice final : public VoiceImpl<IXAudio2SourceVoice>
{
    SourceState state;

    HRESULT STDMETHODCALLTYPE Start(UINT32, UINT32) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        state.running = true;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Stop(UINT32, UINT32) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        state.running = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SubmitSourceBuffer(const XAUDIO2_BUFFER *buf, const XAUDIO2_BUFFER_WMA *wmaBuf) override
    {
        if (!buf) return E_POINTER;
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        if (state.queue.size() >= XAUDIO2_MAX_QUEUED_BUFFERS) return XAUDIO2_E_INVALID_CALL;
        QueuedBuffer q;
        q.desc = *buf;
        if (state.fmt.wma)
        {
            q.pcm = bo1::DecodeXwma(buf->pAudioData, buf->AudioBytes, state.fmt.channels, state.fmt.rate,
                                    state.fmt.wmaBlockAlign, state.fmt.wmaAvgBytes,
                                    wmaBuf ? reinterpret_cast<const uint32_t *>(wmaBuf->pDecodedPacketCumulativeBytes) : nullptr,
                                    wmaBuf ? wmaBuf->PacketCount : 0);
            q.desc.pAudioData = q.pcm ? reinterpret_cast<const BYTE *>(q.pcm->data()) : nullptr;
            q.desc.AudioBytes = q.pcm ? static_cast<UINT32>(q.pcm->size() * sizeof(int16_t)) : 0;
        }
        // q.desc, non *buf: per l'xWMA i dati sono stati sostituiti dal PCM decodificato
        UINT32 total = state.fmt.Supported() && q.desc.pAudioData ? state.fmt.FrameCount(q.desc.AudioBytes) : 0;
        q.begin = std::min(buf->PlayBegin, total);
        q.frames = buf->PlayLength ? std::min(total, q.begin + buf->PlayLength) : total;
        q.loopsLeft = buf->LoopCount;
        state.queue.push_back(q);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE FlushSourceBuffers() override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        // come XAudio2: tolti tutti i buffer tranne quello in riproduzione, con OnBufferEnd per ciascuno
        while (state.queue.size() > (state.running ? 1u : 0u))
        {
            void *ctx = state.queue.back().desc.pContext;
            state.queue.pop_back();
            if (state.callback)
            {
                IXAudio2VoiceCallback *cb = state.callback;
                core.engine->deferred.push_back([cb, ctx] { cb->OnBufferEnd(ctx); });
            }
        }
        if (!state.running) state.pos = 0.0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Discontinuity() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ExitLoop(UINT32) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        if (!state.queue.empty()) state.queue.front().loopsLeft = 0;
        return S_OK;
    }
    void STDMETHODCALLTYPE GetState(XAUDIO2_VOICE_STATE *s) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        s->pCurrentBufferContext = state.queue.empty() ? nullptr : state.queue.front().desc.pContext;
        s->BuffersQueued = static_cast<UINT32>(state.queue.size());
        s->SamplesPlayed = static_cast<UINT64>(state.samplesPlayed);
    }
    HRESULT STDMETHODCALLTYPE SetFrequencyRatio(float r, UINT32) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        state.ratio = std::max(1.0f / 1024.0f, std::min(r, state.maxRatio));
        return S_OK;
    }
    void STDMETHODCALLTYPE GetFrequencyRatio(float *r) override { *r = state.ratio; }
    HRESULT STDMETHODCALLTYPE SetSourceSampleRate(UINT32 rate) override
    {
        std::lock_guard<std::recursive_mutex> l(core.engine->lock);
        if (!rate) return E_INVALIDARG;
        state.fmt.rate = rate;
        return S_OK;
    }
};

struct SubmixVoice final : public VoiceImpl<IXAudio2SubmixVoice> {};
struct MasterVoice final : public VoiceImpl<IXAudio2MasteringVoice> {};

// ------------------------------------------------------------------------------------------------
// Implementazione del motore
// ------------------------------------------------------------------------------------------------
Engine::Engine()
{
#ifndef BO1_XAUDIO2_NO_SDL
    if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (const char *env = getenv("BO1_AUDIO_CHANNELS"))
    {
        int n = atoi(env);
        if (n == 2 || n == 4 || n == 6 || n == 8) deviceChannels = static_cast<UINT32>(n);
    }
#endif
}

Engine::~Engine()
{
    CloseDevice();
    std::lock_guard<std::recursive_mutex> l(lock);
    std::vector<VoiceCore *> all(sources);
    all.insert(all.end(), submixes.begin(), submixes.end());
    if (master) all.push_back(master);
    for (VoiceCore *v : all) Destroy(v);
}

HRESULT Engine::GetDeviceDetails(UINT32 index, XAUDIO2_DEVICE_DETAILS *d)
{
    if (index != 0 || !d) return E_INVALIDARG;
    memset(d, 0, sizeof(*d));
    // Il motore estrae il GUID del dispositivo dal DeviceID (SD_XAudio2GetDeviceGUID)
    swprintf(d->DeviceID, 256, L"{%08X-0000-0000-0000-000000000001}", 0x53444C32u /* "SDL2" */);
    swprintf(d->DisplayName, 256, L"SDL");
    d->Role = GlobalDefaultDevice;
    WAVEFORMATEX &f = d->OutputFormat.Format;
    f.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    f.nChannels = static_cast<WORD>(deviceChannels);
    f.nSamplesPerSec = kMixRate;
    f.wBitsPerSample = 32;
    f.nBlockAlign = static_cast<WORD>(deviceChannels * 4);
    f.nAvgBytesPerSec = kMixRate * f.nBlockAlign;
    f.cbSize = 22;
    d->OutputFormat.Samples.wValidBitsPerSample = 32;
    return S_OK;
}

HRESULT Engine::SetupSends(VoiceCore *v, const XAUDIO2_VOICE_SENDS *sends)
{
    v->sends.clear();
    UINT32 outCh = v->effects.empty() ? v->channels : v->effects.back().channels;
    if (!sends)
    {
        // XAudio2: senza send esplicite la voce va alla mastering voice
        if (v->kind != Kind::Master && master) v->sends.push_back({ master, {} });
    }
    else
    {
        for (UINT32 i = 0; i < sends->SendCount; ++i)
        {
            VoiceCore *dest = Find(sends->pSends[i].pOutputVoice);
            if (!dest) return XAUDIO2_E_INVALID_CALL;
            v->sends.push_back({ dest, {} });
        }
    }
    // Matrice predefinita: identità se i canali coincidono, altrimenti mono -> tutti / N -> primi N
    for (Send &s : v->sends)
    {
        UINT32 d = s.dest->channels;
        s.matrix.assign(static_cast<size_t>(outCh) * d, 0.0f);
        for (UINT32 dc = 0; dc < d; ++dc)
        {
            if (outCh == 1) s.matrix[dc] = dc < 2 ? 1.0f : 0.0f;
            else if (dc < outCh) s.matrix[dc * outCh + dc] = 1.0f;
        }
    }
    return S_OK;
}

HRESULT Engine::SetupEffects(VoiceCore *v, const XAUDIO2_EFFECT_CHAIN *fx)
{
    v->effects.clear();
    if (!fx) return S_OK;
    UINT32 ch = v->channels;
    for (UINT32 i = 0; i < fx->EffectCount; ++i)
    {
        const XAUDIO2_EFFECT_DESCRIPTOR &d = fx->pEffectDescriptors[i];
        EffectSlot slot;
        slot.enabled = d.InitialState;
        slot.channels = d.OutputChannels ? d.OutputChannels : ch;
        if (!d.pEffect || d.pEffect->QueryInterface(__uuidof(IXAPO), reinterpret_cast<void **>(&slot.xapo)) != S_OK || !slot.xapo)
            return XAUDIO2_E_XAPO_CREATION_FAILED;
        if (d.pEffect->QueryInterface(__uuidof(IXAPOParameters), reinterpret_cast<void **>(&slot.params)) != S_OK)
            slot.params = nullptr;

        // Formato float 32 bit WAVE_FORMAT_EXTENSIBLE a 48 kHz, come lo passa XAudio2 (SDXA2Effect lo verifica)
        WAVEFORMATEXTENSIBLE inF{}, outF{};
        for (WAVEFORMATEXTENSIBLE *f : { &inF, &outF })
        {
            f->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
            f->Format.nSamplesPerSec = kMixRate;
            f->Format.wBitsPerSample = 32;
            f->Format.cbSize = 22;
            f->Samples.wValidBitsPerSample = 32;
        }
        inF.Format.nChannels = static_cast<WORD>(ch);
        outF.Format.nChannels = static_cast<WORD>(slot.channels);
        inF.Format.nBlockAlign = static_cast<WORD>(ch * 4);
        outF.Format.nBlockAlign = static_cast<WORD>(slot.channels * 4);
        XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS lin{ &inF.Format, kQuantum }, lout{ &outF.Format, kQuantum };
        slot.xapo->LockForProcess(1, &lin, 1, &lout);
        v->effects.push_back(slot);
        ch = slot.channels;
    }
    return S_OK;
}

void Engine::ReleaseEffects(VoiceCore *v)
{
    for (EffectSlot &e : v->effects)
    {
        if (e.xapo)
        {
            e.xapo->UnlockForProcess();
            e.xapo->Release();
        }
        if (e.params) e.params->Release();
    }
    v->effects.clear();
}

void Engine::Destroy(VoiceCore *v)
{
    std::lock_guard<std::recursive_mutex> l(lock);
    if (!v || !lookup.count(v->iface)) return;
    // le voci che mandano verso questa perdono la send
    for (auto &kv : lookup)
    {
        auto &s = kv.second->sends;
        s.erase(std::remove_if(s.begin(), s.end(), [v](const Send &x) { return x.dest == v; }), s.end());
    }
    ReleaseEffects(v);
    lookup.erase(v->iface);
    sources.erase(std::remove(sources.begin(), sources.end(), v), sources.end());
    submixes.erase(std::remove(submixes.begin(), submixes.end(), v), submixes.end());
    if (v->kind == Kind::Source)
    {
        sourceState.erase(v);
        delete static_cast<SourceVoice *>(v->iface);
    }
    else if (v->kind == Kind::Submix)
    {
        delete static_cast<SubmixVoice *>(v->iface);
    }
    else
    {
        master = nullptr;
        CloseDevice();
        delete static_cast<MasterVoice *>(v->iface);
    }
}

HRESULT Engine::CreateSourceVoice(IXAudio2SourceVoice **out, const WAVEFORMATEX *fmt, UINT32 flags, float maxRatio,
                                  IXAudio2VoiceCallback *cb, const XAUDIO2_VOICE_SENDS *sends, const XAUDIO2_EFFECT_CHAIN *fx)
{
    if (!out || !fmt || !fmt->nChannels || fmt->nChannels > 8) return E_INVALIDARG;
    std::lock_guard<std::recursive_mutex> l(lock);
    auto *v = new SourceVoice();
    v->core.engine = this;
    v->core.kind = Kind::Source;
    v->core.iface = v;
    v->core.flags = flags;
    v->core.channels = fmt->nChannels;
    SourceFormat &f = v->state.fmt;
    f.tag = fmt->wFormatTag;
    if (f.tag == WAVE_FORMAT_EXTENSIBLE) // sottoformato PCM o float
    {
        auto *ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(fmt);
        f.tag = static_cast<WORD>(ext->SubFormat.Data1);
    }
    f.channels = fmt->nChannels;
    f.rate = fmt->nSamplesPerSec ? fmt->nSamplesPerSec : kMixRate;
    f.bits = fmt->wBitsPerSample ? fmt->wBitsPerSample : 16;
    if (f.tag == WAVE_FORMAT_ADPCM)
    {
        auto *a = reinterpret_cast<const ADPCMWAVEFORMAT *>(fmt);
        f.blockAlign = fmt->nBlockAlign;
        f.samplesPerBlock = a->wSamplesPerBlock;
        f.coefs.assign(a->aCoef, a->aCoef + a->wNumCoef);
    }
    if (f.tag == 0x0161) // WAVE_FORMAT_WMAUDIO2 (xWMA): decodificato in PCM 16 bit a ogni SubmitSourceBuffer
    {
        f.wma = true;
        f.wmaBlockAlign = fmt->nBlockAlign;
        f.wmaAvgBytes = fmt->nAvgBytesPerSec;
        f.tag = WAVE_FORMAT_PCM;
        f.bits = 16;
    }
    v->state.callback = cb;
    v->state.maxRatio = maxRatio > 0 ? std::min(maxRatio, XAUDIO2_MAX_FREQ_RATIO) : XAUDIO2_DEFAULT_FREQ_RATIO;
    lookup[v] = &v->core;
    sourceState[&v->core] = &v->state;
    if (SetupEffects(&v->core, fx) != S_OK || SetupSends(&v->core, sends) != S_OK)
    {
        Destroy(&v->core);
        *out = nullptr;
        return XAUDIO2_E_INVALID_CALL;
    }
    sources.push_back(&v->core);
    *out = v;
    return S_OK;
}

HRESULT Engine::CreateSubmixVoice(IXAudio2SubmixVoice **out, UINT32 channels, UINT32, UINT32 flags, UINT32 stage,
                                  const XAUDIO2_VOICE_SENDS *sends, const XAUDIO2_EFFECT_CHAIN *fx)
{
    if (!out || !channels || channels > 8) return E_INVALIDARG;
    std::lock_guard<std::recursive_mutex> l(lock);
    auto *v = new SubmixVoice();
    v->core.engine = this;
    v->core.kind = Kind::Submix;
    v->core.iface = v;
    v->core.flags = flags;
    v->core.channels = channels;
    v->core.stage = stage;
    v->core.accum.assign(static_cast<size_t>(kQuantum) * channels, 0.0f);
    lookup[v] = &v->core;
    if (SetupEffects(&v->core, fx) != S_OK || SetupSends(&v->core, sends) != S_OK)
    {
        Destroy(&v->core);
        *out = nullptr;
        return XAUDIO2_E_INVALID_CALL;
    }
    submixes.push_back(&v->core);
    std::stable_sort(submixes.begin(), submixes.end(), [](VoiceCore *a, VoiceCore *b) { return a->stage < b->stage; });
    *out = v;
    return S_OK;
}

HRESULT Engine::CreateMasteringVoice(IXAudio2MasteringVoice **out, UINT32 channels, UINT32, UINT32 flags, UINT32 device, const XAUDIO2_EFFECT_CHAIN *fx)
{
    if (!out) return E_INVALIDARG;
    if (device != 0) return E_INVALIDARG;
    std::lock_guard<std::recursive_mutex> l(lock);
    if (master) return XAUDIO2_E_INVALID_CALL; // una sola mastering voice, come XAudio2
    auto *v = new MasterVoice();
    v->core.engine = this;
    v->core.kind = Kind::Master;
    v->core.iface = v;
    v->core.flags = flags;
    v->core.channels = channels ? std::min<UINT32>(channels, 8) : deviceChannels;
    v->core.accum.assign(static_cast<size_t>(kQuantum) * v->core.channels, 0.0f);
    lookup[v] = &v->core;
    SetupEffects(&v->core, fx);
    master = &v->core;
    if (!OpenDevice())
    {
        Destroy(&v->core);
        *out = nullptr;
        return XAUDIO2_E_DEVICE_INVALIDATED;
    }
    *out = v;
    return S_OK;
}

HRESULT Engine::StartEngine()
{
    {
        std::lock_guard<std::recursive_mutex> l(lock);
        started = true;
    }
#ifndef BO1_XAUDIO2_NO_SDL
    if (device) SDL_PauseAudioDevice(device, 0);
#endif
    return S_OK;
}

void Engine::StopEngine()
{
    {
        std::lock_guard<std::recursive_mutex> l(lock);
        started = false;
    }
#ifndef BO1_XAUDIO2_NO_SDL
    if (device) SDL_PauseAudioDevice(device, 1);
#endif
}

// Legge il frame "frame" (relativo all'inizio del buffer queueIndex) in out[channels], float.
// Oltre la fine del buffer prosegue con il loop o con il buffer successivo, così l'interpolazione è continua.
bool Engine::FetchFrame(SourceState *s, size_t qi, int64_t frame, float *out)
{
    const SourceFormat &f = s->fmt;
    while (qi < s->queue.size())
    {
        const QueuedBuffer &q = s->queue[qi];
        int64_t len = static_cast<int64_t>(q.frames) - q.begin;
        if (len <= 0) { ++qi; continue; }
        if (frame < len) break;
        if (q.loopsLeft > 0)
        {
            UINT32 lb = q.desc.LoopLength ? q.desc.LoopBegin : q.begin;
            UINT32 le = q.desc.LoopLength ? std::min(q.frames, q.desc.LoopBegin + q.desc.LoopLength) : q.frames;
            if (le > lb) { frame = lb - q.begin + (frame - len) % (le - lb); break; }
        }
        frame -= len;
        ++qi;
    }
    if (qi >= s->queue.size()) return false;

    const QueuedBuffer &q = s->queue[qi];
    UINT32 idx = static_cast<UINT32>(q.begin + frame);
    const BYTE *data = q.desc.pAudioData;
    const int ch = f.channels;
    switch (f.tag)
    {
    case WAVE_FORMAT_PCM:
    {
        size_t bps = f.bits / 8u;
        const BYTE *p = data + static_cast<size_t>(idx) * ch * bps;
        for (int c = 0; c < ch; ++c, p += bps)
        {
            switch (f.bits)
            {
            case 8: out[c] = (static_cast<int>(*p) - 128) / 128.0f; break;
            case 16: out[c] = static_cast<int16_t>(p[0] | (p[1] << 8)) / 32768.0f; break;
            case 24: out[c] = static_cast<int32_t>((p[0] << 8) | (p[1] << 16) | (static_cast<uint32_t>(p[2]) << 24)) / 2147483648.0f; break;
            default: out[c] = static_cast<int32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24)) / 2147483648.0f; break;
            }
        }
        return true;
    }
    case WAVE_FORMAT_IEEE_FLOAT:
        memcpy(out, data + static_cast<size_t>(idx) * ch * 4, static_cast<size_t>(ch) * 4);
        return true;
    case WAVE_FORMAT_ADPCM:
    {
        UINT32 block = idx / f.samplesPerBlock;
        if (s->cacheData != data || s->cacheBlock != block)
        {
            UINT32 offset = block * f.blockAlign;
            UINT32 avail = std::min<UINT32>(f.blockAlign, q.desc.AudioBytes > offset ? q.desc.AudioBytes - offset : 0);
            DecodeAdpcmBlock(f, data + offset, avail, s->cacheSamples);
            s->cacheData = data;
            s->cacheBlock = block;
        }
        size_t within = static_cast<size_t>(idx % f.samplesPerBlock) * ch;
        for (int c = 0; c < ch; ++c)
            out[c] = within + c < s->cacheSamples.size() ? s->cacheSamples[within + c] / 32768.0f : 0.0f;
        return true;
    }
    default:
        return false;
    }
}

// Avanza di "step" frame sorgente: fine buffer, loop e callback.
void Engine::AdvanceSource(VoiceCore *, SourceState *s, double step)
{
    s->pos += step;
    s->samplesPlayed += step;
    while (!s->queue.empty())
    {
        QueuedBuffer &q = s->queue.front();
        double len = static_cast<double>(q.frames) - q.begin;
        if (s->pos < len && len > 0) break;
        IXAudio2VoiceCallback *cb = s->callback;
        void *ctx = q.desc.pContext;
        if (q.loopsLeft > 0 && len > 0)
        {
            UINT32 lb = q.desc.LoopLength ? q.desc.LoopBegin : q.begin;
            UINT32 le = q.desc.LoopLength ? std::min(q.frames, q.desc.LoopBegin + q.desc.LoopLength) : q.frames;
            if (le <= lb) { q.loopsLeft = 0; continue; }
            s->pos = (lb - q.begin) + std::fmod(s->pos - len, static_cast<double>(le - lb));
            if (q.loopsLeft != XAUDIO2_LOOP_INFINITE) --q.loopsLeft;
            if (cb) deferred.push_back([cb, ctx] { cb->OnLoopEnd(ctx); });
            continue;
        }
        bool eos = (q.desc.Flags & XAUDIO2_END_OF_STREAM) != 0;
        s->pos -= std::max(len, 0.0);
        s->queue.pop_front();
        if (cb)
        {
            deferred.push_back([cb, ctx] { cb->OnBufferEnd(ctx); });
            if (eos) deferred.push_back([cb] { cb->OnStreamEnd(); });
        }
        if (s->queue.empty()) s->pos = 0.0;
    }
}

void Engine::RenderSource(VoiceCore *v, SourceState *s, float *dst)
{
    const int ch = s->fmt.channels;
    const double step = static_cast<double>(s->fmt.rate) * s->ratio / kMixRate;
    float a[8], b[8];
    if (s->callback)
    {
        IXAudio2VoiceCallback *cb = s->callback;
        UINT32 bytes = 0; // il motore non lo usa (streaming a finestre)
        deferred.push_back([cb, bytes] { cb->OnVoiceProcessingPassStart(bytes); });
    }
    for (UINT32 i = 0; i < kQuantum; ++i)
    {
        float *o = dst + static_cast<size_t>(i) * ch;
        if (s->queue.empty() || !s->fmt.Supported())
        {
            for (int c = 0; c < ch; ++c) o[c] = 0.0f;
            if (!s->queue.empty()) // formato non supportato (xWMA): il buffer "suona" in silenzio e finisce
                AdvanceSource(v, s, step);
            continue;
        }
        QueuedBuffer &q = s->queue.front();
        if (!q.started)
        {
            q.started = true;
            if (s->callback)
            {
                IXAudio2VoiceCallback *cb = s->callback;
                void *ctx = q.desc.pContext;
                deferred.push_back([cb, ctx] { cb->OnBufferStart(ctx); });
            }
        }
        int64_t i0 = static_cast<int64_t>(std::floor(s->pos));
        float frac = static_cast<float>(s->pos - static_cast<double>(i0));
        if (!FetchFrame(s, 0, i0, a)) for (int c = 0; c < ch; ++c) a[c] = 0.0f;
        if (frac > 0.0f && FetchFrame(s, 0, i0 + 1, b))
            for (int c = 0; c < ch; ++c) o[c] = a[c] + (b[c] - a[c]) * frac;
        else
            for (int c = 0; c < ch; ++c) o[c] = a[c];
        AdvanceSource(v, s, step);
    }
}

void Engine::RunEffects(VoiceCore *v, float *buf)
{
    for (EffectSlot &e : v->effects)
    {
        XAPO_PROCESS_BUFFER_PARAMETERS in{ buf, XAPO_BUFFER_VALID, kQuantum };
        XAPO_PROCESS_BUFFER_PARAMETERS out{ buf, XAPO_BUFFER_VALID, kQuantum };
        e.xapo->Process(1, &in, 1, &out, e.enabled);
    }
}

void Engine::MixToSends(VoiceCore *v, const float *buf)
{
    UINT32 inCh = v->effects.empty() ? v->channels : v->effects.back().channels;
    for (Send &s : v->sends)
    {
        if (!s.dest) continue;
        UINT32 outCh = s.dest->channels;
        float *acc = s.dest->accum.data();
        for (UINT32 d = 0; d < outCh; ++d)
        {
            for (UINT32 c = 0; c < inCh; ++c)
            {
                float g = s.matrix[d * inCh + c] * v->volume;
                if (g == 0.0f) continue;
                for (UINT32 i = 0; i < kQuantum; ++i) acc[i * outCh + d] += buf[i * inCh + c] * g;
            }
        }
    }
}

void Engine::RenderQuantum(float *out)
{
    const UINT32 outCh = master ? master->channels : deviceChannels;
    if (!master || !started)
    {
        memset(out, 0, sizeof(float) * kQuantum * outCh);
        return;
    }
    for (VoiceCore *m : submixes) std::fill(m->accum.begin(), m->accum.end(), 0.0f);
    std::fill(master->accum.begin(), master->accum.end(), 0.0f);

    std::vector<float> tmp;
    for (VoiceCore *v : std::vector<VoiceCore *>(sources))
    {
        SourceState *s = sourceState[v];
        if (!s->running) continue;
        tmp.assign(static_cast<size_t>(kQuantum) * std::max<UINT32>(8, v->channels), 0.0f);
        RenderSource(v, s, tmp.data());
        RunEffects(v, tmp.data());
        MixToSends(v, tmp.data());
    }
    for (VoiceCore *m : submixes)
    {
        RunEffects(m, m->accum.data());
        MixToSends(m, m->accum.data());
    }
    RunEffects(master, master->accum.data());
    const float vol = master->volume;
    for (size_t i = 0; i < master->accum.size(); ++i)
    {
        float x = master->accum[i] * vol;
        out[i] = x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
    }
}

void Engine::FlushDeferred()
{
    std::vector<std::function<void()>> calls;
    {
        std::lock_guard<std::recursive_mutex> l(lock);
        calls.swap(deferred);
    }
    for (auto &fn : calls) fn();
}

// Riempie "frames" frame interleaved per i canali della mastering voice.
void Engine::Render(float *out, UINT32 frames)
{
    UINT32 ch;
    {
        std::lock_guard<std::recursive_mutex> l(lock);
        ch = master ? master->channels : deviceChannels;
    }
    size_t need = static_cast<size_t>(frames) * ch;
    size_t done = 0;
    while (done < need)
    {
        if (pendingPos >= pending.size())
        {
            pending.assign(static_cast<size_t>(kQuantum) * ch, 0.0f);
            {
                std::lock_guard<std::recursive_mutex> l(lock);
                RenderQuantum(pending.data());
            }
            pendingPos = 0;
            FlushDeferred();
        }
        size_t n = std::min(need - done, pending.size() - pendingPos);
        memcpy(out + done, pending.data() + pendingPos, n * sizeof(float));
        done += n;
        pendingPos += n;
    }
}

#ifndef BO1_XAUDIO2_NO_SDL
void SDLCALL AudioCallback(void *user, Uint8 *stream, int len)
{
    auto *e = static_cast<Engine *>(user);
    UINT32 ch = e->master ? e->master->channels : e->deviceChannels;
    e->Render(reinterpret_cast<float *>(stream), static_cast<UINT32>(len) / (ch * sizeof(float)));
    if (auto mix = g_bo1ExtraMix)
        mix(reinterpret_cast<float *>(stream), static_cast<unsigned>(len) / (ch * sizeof(float)), ch);
}
#endif

bool Engine::OpenDevice()
{
#ifndef BO1_XAUDIO2_NO_SDL
    if (device) return true;
    SDL_AudioSpec want{}, have{};
    want.freq = kMixRate;
    want.format = AUDIO_F32SYS;
    want.channels = static_cast<Uint8>(master->channels);
    want.samples = 512;
    want.callback = AudioCallback;
    want.userdata = this;
    // Nessun cambiamento permesso: se il dispositivo vuole altro, SDL converte per noi
    device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!device)
    {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(device, started ? 0 : 1);
#endif
    return true;
}

void Engine::CloseDevice()
{
#ifndef BO1_XAUDIO2_NO_SDL
    if (device)
    {
        SDL_AudioDeviceID d = device;
        device = 0;
        SDL_CloseAudioDevice(d); // aspetta la fine della callback in corso
    }
#endif
}
} // namespace

HRESULT XAudio2Create(IXAudio2 **out, UINT32, XAUDIO2_PROCESSOR)
{
    if (!out) return E_POINTER;
    *out = new Engine();
    return S_OK;
}

// Per i test (tests/platform/xaudio2_test.cpp): fa girare il mixer senza dispositivo audio.
extern "C" void bo1_XAudio2RenderForTest(IXAudio2 *engine, float *out, UINT32 frames)
{
    static_cast<Engine *>(engine)->Render(out, frames);
}

#endif // !_WIN32

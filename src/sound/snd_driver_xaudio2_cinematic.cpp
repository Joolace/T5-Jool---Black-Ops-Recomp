// Movie audio for the FFmpeg Bink decoder: an XAudio2 source voice that pulls
// the decoded samples from the movie's queue (binklib/bink_audio.h).
#include "snd_driver_xaudio2.h"

#include <binklib/bink_audio.h>

#include <cstring>
#include <mutex>

namespace
{
constexpr unsigned kFrames = 1024;
constexpr unsigned kBuffers = 3;

class CinematicVoice : public IXAudio2VoiceCallback
{
public:
    bool create()
    {
        WAVEFORMATEX format = {};
        format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        format.nChannels = 2;
        format.nSamplesPerSec = BinkAudioQueue::Rate;
        format.wBitsPerSample = 32;
        format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        XAUDIO2_SEND_DESCRIPTOR send = { 0, g_sd.masterVoice };
        XAUDIO2_VOICE_SENDS sends = { 1, &send };
        if (g_sd.xa2->CreateSourceVoice(&m_voice, &format, 0, XAUDIO2_DEFAULT_FREQ_RATIO, this, &sends, 0) < 0)
        {
            m_voice = 0;
            return false;
        }
        for (unsigned i = 0; i < kBuffers; ++i)
            submit();
        m_voice->Start(0, XAUDIO2_COMMIT_NOW);
        return true;
    }

    void destroy()
    {
        if (!m_voice)
            return;
        m_voice->DestroyVoice();
        m_voice = 0;
    }

    bool attach(BinkAudioQueue *audio)
    {
        if (!m_voice && !create())
            return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_audio)
            return false;
        m_audio = audio;
        return true;
    }

    void detach(BinkAudioQueue *audio)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_audio == audio)
            m_audio = 0;
    }

    void STDMETHODCALLTYPE OnBufferEnd(void *) override { submit(); }
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnBufferStart(void *) override {}
    void STDMETHODCALLTYPE OnLoopEnd(void *) override {}
    void STDMETHODCALLTYPE OnVoiceError(void *, HRESULT) override {}

private:
    void submit()
    {
        float *samples = m_samples[m_next];
        m_next = (m_next + 1) % kBuffers;
        memset(samples, 0, sizeof(m_samples[0]));
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_audio && Sys_BinkHasFocus())
                m_audio->mix(samples, kFrames, (kBuffers - 1) * kFrames, Sys_BinkTimeUs());
        }
        XAUDIO2_BUFFER buffer = {};
        buffer.AudioBytes = sizeof(m_samples[0]);
        buffer.pAudioData = reinterpret_cast<const BYTE *>(samples);
        m_voice->SubmitSourceBuffer(&buffer);
    }

    std::mutex m_mutex;
    BinkAudioQueue *m_audio = 0;
    IXAudio2SourceVoice *m_voice = 0;
    float m_samples[kBuffers][kFrames * 2];
    unsigned m_next = 0;
};

CinematicVoice s_cinematic;
}

bool Snd_AttachCinematicAudio(BinkAudioQueue *audio)
{
    if (!g_sd.xa2 || !g_sd.masterVoice)
        return false;
    return s_cinematic.attach(audio);
}

void Snd_DetachCinematicAudio(BinkAudioQueue *audio)
{
    s_cinematic.detach(audio);
}

void SND_ShutdownCinematicVoice()
{
    s_cinematic.destroy();
}

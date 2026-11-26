#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>

class BinkAudioQueue
{
public:
    static constexpr unsigned Rate = 48000;
    ~BinkAudioQueue();
    bool initialize(size_t frames);
    bool add(const float *samples, size_t frames, uint64_t first, float volume);
    void start();
    void pause(bool paused, uint64_t now);
    void mix(float *output, unsigned frames, unsigned queuedFrames, uint64_t now);
    uint64_t timeUs(uint64_t now);

private:
    uint64_t timeLocked(uint64_t now);
    std::mutex mutex;
    float *buffer = nullptr;
    size_t capacity = 0;
    uint64_t position = 0, mixedAt = 0, pausedTime = 0, lastTime = 0;
    unsigned latency = 0, quantum = 0;
    bool started = false, paused = false, mixed = false;
};

// Implemented by the platform sound driver: mixes the queue into the output.
bool Snd_AttachCinematicAudio(BinkAudioQueue *audio);
void Snd_DetachCinematicAudio(BinkAudioQueue *audio);

// Implemented by the platform: the movie clock in microseconds, which stops
// while the game is in the background.
uint64_t Sys_BinkTimeUs();
void Sys_BinkInitClock();
bool Sys_BinkHasFocus();

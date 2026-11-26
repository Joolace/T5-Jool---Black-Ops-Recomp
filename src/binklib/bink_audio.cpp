#include "bink_audio.h"

#include <algorithm>
#include <cstdlib>

BinkAudioQueue::~BinkAudioQueue()
{
    std::free(buffer);
}

bool BinkAudioQueue::initialize(size_t frames)
{
    if (!frames || frames > Rate * 16)
        return false;
    buffer = static_cast<float *>(std::calloc(frames * 2, sizeof(float)));
    capacity = buffer ? frames : 0;
    return buffer != nullptr;
}

bool BinkAudioQueue::add(const float *samples, size_t frames, uint64_t first, float volume)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (first < position)
    {
        const size_t skip = std::min<uint64_t>(position - first, frames);
        samples += skip * 2;
        frames -= skip;
        first += skip;
    }
    if (!frames)
        return true;
    if (first - position >= capacity || frames > capacity - (first - position))
        return false;
    for (size_t i = 0; i < frames; ++i)
    {
        const size_t index = (first + i) % capacity * 2;
        buffer[index] += samples[i * 2] * volume;
        buffer[index + 1] += samples[i * 2 + 1] * volume;
    }
    return true;
}

void BinkAudioQueue::start()
{
    std::lock_guard<std::mutex> lock(mutex);
    started = true;
}

uint64_t BinkAudioQueue::timeLocked(uint64_t now)
{
    if (paused)
        return pausedTime;
    if (!mixed)
        return 0;
    const uint64_t elapsed = now > mixedAt ? now - mixedAt : 0;
    const uint64_t extrapolated = std::min<uint64_t>(elapsed * Rate / 1000000, quantum);
    const uint64_t end = position + extrapolated;
    lastTime = std::max(lastTime, end > latency ? (end - latency) * 1000000 / Rate : 0);
    return lastTime;
}

uint64_t BinkAudioQueue::timeUs(uint64_t now)
{
    std::lock_guard<std::mutex> lock(mutex);
    return timeLocked(now);
}

void BinkAudioQueue::pause(bool value, uint64_t now)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (value && !paused)
        pausedTime = timeLocked(now);
    paused = value;
}

void BinkAudioQueue::mix(float *output, unsigned frames, unsigned queuedFrames, uint64_t now)
{
    std::lock_guard<std::mutex> lock(mutex);
    if (!started || paused)
        return;
    for (unsigned i = 0; i < frames; ++i)
    {
        const size_t index = position++ % capacity * 2;
        output[i * 2] += buffer[index];
        output[i * 2 + 1] += buffer[index + 1];
        buffer[index] = buffer[index + 1] = 0.0f;
    }
    latency = queuedFrames + frames;
    quantum = frames;
    mixedAt = now;
    mixed = true;
}

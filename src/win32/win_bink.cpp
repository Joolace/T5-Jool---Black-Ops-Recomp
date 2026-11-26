// Windows movie clock for the FFmpeg Bink decoder (binklib/bink_audio.h).
#include <binklib/bink_audio.h>

#include <windows.h>

namespace
{
LARGE_INTEGER s_frequency, s_start;
}

void Sys_BinkInitClock()
{
    if (s_frequency.QuadPart)
        return;
    QueryPerformanceFrequency(&s_frequency);
    QueryPerformanceCounter(&s_start);
}

bool Sys_BinkHasFocus()
{
    return true;
}

uint64_t Sys_BinkTimeUs()
{
    Sys_BinkInitClock();
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const uint64_t ticks = uint64_t(now.QuadPart - s_start.QuadPart);
    const uint64_t frequency = uint64_t(s_frequency.QuadPart);
    return ticks / frequency * 1000000 + ticks % frequency * 1000000 / frequency;
}

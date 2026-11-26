#pragma once

// HOME and sleep suspend the process anywhere, also inside a script run, so
// the VM's 2.5 s infinite-loop watchdog leaves gaps of a second or more
// between two of its checks out of the run's time.
constexpr unsigned int NX_SCRIPT_SUSPEND_GAP_MS = 1000;

// Milliseconds the run has spent since *start. *lastCheck is set with *start
// when the timer restarts.
inline int NX_ScriptRunTime(int *start, unsigned int *lastCheck, unsigned int now)
{
    const unsigned int gap = now - *lastCheck;
    if (gap >= NX_SCRIPT_SUSPEND_GAP_MS)
        *start = (int)((unsigned int)*start + gap);
    *lastCheck = now;
    return (int)(now - (unsigned int)*start);
}

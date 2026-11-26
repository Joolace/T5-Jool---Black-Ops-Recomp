// bo1_feel.cpp - T5Jool: vibrazione e mira giroscopica (vedi platform/bo1_feel.h).
#if !defined(_WIN32)

#include <platform/bo1_feel.h>
#include <universal/dvar.h>
#include <SDL.h>

#include <math.h>
#include <mutex>
#include <stdlib.h>
#include <string.h>

// controller aperto da sdl_xinput.cpp (slot 0), nullptr se non c'e'
SDL_GameController *BO1Pad_Controller0();

namespace
{
std::mutex g_feelLock;

float DvarFloat(const char *name, float def)
{
    const dvar_s *d = Dvar_FindVar(name);
    if (!d)
        return def;
    switch (d->type)
    {
    case DVAR_TYPE_BOOL: return d->current.enabled ? 1.0f : 0.0f;
    case DVAR_TYPE_INT: return (float)d->current.integer;
    case DVAR_TYPE_FLOAT: return d->current.value;
    case DVAR_TYPE_STRING: return d->current.string ? (float)atof(d->current.string) : def;
    default: return def;
    }
}

// ------------------------------------------------------------------------------------------ vibrazione
struct Pulse
{
    float low, high;
    Uint32 end;
};
Pulse g_pulses[8];
float g_continuous;
float g_sentLow = -1.0f, g_sentHigh = -1.0f;
Uint32 g_sentAt;
SDL_Haptic *g_haptic; // Android: il motore di vibrazione del dispositivo (quando il controller non vibra)
bool g_hapticTried;

SDL_Haptic *DeviceHaptic()
{
#ifdef __ANDROID__
    if (!g_hapticTried)
    {
        g_hapticTried = true;
        if (SDL_InitSubSystem(SDL_INIT_HAPTIC) == 0 && SDL_NumHaptics() > 0)
        {
            g_haptic = SDL_HapticOpen(0);
            if (g_haptic && SDL_HapticRumbleInit(g_haptic) != 0)
            {
                SDL_HapticClose(g_haptic);
                g_haptic = nullptr;
            }
        }
        fprintf(stderr, "T5Jool: vibrazione del dispositivo %s\n", g_haptic ? "disponibile" : "non disponibile");
    }
#endif
    return g_haptic;
}

// ------------------------------------------------------------------------------------------ giroscopio
SDL_Sensor *g_gyro, *g_accel;
bool g_sensorsTried;
float g_up[3] = { 0.0f, 1.0f, 0.0f }; // direzione "su" nel sistema del dispositivo (dall'accelerometro, filtrata)
Uint64 g_lastGyroTicks;

void OpenDeviceSensors()
{
    if (g_sensorsTried)
        return;
    g_sensorsTried = true;
    if (SDL_InitSubSystem(SDL_INIT_SENSOR) != 0)
        return;
    for (int i = 0; i < SDL_NumSensors(); ++i)
    {
        const SDL_SensorType t = SDL_SensorGetDeviceType(i);
        if (t == SDL_SENSOR_GYRO && !g_gyro)
            g_gyro = SDL_SensorOpen(i);
        else if (t == SDL_SENSOR_ACCEL && !g_accel)
            g_accel = SDL_SensorOpen(i);
    }
    fprintf(stderr, "T5Jool: giroscopio del dispositivo %s\n", g_gyro ? "trovato" : "assente");
}
} // namespace

extern "C" void BO1Rumble_Pulse(float low, float high, int ms)
{
    std::lock_guard<std::mutex> l(g_feelLock);
    const Uint32 now = SDL_GetTicks();
    int slot = 0;
    for (int i = 0; i < 8; ++i)
    {
        if ((int)(g_pulses[i].end - now) <= 0)
        {
            slot = i;
            break;
        }
        if (g_pulses[i].end < g_pulses[slot].end)
            slot = i;
    }
    g_pulses[slot] = { low, high, now + (Uint32)ms };
}

extern "C" void BO1Rumble_Continuous(float level)
{
    std::lock_guard<std::mutex> l(g_feelLock);
    if (level > g_continuous)
        g_continuous = level;
}

extern "C" void BO1Rumble_Frame(void)
{
    std::lock_guard<std::mutex> l(g_feelLock);
    const Uint32 now = SDL_GetTicks();
    float low = g_continuous, high = g_continuous * 0.6f;
    g_continuous = 0.0f;
    for (const Pulse &p : g_pulses)
        if ((int)(p.end - now) > 0)
        {
            if (p.low > low) low = p.low;
            if (p.high > high) high = p.high;
        }
    const float strength = DvarFloat("bo1_rumble", 1.0f) > 0.0f ? DvarFloat("bo1_rumble_strength", 0.7f) : 0.0f;
    low = fminf(1.0f, low * strength);
    high = fminf(1.0f, high * strength);
    // si manda solo se cambia o ogni 80 ms (la durata di 150 ms copre il frame successivo anche a pochi FPS)
    if (fabsf(low - g_sentLow) < 0.03f && fabsf(high - g_sentHigh) < 0.03f && now - g_sentAt < 80)
        return;
    if (low <= 0.0f && high <= 0.0f && g_sentLow <= 0.0f && g_sentHigh <= 0.0f)
        return;
    g_sentLow = low;
    g_sentHigh = high;
    g_sentAt = now;
    SDL_GameController *c = BO1Pad_Controller0();
    if (c && SDL_GameControllerHasRumble(c))
    {
        SDL_GameControllerRumble(c, (Uint16)(low * 65535.0f), (Uint16)(high * 65535.0f), low > 0.0f || high > 0.0f ? 150 : 0);
        return;
    }
    if (SDL_Haptic *h = DeviceHaptic())
    {
        const float v = fmaxf(low, high);
        if (v > 0.02f)
            SDL_HapticRumblePlay(h, v, 150);
        else
            SDL_HapticRumbleStop(h);
    }
}

extern "C" void BO1Gyro_Apply(float *yawDelta, float *pitchDelta, int aiming)
{
    *yawDelta = *pitchDelta = 0.0f;
    const int mode = (int)DvarFloat("bo1_gyro", 0.0f);
    const Uint64 now = SDL_GetTicks64();
    const float dt = g_lastGyroTicks ? (float)(now - g_lastGyroTicks) / 1000.0f : 0.0f;
    g_lastGyroTicks = now;
    if (mode <= 0 || (mode == 2 && !aiming) || dt <= 0.0f || dt > 0.1f)
        return;
    std::lock_guard<std::mutex> l(g_feelLock);
    float g[3] = {};
    // 1) giroscopio del controller (DualSense, Switch Pro...): assi gia' orientati come il controller
    SDL_GameController *c = BO1Pad_Controller0();
    if (c && SDL_GameControllerHasSensor(c, SDL_SENSOR_GYRO))
    {
        SDL_GameControllerSetSensorEnabled(c, SDL_SENSOR_GYRO, SDL_TRUE);
        if (SDL_GameControllerGetSensorData(c, SDL_SENSOR_GYRO, g, 3) == 0)
        {
            // SDL: x = beccheggio, y = imbardata (rad/s)
            *yawDelta = g[1] * dt * 57.29578f;
            *pitchDelta = -g[0] * dt * 57.29578f;
        }
    }
    else
    {
        // 2) giroscopio del dispositivo (Android): la "imbardata" e' la rotazione attorno alla verticale reale, presa
        // dall'accelerometro, cosi' funziona in qualunque orientamento dello schermo e con la console inclinata.
        OpenDeviceSensors();
        if (!g_gyro)
            return;
        SDL_SensorUpdate(); // dati freschi anche se gli eventi SDL si leggono in un altro punto del frame
        if (SDL_SensorGetData(g_gyro, g, 3) != 0)
        {
            static bool logged;
            if (!logged)
            {
                logged = true;
                fprintf(stderr, "T5Jool: giroscopio, lettura non riuscita: %s\n", SDL_GetError());
            }
            return;
        }
        float a[3];
        if (g_accel && SDL_SensorGetData(g_accel, a, 3) == 0)
        {
            const float len = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
            if (len > 1.0f)
                for (int i = 0; i < 3; ++i)
                    g_up[i] = g_up[i] * 0.9f + (a[i] / len) * 0.1f; // a riposo l'accelerometro segna +g verso l'alto
        }
        float ulen = sqrtf(g_up[0] * g_up[0] + g_up[1] * g_up[1] + g_up[2] * g_up[2]);
        if (ulen < 0.1f)
            return;
        const float up[3] = { g_up[0] / ulen, g_up[1] / ulen, g_up[2] / ulen };
        const float yawRate = g[0] * up[0] + g[1] * up[1] + g[2] * up[2];
        // asse orizzontale dello schermo: perpendicolare a "su" nel piano dello schermo (x, y del dispositivo)
        float px = up[1], py = -up[0];
        const float plen = sqrtf(px * px + py * py);
        if (plen < 0.1f)
            return;
        px /= plen;
        py /= plen;
        const float pitchRate = g[0] * px + g[1] * py;
        *yawDelta = yawRate * dt * 57.29578f;
        *pitchDelta = -pitchRate * dt * 57.29578f;
    }
    // diagnostica (ogni 3 s nel log): dati grezzi e rotazione applicata, per verificare versi e sensibilita'
    {
        static Uint64 lastLog;
        static float sumYaw, sumPitch;
        sumYaw += *yawDelta;
        sumPitch += *pitchDelta;
        if (now - lastLog > 3000)
        {
            fprintf(stderr, "T5Jool: giroscopio %s, grezzo %.3f %.3f %.3f rad/s, su %.2f %.2f %.2f, ruotato %.1f / %.1f gradi in 3 s\n",
                    c && SDL_GameControllerHasSensor(c, SDL_SENSOR_GYRO) ? "del controller" : "del dispositivo", g[0], g[1], g[2],
                    g_up[0], g_up[1], g_up[2], sumYaw, sumPitch);
            lastLog = now;
            sumYaw = sumPitch = 0.0f;
        }
    }
    const float sens = DvarFloat("bo1_gyro_sens", 1.5f);
    *yawDelta *= sens * (DvarFloat("bo1_gyro_invert_x", 0.0f) > 0.0f ? -1.0f : 1.0f);
    *pitchDelta *= sens * (DvarFloat("bo1_gyro_invert_y", 0.0f) > 0.0f ? -1.0f : 1.0f);
}

#endif // !_WIN32

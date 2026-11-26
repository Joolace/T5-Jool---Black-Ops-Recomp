// steam_stub.cpp - steam_api senza Steam (Linux/Android).
// Black Ops su Steam esiste solo per Windows, quindi qui Steam non c'è: SteamAPI_Init fallisce e ogni
// interfaccia (SteamUser(), SteamFriends(), ...) è nulla. Il motore lo gestisce già: senza Steam resta offline
// (vedi LiveSteam_Init in src/live/live_steam.cpp).
#ifndef _WIN32
#include <steam/steam_api.h>
#include <steam/steam_gameserver.h>
#include <stdint.h>
#include <string.h>

extern "C" {

ESteamAPIInitResult SteamInternal_SteamAPI_Init(const char *, SteamErrMsg *err)
{
    if (err) strncpy(*err, "Steam non disponibile su questa piattaforma", sizeof(*err) - 1);
    return k_ESteamAPIInitResult_NoSteamClient;
}
void SteamAPI_Shutdown() {}
bool SteamAPI_RestartAppIfNecessary(uint32) { return false; }
bool SteamAPI_IsSteamRunning() { return false; }
void SteamAPI_RunCallbacks() {}
void SteamGameServer_RunCallbacks() {}
HSteamUser SteamAPI_GetHSteamUser() { return 0; }
HSteamUser SteamGameServer_GetHSteamUser() { return 0; }
HSteamPipe SteamAPI_GetHSteamPipe() { return 0; }
void *SteamInternal_CreateInterface(const char *) { return nullptr; }
void *SteamInternal_FindOrCreateUserInterface(HSteamUser, const char *) { return nullptr; }
void *SteamInternal_FindOrCreateGameServerInterface(HSteamUser, const char *) { return nullptr; }

// Accessori delle interfacce (STEAM_DEFINE_INTERFACE_ACCESSOR): { funzione di init, contatore, puntatore }.
// Alla prima chiamata esegue l'init, che qui mette nullptr; poi restituisce l'indirizzo del puntatore.
void *SteamInternal_ContextInit(void *data)
{
    void **slot = static_cast<void **>(data);
    if (!slot[1])
    {
        slot[1] = reinterpret_cast<void *>(static_cast<uintptr_t>(1));
        reinterpret_cast<void (*)(void *)>(slot[0])(&slot[2]);
    }
    return &slot[2];
}

void SteamAPI_RegisterCallback(class CCallbackBase *, int) {}
void SteamAPI_UnregisterCallback(class CCallbackBase *) {}
void SteamAPI_RegisterCallResult(class CCallbackBase *, SteamAPICall_t) {}
void SteamAPI_UnregisterCallResult(class CCallbackBase *, SteamAPICall_t) {}

} // extern "C"
#endif

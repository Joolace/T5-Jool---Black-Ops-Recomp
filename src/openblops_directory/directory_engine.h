#pragma once
struct netadr_t;
struct UiContext;
struct menuDef_t;
struct itemDef_s;
#if defined(OPENBLOPS_NO_STEAM_AUTH)
void Directory_Init();
// Keeps an old build's saved directory address out of the config being written.
void Directory_BeforeConfigWrite();
void Directory_MenuOpening(int localClientNum, UiContext *dc, menuDef_t *menu);
bool Directory_HandleMenuEvent(int localClientNum, UiContext *dc, itemDef_s *item, const char *event);
bool Directory_HandleControllerKey(int localClientNum, UiContext *dc, menuDef_t *menu, int key);
bool Directory_StartHostMatch();
void Directory_ApplyHostPlayerLimit(const char *mode, bool publicMatch);
void Directory_FrontendFrame();
const char *Directory_BrowserMenu(const char *name);
bool Directory_PopulateServers(int localClientNum, int contextIndex);
void Directory_Frame();
bool Directory_Packet(const netadr_t &, const unsigned char *, int);
void Directory_TransportReset();
bool Directory_Refresh(int source);
void Directory_CancelRefresh();
void Directory_CancelJoin();
void Directory_ConnectionStarted(const char *address);
bool Directory_IsRow(int source, int index);
bool Directory_Source(int source);
bool Directory_UpdateBrowserCounts();
bool Directory_Join(int source, int index);
bool Directory_PromptJoinPassword(int source, int index);
bool Directory_AcceptJoinPassword();
bool Directory_HasPasswordPrompt();
void Directory_CancelJoinPassword();
const char *Directory_Status();
#ifdef KISAK_NX
// The updater popup (directory_update.cpp).
void Directory_UpdateFrame();
bool Directory_UpdateBlocksOnline();
const char *Directory_UpdateGateMenu(const char *name);
bool Directory_UpdateOpenGate();
bool Directory_UpdateMenuEvent(int localClientNum, UiContext *dc, itemDef_s *item, const char *event);
bool Directory_UpdateControllerKey(int localClientNum, UiContext *dc, menuDef_t *menu, int key);
// A front end menu level message (Cmd_MenuLevelMessage_f): SP offers the
// update popup once the TV room is up.
void Directory_FrontendMenuMessage(const char *state, const char *item);
#endif
#else
inline void Directory_MenuOpening(int, UiContext *, menuDef_t *)
{
}
inline bool Directory_HandleMenuEvent(int, UiContext *, itemDef_s *, const char *)
{
    return false;
}
inline bool Directory_HandleControllerKey(int, UiContext *, menuDef_t *, int) { return false; }
inline bool Directory_StartHostMatch()
{
    return false;
}
inline void Directory_ApplyHostPlayerLimit(const char *, bool) {}
inline void Directory_FrontendFrame()
{
}
inline const char *Directory_BrowserMenu(const char *name)
{
    return name;
}
inline void Directory_Init()
{
}
inline void Directory_BeforeConfigWrite()
{
}
inline bool Directory_PopulateServers(int, int)
{
    return false;
}
inline void Directory_Frame()
{
}
inline bool Directory_Packet(const netadr_t &, const unsigned char *, int)
{
    return false;
}
inline void Directory_TransportReset()
{
}
inline bool Directory_Refresh(int)
{
    return false;
}
inline void Directory_CancelRefresh()
{
}
inline void Directory_CancelJoin()
{
}
inline void Directory_ConnectionStarted(const char *)
{
}
inline bool Directory_IsRow(int, int)
{
    return false;
}
inline bool Directory_Source(int)
{
    return false;
}
inline bool Directory_UpdateBrowserCounts() { return false; }
inline bool Directory_Join(int, int)
{
    return false;
}
inline bool Directory_PromptJoinPassword(int, int) { return false; }
inline bool Directory_AcceptJoinPassword() { return false; }
inline bool Directory_HasPasswordPrompt() { return false; }
inline void Directory_CancelJoinPassword() {}
inline const char *Directory_Status()
{
    return "";
}
#endif

#if defined(OPENBLOPS_NO_STEAM_AUTH) && defined(KISAK_SP)
bool Directory_ZombiesPrepareMenus(UiContext *dc);
bool Directory_StartZombieMapNotify(int localClientNum, const char *notify);
#else
inline bool Directory_StartZombieMapNotify(int, const char *) { return false; }
#endif

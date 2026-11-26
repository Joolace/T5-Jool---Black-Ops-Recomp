#pragma once

// Switch 2 builds (KISAK_NX2) install next to their own data, settings and
// shader caches, and present at the screen's 1920x1080 (the system scales it
// to the TV when docked). Keep the NRO names in step with cmake/switch.cmake.
#if defined(KISAK_NX2)
#define NX_GAME_DIR "sdmc:/switch/OpenBLOPS-NX2"
#define NX_SP_NRO "OpenBLOPS-NX2.nro"
#define NX_MP_NRO "OpenBLOPS-NX2-MP.nro"
#define NX_UPDATE_ZIP "OpenBLOPS-NX2.zip"
#define NX_SCREEN_WIDTH 1920
#define NX_SCREEN_HEIGHT 1080
#else
#define NX_GAME_DIR "sdmc:/switch/OpenBLOPS-NX"
#define NX_SP_NRO "OpenBLOPS-NX.nro"
#define NX_MP_NRO "OpenBLOPS-NX-MP.nro"
#define NX_UPDATE_ZIP "OpenBLOPS-NX.zip"
#define NX_SCREEN_WIDTH 1280
#define NX_SCREEN_HEIGHT 720
#endif

#if defined(KISAK_SP)
#define NX_GAME_NRO NX_SP_NRO
#else
#define NX_GAME_NRO NX_MP_NRO
#endif

#pragma once
// ShlObj.h: solo SHGetFolderPathA, usata per la cartella dei salvataggi/profili.
#include "win_compat.h"
#define CSIDL_APPDATA 0x001a
#define CSIDL_LOCAL_APPDATA 0x001c
#define CSIDL_PERSONAL 0x0005
#define CSIDL_COMMON_APPDATA 0x0023
#define CSIDL_FLAG_CREATE 0x8000
#ifdef __cplusplus
extern "C"
#endif
HRESULT SHGetFolderPathA(HWND wnd, int folder, HANDLE token, DWORD flags, LPSTR path);
#define SHGetFolderPath SHGetFolderPathA

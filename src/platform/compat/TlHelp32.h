#pragma once
// TlHelp32.h: elenco dei thread/moduli del processo, usato solo dai profiler interni. Su Linux/Android
// CreateToolhelp32Snapshot fallisce e i profiler saltano quella parte.
#include "win_compat.h"
#define TH32CS_SNAPTHREAD 0x00000004
#define TH32CS_SNAPMODULE 0x00000008
typedef struct tagTHREADENTRY32 { DWORD dwSize, cntUsage, th32ThreadID, th32OwnerProcessID; LONG tpBasePri, tpDeltaPri; DWORD dwFlags; } THREADENTRY32, *LPTHREADENTRY32;
typedef struct tagMODULEENTRY32 { DWORD dwSize, th32ModuleID, th32ProcessID, GlblcntUsage, ProccntUsage; BYTE *modBaseAddr; DWORD modBaseSize; HMODULE hModule; char szModule[256]; char szExePath[MAX_PATH]; } MODULEENTRY32, *LPMODULEENTRY32;
static inline HANDLE CreateToolhelp32Snapshot(DWORD, DWORD) { return INVALID_HANDLE_VALUE; }
static inline BOOL Thread32First(HANDLE, LPTHREADENTRY32) { return FALSE; }
static inline BOOL Thread32Next(HANDLE, LPTHREADENTRY32) { return FALSE; }
static inline BOOL Module32First(HANDLE, LPMODULEENTRY32) { return FALSE; }
static inline BOOL Module32Next(HANDLE, LPMODULEENTRY32) { return FALSE; }

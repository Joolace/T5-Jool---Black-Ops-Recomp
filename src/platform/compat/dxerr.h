#pragma once
#include "win_compat.h"
// DXGetErrorString: solo testo del codice di errore
#ifdef __cplusplus
extern "C"
#endif
const char *DXGetErrorStringA(HRESULT hr);
#define DXGetErrorString DXGetErrorStringA
#define DXGetErrorDescriptionA DXGetErrorStringA

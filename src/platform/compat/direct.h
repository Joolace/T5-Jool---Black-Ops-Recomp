#pragma once
#include <sys/stat.h>
#include <unistd.h>
#ifdef __cplusplus
extern "C"
#endif
int bo1_mkdir(const char *path); // risolve il percorso (src/platform/win_compat.cpp)
#define _mkdir(p) bo1_mkdir(p)
#define _rmdir rmdir

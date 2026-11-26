// Switch implementation of the zone loader's platform hooks (database/db_kbz.h).
#include <database/db_kbz.h>

#include "nx_load_boost.h"

extern "C" void nx_normalize_path(const char *in, char *out, size_t outSize);
extern "C" const char *nx_get_install_dir(void);
extern "C" bool NX_D3D_IsTexture(const void *p);
extern "C" void NX_D3D_HoldDeviceLock(bool hold);

const char *Sys_KbzInstallDir()
{
    return nx_get_install_dir();
}

void Sys_KbzNativePath(const char *path, char *out, size_t outSize)
{
    nx_normalize_path(path, out, outSize);
}

bool Sys_KbzIsTexture(const void *texture)
{
    return NX_D3D_IsTexture(texture);
}

void Sys_KbzHoldDeviceLock(bool hold)
{
    NX_D3D_HoldDeviceLock(hold);
}

bool Sys_KbzBackgroundLoading()
{
    return NX_LoadBoostBackgroundLoading();
}

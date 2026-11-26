// Windows implementation of the zone loader's platform hooks (database/db_kbz.h).
#include <database/db_kbz.h>

#include <cstring>

#include <win32/win_common.h>

const char *Sys_KbzInstallDir()
{
    return Sys_DefaultInstallPath();
}

void Sys_KbzNativePath(const char *path, char *out, size_t outSize)
{
    if (!outSize)
        return;
    strncpy(out, path ? path : "", outSize - 1);
    out[outSize - 1] = 0;
}

// Real textures come from the D3D runtime; zone memory can only hold an
// image definition that was never built into one.
bool Sys_KbzIsTexture(const void *texture)
{
    return texture && !DB_KbzOwnsAddress(texture);
}

// The D3D9 device is created with D3DCREATE_MULTITHREADED (r_multithreaded_device).
void Sys_KbzHoldDeviceLock(bool) {}

bool Sys_KbzBackgroundLoading()
{
    return false;
}

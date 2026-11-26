#pragma once

#include <cstddef>

// Zones converted offline to the 64-bit layout (tools/ffconv), loaded by
// 64-bit builds in place of the PC fastfiles.

struct XZone;

int DB_TryLoadKbz(const char *zoneName, const char *ffFilename, XZone *zone);
void DB_FreeKbz(XZone *zone);
void DB_KbzStopReaders();
void DB_KbzAbortImageBatch();
void DB_KbzValidateImages();
// True when the address lies in a loaded zone, e.g. a GfxImageLoadDef left
// in an image's texture slot.
bool DB_KbzOwnsAddress(const void *address);

// Platform hooks, implemented by each 64-bit platform.
const char *Sys_KbzInstallDir();                                  // folder holding kbz/
void Sys_KbzNativePath(const char *path, char *out, size_t outSize); // engine path -> fopen path
bool Sys_KbzIsTexture(const void *texture);                       // a created D3D texture
void Sys_KbzHoldDeviceLock(bool hold);                            // serialize D3D device use
bool Sys_KbzBackgroundLoading();                                  // MP zone loads behind the menu

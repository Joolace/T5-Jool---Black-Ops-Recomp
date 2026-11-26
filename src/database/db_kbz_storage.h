#pragma once

#include <cstddef>
#include <cstdint>

struct XModel;

struct KbzStorage {
    uint8_t *blocks[16];
    uint32_t sizes[16];
    uint32_t version, blockCount, relocCount, assetCount;
    uint8_t *metadata;
    size_t metadataSize;
    XModel *emptyWeaponModels[16];
};

using KbzProgress = void (*)(size_t completed, size_t total);

// 1: loaded, 0: absent, -1: invalid, unreadable or out of memory.
int DB_ReadKbz(const char *path, KbzStorage **storage, KbzProgress progress = nullptr);
void DB_FreeKbzStorage(KbzStorage *storage);

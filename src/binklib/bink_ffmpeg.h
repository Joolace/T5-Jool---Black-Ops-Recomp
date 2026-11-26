#pragma once

#include <binklib/bink.h>
#include <cstddef>

HBINK Bink_OpenMemory(const void *data, size_t size, unsigned flags);
bool Bink_Finished(HBINK bink);

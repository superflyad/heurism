#ifndef COMPANION_DEVICE_MEMORY_H
#define COMPANION_DEVICE_MEMORY_H
#include "../../kernel/boot_info.h"
/* Bootstrap device mapping uses 2 MiB leaves. Reject RAM in the full envelope. */
int device_memory_envelope(const BootInfo *,U64,U64,MemoryRange *);
#endif

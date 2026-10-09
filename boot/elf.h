#ifndef COMPANION_ELF_H
#define COMPANION_ELF_H
#include "../kernel/boot_info.h"
typedef struct { U64 offset, address, file_size, memory_size; U32 flags; } ElfSegment;
typedef struct { U64 entry, base, size; U32 count; ElfSegment segments[COMPANION_MAX_SEGMENTS]; } ElfPlan;
int elf_plan(const void *, U64, ElfPlan *);
#endif

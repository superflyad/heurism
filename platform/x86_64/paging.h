#ifndef COMPANION_X86_PAGING_H
#define COMPANION_X86_PAGING_H
#include "../../kernel/boot_info.h"
int paging_init(const BootInfo *);
int paging_device_range(const BootInfo *,U64,U64);
extern U8 __text_start[],__text_end[],__rodata_start[],__rodata_end[],__kernel_end[];
#endif

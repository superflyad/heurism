#ifndef COMPANION_KERNEL_HANDOFF_H
#define COMPANION_KERNEL_HANDOFF_H
#include "kernel_uefi.h"
#include "../kernel/boot_info.h"
EFI_STATUS kernel_exit_boot_services(EFI_BOOT_SERVICES *, EFI_HANDLE, BootInfo *, U64, U32 *, int);
#endif

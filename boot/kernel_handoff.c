#include "kernel_handoff.h"
EFI_STATUS kernel_exit_boot_services(EFI_BOOT_SERVICES *bs, EFI_HANDLE image, BootInfo *info,
                                    U64 capacity, U32 *attempts, int stale_once) {
    U32 n; EFI_STATUS status=EFI_INVALID_PARAMETER;
    GetMemoryMapFn get_map=(GetMemoryMapFn)bs->get_memory_map;
    ExitBootServicesFn exit_boot=(ExitBootServicesFn)bs->exit_boot_services;
    *attempts=0;
    for (n=0; n<3; ++n) {
        UINTN size=capacity, key=0, stride=0; U32 version=0;
        status=get_map(&size,(void *)info->memory_map,&key,&stride,&version);
        if (EFI_ERROR(status)) return status;
        if (version!=1 || stride<sizeof(EfiMemoryDescriptor) || stride>256 || !size || size>capacity || size%stride) return EFI_LOAD_ERROR;
        info->memory_map_size=size; info->descriptor_size=stride; info->descriptor_version=version;
        ++*attempts;
        status=exit_boot(image,key+(stale_once && n==0));
        if (!EFI_ERROR(status) || status!=EFI_INVALID_PARAMETER) return status;
        /* No console, allocation, logging or protocol calls between retries. */
    }
    return status;
}

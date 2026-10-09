#ifndef COMPANION_BOOT_INFO_H
#define COMPANION_BOOT_INFO_H
#include "../common/types.h"
#define COMPANION_BOOT_MAGIC 0x314e52454b504d43ULL
#define COMPANION_BOOT_VERSION 1
#define COMPANION_MAX_SEGMENTS 16
#define COMPANION_MAX_RESERVED 24
#define COMPANION_BOOT_VM_SERIAL 1
#define COMPANION_BOOT_I8042 2
#define COMPANION_BOOT_VM_NETWORK 4
#define COMPANION_BOOT_VM_USB 8
#define COMPANION_BOOT_VM_USB_NETWORK 16
#define COMPANION_BOOT_TEST_UD2 0x100
#define COMPANION_BOOT_TEST_PAGEFAULT 0x200
typedef struct { U64 base, size; } MemoryRange;
typedef struct {
    U64 base, size;
    U32 width, height, stride, format;
} BootFramebuffer;
typedef struct {
    U64 magic;
    U32 version, size;
    U64 flags;
    BootFramebuffer framebuffer;
    U64 memory_map, memory_map_size, descriptor_size;
    U32 descriptor_version, reserved_count;
    U64 acpi_rsdp, stack_base, stack_size;
    MemoryRange reserved[COMPANION_MAX_RESERVED];
    U64 reserved_fields[4];
} BootInfo;
_Static_assert(sizeof(BootInfo)==528, "handoff ABI");
_Static_assert(__builtin_offsetof(BootInfo, reserved)==112, "handoff ranges ABI");
#endif

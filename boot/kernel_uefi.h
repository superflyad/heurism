#ifndef COMPANION_KERNEL_UEFI_H
#define COMPANION_KERNEL_UEFI_H
#include "uefi.h"
#define EFI_INVALID_PARAMETER (0x8000000000000000ULL|2)
#define EFI_BUFFER_TOO_SMALL (0x8000000000000000ULL|5)
#define EFI_LOAD_ERROR (0x8000000000000000ULL|1)
typedef struct { U32 type, pad; U64 physical, virtual_address, pages, attributes; } EfiMemoryDescriptor;
typedef EFI_STATUS (EFIAPI *AllocatePagesFn)(U32,U32,UINTN,U64 *);
typedef EFI_STATUS (EFIAPI *FreePagesFn)(U64,UINTN);
typedef EFI_STATUS (EFIAPI *AllocatePoolFn)(U32,UINTN,void **);
typedef EFI_STATUS (EFIAPI *FreePoolFn)(void *);
typedef EFI_STATUS (EFIAPI *GetMemoryMapFn)(UINTN *,void *,UINTN *,UINTN *,U32 *);
typedef EFI_STATUS (EFIAPI *ExitBootServicesFn)(EFI_HANDLE,UINTN);
typedef struct { EFI_GUID guid; void *table; } EfiConfigurationTable;
typedef struct {
    U32 revision; EFI_HANDLE parent; EFI_SYSTEM_TABLE *system; EFI_HANDLE device;
    void *file_path, *reserved; U32 load_options_size; void *load_options;
    void *image_base; U64 image_size; U32 code_type, data_type; void *unload;
} LoadedImage;
typedef struct EfiFile EfiFile;
struct EfiFile {
    U64 revision;
    EFI_STATUS (EFIAPI *open)(EfiFile *,EfiFile **,const U16 *,U64,U64);
    EFI_STATUS (EFIAPI *close)(EfiFile *);
    void *delete_file;
    EFI_STATUS (EFIAPI *read)(EfiFile *,UINTN *,void *);
    void *write,*get_position,*set_position;
    EFI_STATUS (EFIAPI *get_info)(EfiFile *,EFI_GUID *,UINTN *,void *);
};
typedef struct { U64 revision; EFI_STATUS (EFIAPI *open_volume)(void *,EfiFile **); } SimpleFs;
_Static_assert(sizeof(EfiMemoryDescriptor)==40, "EFI memory descriptor ABI");
_Static_assert(__builtin_offsetof(EFI_BOOT_SERVICES, get_memory_map)==56, "memory map ABI");
_Static_assert(__builtin_offsetof(EFI_BOOT_SERVICES, exit_boot_services)==232, "exit ABI");
_Static_assert(__builtin_offsetof(LoadedImage, device)==24, "loaded image ABI");
_Static_assert(__builtin_offsetof(EfiFile, get_info)==64, "file ABI");
#endif

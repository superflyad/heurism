/* VM-only SysPrep probe. It records entry in a volatile runtime variable and exits. */
#include "uefi.h"

typedef struct {
    EFI_TABLE_HEADER header;
    void *get_time, *set_time, *get_wakeup_time, *set_wakeup_time;
    void *set_virtual_address_map, *convert_pointer;
    EFI_STATUS (EFIAPI *get_variable)(U16 *, EFI_GUID *, U32 *, UINTN *, void *);
    void *get_next_variable_name;
    EFI_STATUS (EFIAPI *set_variable)(U16 *, EFI_GUID *, U32, UINTN, void *);
} EFI_PROBE_RUNTIME;

typedef struct {
    U32 magic;
    U16 boot_current;
    U16 reserved;
} PROBE_RECORD;

_Static_assert(__builtin_offsetof(EFI_PROBE_RUNTIME, get_variable) == 72, "GetVariable ABI");
_Static_assert(__builtin_offsetof(EFI_PROBE_RUNTIME, set_variable) == 88, "SetVariable ABI");

static void debug_line(const char *message) {
    while (*message) {
        __asm__ volatile("outb %0, %1" : : "a"(*message++), "Nd"((U16)0xe9));
    }
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system) {
    (void)image;
    if (!system || !system->runtime_services) return EFI_UNSUPPORTED;
    EFI_PROBE_RUNTIME *runtime = (EFI_PROBE_RUNTIME *)system->runtime_services;
    EFI_GUID global = {0x8be4df61, 0x93ca, 0x11d2,
                       {0xaa, 0x0d, 0x00, 0xe0, 0x98, 0x03, 0x2b, 0x8c}};
    EFI_GUID probe = {0x51e384d5, 0xe70c, 0x4403,
                      {0x9d, 0x32, 0x9d, 0x42, 0x7d, 0x6a, 0x13, 0x09}};
    PROBE_RECORD record = {0x50505343U, 0xffffU, 0};
    UINTN size = sizeof(record.boot_current);
    if (!EFI_ERROR(runtime->get_variable((U16 *)L"BootCurrent", &global, 0,
                                         &size, &record.boot_current)) && size != 2)
        record.boot_current = 0xffffU;
    EFI_STATUS status = runtime->set_variable((U16 *)L"CompanionSysPrepProbe", &probe,
                                              0x6U, sizeof(record), &record);
    if (!EFI_ERROR(status)) debug_line("SYSPREP_EXECUTED\n");
    return status;
}

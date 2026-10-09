/* Isolated OVMF setup image: register the VM probe, reset, then verify its marker. */
#include "uefi.h"

typedef struct {
    EFI_TABLE_HEADER header;
    void *get_time, *set_time, *get_wakeup_time, *set_wakeup_time;
    void *set_virtual_address_map, *convert_pointer;
    EFI_STATUS (EFIAPI *get_variable)(U16 *, EFI_GUID *, U32 *, UINTN *, void *);
    void *get_next_variable_name;
    EFI_STATUS (EFIAPI *set_variable)(U16 *, EFI_GUID *, U32, UINTN, void *);
    void *get_next_high_monotonic_count;
    void (EFIAPI *reset_system)(U32, EFI_STATUS, UINTN, void *);
} EFI_SETUP_RUNTIME;

_Static_assert(__builtin_offsetof(EFI_SETUP_RUNTIME, reset_system) == 104, "ResetSystem ABI");

static void debug_line(const char *message) {
    while (*message) __asm__ volatile("outb %0, %1" : : "a"(*message++), "Nd"((U16)0xe9));
}

static void copy(U8 *to, const U8 *from, UINTN size) {
    for (UINTN i = 0; i < size; ++i) to[i] = from[i];
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system) {
    (void)image;
    if (!system || !system->runtime_services) return EFI_UNSUPPORTED;
    EFI_SETUP_RUNTIME *runtime = (EFI_SETUP_RUNTIME *)system->runtime_services;
    EFI_GUID global = {0x8be4df61, 0x93ca, 0x11d2,
                       {0xaa, 0x0d, 0x00, 0xe0, 0x98, 0x03, 0x2b, 0x8c}};
    EFI_GUID probe = {0x51e384d5, 0xe70c, 0x4403,
                      {0x9d, 0x32, 0x9d, 0x42, 0x7d, 0x6a, 0x13, 0x09}};
    U16 order = 0;
    UINTN size = sizeof(order);
    if (!EFI_ERROR(runtime->get_variable((U16 *)L"SysPrepOrder", &global, 0,
                                         &size, &order))) {
        U8 record[8] = {0};
        size = sizeof(record);
        if (!EFI_ERROR(runtime->get_variable((U16 *)L"CompanionSysPrepProbe", &probe,
                                             0, &size, record)) && size == 8 &&
            record[0] == 'C' && record[1] == 'S' && record[2] == 'P' && record[3] == 'P')
            debug_line("BOOT_AFTER_SYSPREP_MARKER_OK\n");
        else debug_line("BOOT_WITHOUT_SYSPREP_MARKER\n");
        return EFI_SUCCESS;
    }

    static const U16 description[] = L"Companion SysPrep VM Probe";
    static const U16 path[] = L"\\EFI\\Companion\\SYSPREP.EFI";
    enum { path_bytes = sizeof(path), desc_bytes = sizeof(description),
           device_bytes = 4 + path_bytes + 4, option_bytes = 6 + desc_bytes + device_bytes };
    U8 option[option_bytes] = {0};
    option[0] = 1;  /* LOAD_OPTION_ACTIVE */
    option[4] = (U8)device_bytes;
    option[5] = (U8)(device_bytes >> 8);
    copy(option + 6, (const U8 *)description, desc_bytes);
    UINTN at = 6 + desc_bytes;
    option[at] = 4; option[at + 1] = 4;  /* Media FilePath device-path node */
    option[at + 2] = (U8)(4 + path_bytes);
    option[at + 3] = (U8)((4 + path_bytes) >> 8);
    copy(option + at + 4, (const U8 *)path, path_bytes);
    at += 4 + path_bytes;
    option[at] = 0x7f; option[at + 1] = 0xff; option[at + 2] = 4;
    EFI_STATUS status = runtime->set_variable((U16 *)L"SysPrep0000", &global,
                                              0x7U, sizeof(option), option);
    if (EFI_ERROR(status)) { debug_line("SET_OPTION_FAILED\n"); return status; }
    status = runtime->set_variable((U16 *)L"SysPrepOrder", &global,
                                   0x7U, sizeof(order), &order);
    if (EFI_ERROR(status)) { debug_line("SET_ORDER_FAILED\n"); return status; }
    debug_line("SYSPREP_REGISTERED\n");
    runtime->reset_system(1, EFI_SUCCESS, 0, 0);
    return EFI_UNSUPPORTED;
}

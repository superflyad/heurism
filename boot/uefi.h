#ifndef COMPANION_UEFI_H
#define COMPANION_UEFI_H
/* Minimal x64 UEFI ABI subset, per UEFI 2.11 sections 4 and 12. */
#include "../common/types.h"
typedef U64 UINTN;
typedef U64 EFI_STATUS;
typedef void *EFI_HANDLE;
typedef void *EFI_EVENT;
#define EFIAPI __attribute__((ms_abi))
#define EFI_SUCCESS 0
#define EFI_ERROR(s) (((s) >> 63) != 0)
#define EFI_UNSUPPORTED (0x8000000000000000ULL | 3)
#define EFI_NOT_READY (0x8000000000000000ULL | 6)
typedef struct { U32 a; U16 b, c; U8 d[8]; } EFI_GUID;
typedef struct { U64 signature; U32 revision, header_size, crc32, reserved; } EFI_TABLE_HEADER;
typedef struct { U16 scan_code, unicode_char; } EFI_INPUT_KEY;
typedef struct EFI_INPUT {
    void *reset;
    EFI_STATUS (EFIAPI *read_key)(struct EFI_INPUT *, EFI_INPUT_KEY *);
    EFI_EVENT wait_for_key;
} EFI_INPUT;
typedef struct EFI_OUTPUT {
    void *reset;
    EFI_STATUS (EFIAPI *output_string)(struct EFI_OUTPUT *, const U16 *);
} EFI_OUTPUT;
typedef struct {
    EFI_TABLE_HEADER header;
    void *raise_tpl, *restore_tpl, *allocate_pages, *free_pages, *get_memory_map;
    void *allocate_pool, *free_pool, *create_event, *set_timer;
    EFI_STATUS (EFIAPI *wait_for_event)(UINTN, EFI_EVENT *, UINTN *);
    void *signal_event, *close_event, *check_event, *install_protocol_interface;
    void *reinstall_protocol_interface, *uninstall_protocol_interface;
    EFI_STATUS (EFIAPI *handle_protocol)(EFI_HANDLE, EFI_GUID *, void **);
    void *reserved, *register_protocol_notify, *locate_handle, *locate_device_path;
    void *install_configuration_table, *load_image, *start_image, *exit;
    void *unload_image, *exit_boot_services, *get_next_monotonic_count;
    EFI_STATUS (EFIAPI *stall)(UINTN);
    EFI_STATUS (EFIAPI *set_watchdog_timer)(UINTN, U64, UINTN, U16 *);
    void *connect_controller, *disconnect_controller, *open_protocol, *close_protocol;
    void *open_protocol_information, *protocols_per_handle, *locate_handle_buffer;
    EFI_STATUS (EFIAPI *locate_protocol)(EFI_GUID *, void *, void **);
    void *install_multiple_protocol_interfaces, *uninstall_multiple_protocol_interfaces;
    void *calculate_crc32, *copy_mem, *set_mem, *create_event_ex;
} EFI_BOOT_SERVICES;
typedef struct {
    EFI_TABLE_HEADER header;
    U16 *firmware_vendor;
    U32 firmware_revision;
    EFI_HANDLE console_in_handle;
    EFI_INPUT *con_in;
    EFI_HANDLE console_out_handle;
    EFI_OUTPUT *con_out;
    EFI_HANDLE standard_error_handle;
    EFI_OUTPUT *std_err;
    void *runtime_services;
    EFI_BOOT_SERVICES *boot_services;
    UINTN table_entries;
    void *configuration_table;
} EFI_SYSTEM_TABLE;
typedef struct { U32 red, green, blue, reserved; } EFI_PIXEL_MASK;
typedef struct {
    U32 version, width, height, pixel_format;
    EFI_PIXEL_MASK masks;
    U32 pixels_per_scanline;
} EFI_GOP_INFO;
typedef struct {
    U32 max_mode, mode;
    EFI_GOP_INFO *info;
    UINTN info_size;
    U64 framebuffer_base;
    UINTN framebuffer_size;
} EFI_GOP_MODE;
typedef struct { void *query_mode, *set_mode, *blt; EFI_GOP_MODE *mode; } EFI_GOP;
_Static_assert(sizeof(void *) == 8, "x64 UEFI only in milestone 0");
_Static_assert(__builtin_offsetof(EFI_SYSTEM_TABLE, boot_services) == 96, "system table ABI");
_Static_assert(__builtin_offsetof(EFI_BOOT_SERVICES, locate_protocol) == 320, "boot services ABI");
_Static_assert(__builtin_offsetof(EFI_BOOT_SERVICES, wait_for_event) == 96, "event ABI");
_Static_assert(__builtin_offsetof(EFI_BOOT_SERVICES, set_watchdog_timer) == 256, "watchdog ABI");
_Static_assert(sizeof(EFI_GOP_INFO) == 36, "GOP info ABI");
#endif

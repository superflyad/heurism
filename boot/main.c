#include "uefi.h"
#include "../ui/framebuffer.h"
static EFI_GUID gop_guid = {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
/* Keep an address relocation so firmware can relocate the PE image explicitly. */
static EFI_GUID *volatile graphics_protocol = &gop_guid;
static void report(EFI_SYSTEM_TABLE *st, const U16 *message) {
    if (st->con_out && st->con_out->output_string) st->con_out->output_string(st->con_out, message);
}
static void report_number(EFI_SYSTEM_TABLE *st, U64 number) {
    U16 value[21];
    const char *digits = "0123456789ABCDEF";
    U32 i;
    value[0] = '0'; value[1] = 'x';
    for (i = 0; i < 16; ++i) value[2 + i] = (U16)digits[(number >> ((15 - i) * 4)) & 15];
    value[18] = '\r'; value[19] = '\n'; value[20] = 0;
    report(st, value);
}
static EFI_STATUS failure(EFI_SYSTEM_TABLE *st, const U16 *stage, EFI_STATUS status) {
    report(st, stage);
    report_number(st, status);
    report(st, (const U16 *)u"Returning to firmware in 15 seconds. Photograph this screen.\r\n");
    if (st->boot_services->stall) st->boot_services->stall(15000000);
    return status;
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    EFI_GOP *gop = 0;
    Framebuffer fb;
    EFI_STATUS status;
    UINTN index;
    EFI_INPUT_KEY key;
    (void)image;
    if (!st || !st->boot_services) return EFI_UNSUPPORTED;
    report(st, (const U16 *)u"\r\nCOMPANION BOOT 0.2 - EFI ENTRY REACHED\r\n");
    /* Disable the boot manager watchdog before initialization or diagnostics. */
    if (!st->boot_services->set_watchdog_timer)
        return failure(st, (const U16 *)u"Watchdog service missing: ", EFI_UNSUPPORTED);
    status = st->boot_services->set_watchdog_timer(0, 0, 0, 0);
    if (EFI_ERROR(status))
        return failure(st, (const U16 *)u"Watchdog disable failed: ", status);
    report(st, (const U16 *)u"Watchdog disabled. Finding the console display...\r\n");
    if (st->boot_services->stall) st->boot_services->stall(2000000);
    /* Prefer the display actually used by the firmware console. */
    status = EFI_UNSUPPORTED;
    if (st->console_out_handle && st->boot_services->handle_protocol)
        status = st->boot_services->handle_protocol(st->console_out_handle, graphics_protocol, (void **)&gop);
    if (EFI_ERROR(status) || !gop) {
        gop = 0;
        if (st->boot_services->locate_protocol)
            status = st->boot_services->locate_protocol(graphics_protocol, 0, (void **)&gop);
    }
    if (!EFI_ERROR(status) && gop && framebuffer_init(&fb, gop->mode)) {
        report(st, (const U16 *)u"Framebuffer validated. Rendering Companion...\r\n");
        companion_screen(&fb);
    } else {
        report(st, (const U16 *)u"Graphics unavailable; Companion is running in text mode.\r\nGOP status: ");
        report_number(st, status);
        if (gop && gop->mode && gop->mode->info && gop->mode->info_size >= sizeof(EFI_GOP_INFO)) {
            report(st, (const U16 *)u"Pixel format (0=RGB, 1=BGR, 2=mask, 3=BLT): ");
            report_number(st, gop->mode->info->pixel_format);
            report(st, (const U16 *)u"Framebuffer base: "); report_number(st, gop->mode->framebuffer_base);
            report(st, (const U16 *)u"Framebuffer bytes: "); report_number(st, gop->mode->framebuffer_size);
        }
        report(st, (const U16 *)u"Press ESC to return to firmware.\r\n");
    }
    if (!st->con_in || !st->con_in->read_key || !st->con_in->wait_for_key || !st->boot_services->wait_for_event)
        return failure(st, (const U16 *)u"Keyboard/event service unavailable: ", EFI_UNSUPPORTED);
    for (;;) {
        status = st->boot_services->wait_for_event(1, &st->con_in->wait_for_key, &index);
        if (EFI_ERROR(status)) return failure(st, (const U16 *)u"Keyboard wait failed: ", status);
        status = st->con_in->read_key(st->con_in, &key);
        if (status == EFI_NOT_READY) continue;
        if (EFI_ERROR(status)) return failure(st, (const U16 *)u"Keyboard read failed: ", status);
        if (key.scan_code == 0x17 || key.unicode_char == 27) return EFI_SUCCESS;
    }
}

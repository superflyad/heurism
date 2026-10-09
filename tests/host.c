#include "../ui/framebuffer.h"
/* Aggregate initialization may lower to memset even in freestanding C. */
void *memset(void *destination, int value, UINTN count) {
    volatile U8 *bytes = (volatile U8 *)destination;
    UINTN n;
    for (n = 0; n < count; ++n) bytes[n] = (U8)value;
    return destination;
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE, EFI_SYSTEM_TABLE *);
static U32 pixels[64 * 48 + 2];
static EFI_GOP_INFO info = {0, 60, 48, 1, {0,0,0,0}, 64};
static EFI_GOP_MODE mode = {1, 0, &info, sizeof(info), 0, 64 * 48 * 4};
static EFI_GOP gop = {0,0,0,&mode};
static int watchdog_calls, reports, key_calls, pauses, console_lookups, protocol_lookups;
static EFI_STATUS protocol_status, watchdog_status, wait_status;
static EFI_STATUS EFIAPI locate(EFI_GUID *guid, void *registration, void **out) {
    (void)registration;
    if (guid->a != 0x9042a9de || guid->d[7] != 0x6a) return EFI_UNSUPPORTED;
    ++protocol_lookups; *out = &gop; return protocol_status;
}
static EFI_STATUS EFIAPI console_gop(EFI_HANDLE handle, EFI_GUID *guid, void **out) {
    (void)guid;
    if (handle != (void *)2) return EFI_UNSUPPORTED;
    ++console_lookups; *out = &gop; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI stall(UINTN microseconds) {
    if (microseconds == 15000000) ++pauses;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI watchdog(UINTN timeout, U64 code, UINTN size, U16 *data) {
    if (timeout || code || size || data) return EFI_UNSUPPORTED;
    ++watchdog_calls; return watchdog_status;
}
static EFI_STATUS EFIAPI wait(UINTN count, EFI_EVENT *event, UINTN *index) {
    if (count != 1 || !*event) return EFI_UNSUPPORTED;
    *index = 0; return wait_status;
}
static EFI_STATUS EFIAPI key(EFI_INPUT *input, EFI_INPUT_KEY *out) {
    (void)input;
    ++key_calls;
    if (key_calls == 1) return EFI_NOT_READY;
    out->scan_code = key_calls == 2 ? 0 : 0x17; out->unicode_char = 'A';
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI report(EFI_OUTPUT *output, const U16 *message) {
    (void)output; if (message && *message) ++reports; return EFI_SUCCESS;
}
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)
int run_tests(void) {
    Framebuffer fb;
    EFI_BOOT_SERVICES bs = {0};
    EFI_SYSTEM_TABLE st = {0};
    EFI_INPUT input = {0,key,(void *)1};
    EFI_OUTPUT output = {0,report};
    U32 i;
    for (i = 0; i < 64 * 48 + 2; ++i) pixels[i] = 0xdeadbeef;
    mode.framebuffer_base = (U64)(UINTN)&pixels[1];
    CHECK(framebuffer_init(&fb, &mode));
    framebuffer_rect(&fb, 59, 47, 0xffffffff, 0xffffffff, 0x123456);
    CHECK(pixels[1 + 47 * 64 + 59] == 0x123456);
    CHECK(pixels[0] == 0xdeadbeef && pixels[64 * 48 + 1] == 0xdeadbeef);
    CHECK(pixels[1 + 47 * 64 + 60] == 0xdeadbeef);
    framebuffer_rect(&fb, 0xffffffff, 0xffffffff, 10, 10, 0);
    info.pixel_format = 0;
    CHECK(framebuffer_init(&fb, &mode));
    framebuffer_rect(&fb, 0, 0, 1, 1, 0x123456);
    CHECK(pixels[1] == 0x563412);
    info.pixel_format = 2; CHECK(!framebuffer_init(&fb, &mode));
    info.pixel_format = 3; CHECK(!framebuffer_init(&fb, &mode));
    info.pixel_format = 1;
    mode.framebuffer_size = 1; CHECK(!framebuffer_init(&fb, &mode));
    mode.framebuffer_size = 64 * 48 * 4;
    info.pixels_per_scanline = 59; CHECK(!framebuffer_init(&fb, &mode));
    info.pixels_per_scanline = 64;
    mode.info_size = 0; CHECK(!framebuffer_init(&fb, &mode));
    mode.info_size = sizeof(info);
    CHECK(!framebuffer_init(&fb, 0));
    bs.locate_protocol = locate; bs.set_watchdog_timer = watchdog; bs.wait_for_event = wait; bs.stall = stall;
    st.boot_services = &bs; st.con_in = &input; st.con_out = &output;
    CHECK(efi_main(0, &st) == EFI_SUCCESS);
    CHECK(watchdog_calls == 1 && key_calls == 3);
    CHECK(pixels[0] == 0xdeadbeef && pixels[64 * 48 + 1] == 0xdeadbeef);
    for (i = 0; i < 48; ++i) CHECK(pixels[1 + i * 64 + 60] == 0xdeadbeef);
    protocol_status = EFI_UNSUPPORTED;
    key_calls = 0; reports = 0;
    CHECK(efi_main(0, &st) == EFI_SUCCESS && reports >= 6);
    protocol_status = 0; info.pixel_format = 3;
    key_calls = 0; reports = 0;
    CHECK(efi_main(0, &st) == EFI_SUCCESS && reports >= 10);
    info.pixel_format = 1;
    watchdog_status = EFI_UNSUPPORTED;
    CHECK(efi_main(0, &st) == EFI_UNSUPPORTED && pauses == 1);
    watchdog_status = 0; wait_status = EFI_UNSUPPORTED;
    CHECK(efi_main(0, &st) == EFI_UNSUPPORTED && pauses == 2);
    CHECK(efi_main(0, 0) == EFI_UNSUPPORTED);
    st.con_in = 0; CHECK(efi_main(0, &st) == EFI_UNSUPPORTED && pauses == 3);
    st.con_in = &input; wait_status = 0; key_calls = 0;
    st.console_out_handle = (void *)2; bs.handle_protocol = console_gop;
    protocol_lookups = 0;
    CHECK(efi_main(0, &st) == EFI_SUCCESS && console_lookups == 1 && protocol_lookups == 0);
    return 0;
}

#include "io.h"
static int enabled;
void serial_init(void) {
    enabled=1; out8(0x3f9,0); out8(0x3fb,0x80); out8(0x3f8,1); out8(0x3f9,0);
    out8(0x3fb,3); out8(0x3fa,0xc7); out8(0x3fc,0x0b);
}
static void put(U8 c) {
    U32 i;
    if (!enabled) return;
    for (i=0;i<100000;++i) if (in8(0x3fd)&0x20) { out8(0x3f8,c); return; }
}
void serial_write(const char *s) { for (;*s;++s) put((U8)*s); }
void serial_hex(U64 n) {
    const char *digits="0123456789ABCDEF"; int i;
    serial_write("0x"); for (i=60;i>=0;i-=4) put((U8)digits[(n>>i)&15]);
}

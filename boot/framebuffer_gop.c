#include "../ui/framebuffer.h"
int framebuffer_init(Framebuffer *fb, const EFI_GOP_MODE *mode) {
    const EFI_GOP_INFO *i;
    U64 required;
    if (!fb || !mode || !mode->info || mode->info_size < sizeof(EFI_GOP_INFO)) return 0;
    i = mode->info;
    if (!i->width || !i->height || i->pixels_per_scanline < i->width || i->pixel_format > 1) return 0;
    required = (U64)i->pixels_per_scanline * i->height;
    if (required > (~(U64)0) / 4) return 0;
    required *= 4;
    if (!mode->framebuffer_base || (mode->framebuffer_base & 3) || required > mode->framebuffer_size) return 0;
    if (mode->framebuffer_base > (~(U64)0) - required) return 0;
    fb->pixels = (volatile U32 *)(UINTN)mode->framebuffer_base;
    fb->width = i->width; fb->height = i->height;
    fb->stride = i->pixels_per_scanline; fb->format = i->pixel_format;
    fb->size = mode->framebuffer_size;
    return 1;
}

#ifndef COMPANION_FRAMEBUFFER_H
#define COMPANION_FRAMEBUFFER_H
#include "../boot/uefi.h"
#include "drawing.h"
int framebuffer_init(Framebuffer *fb, const EFI_GOP_MODE *mode);
void framebuffer_rect(Framebuffer *fb, U32 x, U32 y, U32 w, U32 h, U32 rgb);
void companion_screen(Framebuffer *fb);
#endif

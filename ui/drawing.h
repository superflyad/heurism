#ifndef COMPANION_DRAWING_H
#define COMPANION_DRAWING_H
#include "../common/types.h"
typedef struct { volatile U32 *pixels; U32 width, height, stride, format; U64 size; } Framebuffer;
void framebuffer_rect(Framebuffer *, U32, U32, U32, U32, U32);
void framebuffer_text(Framebuffer *, U32, U32, U32, const char *, U32);
void companion_screen(Framebuffer *);
#endif

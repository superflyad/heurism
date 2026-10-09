#ifndef COMPANION_I8042_H
#define COMPANION_I8042_H
#include "../../common/types.h"
typedef struct { void *context;U8 (*read)(void *,U16);void (*write)(void *,U16,U8); } I8042Io;
typedef struct { U16 code;U8 pressed,character; } KeyEvent;
typedef struct { I8042Io io;U8 ready,extended,pause_bytes,left_shift,right_shift,caps,caps_down; } I8042Keyboard;
int i8042_start(I8042Keyboard *,const I8042Io *);
/* At most 32 controller bytes per call; AUX/parity/timeout bytes are discarded. */
int i8042_poll(I8042Keyboard *,KeyEvent *);
int i8042_decode(I8042Keyboard *,U8,KeyEvent *);
#endif

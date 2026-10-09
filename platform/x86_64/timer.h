#ifndef COMPANION_X86_TIMER_H
#define COMPANION_X86_TIMER_H
#include "../../common/types.h"
int timer_start(void);
void timer_interrupt(void);
U64 timer_ticks(void);
#endif

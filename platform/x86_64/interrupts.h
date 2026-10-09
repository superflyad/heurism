#ifndef COMPANION_X86_INTERRUPTS_H
#define COMPANION_X86_INTERRUPTS_H
#include "../../common/types.h"
typedef struct {
    U64 r15,r14,r13,r12,r11,r10,r9,r8,rdi,rsi,rbp,rdx,rcx,rbx,rax;
    U64 vector,error,rip,cs,flags,rsp,ss;
} InterruptFrame;
void interrupts_init(U64 stack_top);
void x86_interrupt(InterruptFrame *);
void kernel_exception(const InterruptFrame *) __attribute__((noreturn));
#endif

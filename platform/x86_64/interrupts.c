#include "interrupts.h"
#include "timer.h"
typedef struct __attribute__((packed)) { U16 limit; U64 base; } TablePointer;
typedef struct __attribute__((packed)) { U16 low,selector; U8 ist,flags; U16 middle; U32 high,reserved; } Gate;
typedef struct __attribute__((packed)) { U32 reserved0; U64 rsp[3],reserved1,ist[7],reserved2; U16 reserved3,iomap; } Tss;
_Static_assert(sizeof(Tss)==104 && sizeof(Gate)==16 && sizeof(InterruptFrame)==176, "x86 interrupt ABI");
extern void *isr_stubs[35];
extern void x86_load_gdt(const TablePointer *);
static Gate idt[256]; static U64 gdt[5]; static Tss tss;
static U8 double_fault_stack[16384] __attribute__((aligned(16)));
static void gate(U32 vector,void *entry,U8 ist) {
    U64 address=(U64)entry;
    idt[vector]=(Gate){(U16)address,8,ist,0x8e,(U16)(address>>16),(U32)(address>>32),0};
}
void interrupts_init(U64 stack_top) {
    U64 base=(U64)&tss;
    tss.rsp[0]=stack_top; tss.ist[0]=(U64)double_fault_stack+sizeof(double_fault_stack); tss.iomap=sizeof(Tss);
    gdt[0]=0; gdt[1]=0x00af9a000000ffff; gdt[2]=0x00cf92000000ffff;
    gdt[3]=(sizeof(Tss)-1)|((base&0xffffff)<<16)|(0x89ULL<<40)|((base>>24&255)<<56); gdt[4]=base>>32;
    TablePointer gp={sizeof(gdt)-1,(U64)gdt},ip={sizeof(idt)-1,(U64)idt};
    x86_load_gdt(&gp);
    for(U32 i=0;i<256;++i) gate(i,isr_stubs[33],0);
    for(U32 i=0;i<32;++i) gate(i,isr_stubs[i],i==8?1:0);
    gate(32,isr_stubs[32],0);gate(255,isr_stubs[34],0);
    __asm__ volatile("lidt %0"::"m"(ip):"memory");
}
void x86_interrupt(InterruptFrame *frame) {
    if(frame->vector==32) { timer_interrupt(); return; }
    if(frame->vector==255) return;
    kernel_exception(frame);
}

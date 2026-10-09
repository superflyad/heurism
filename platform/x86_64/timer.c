#include "timer.h"
#include "io.h"
static volatile U64 ticks;
static U64 apic_base;
static int x2apic;
static U64 rdmsr(U32 reg) { U32 lo,hi; __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(reg)); return (U64)hi<<32|lo; }
static void wrmsr(U32 reg,U64 value) { __asm__ volatile("wrmsr"::"c"(reg),"a"((U32)value),"d"((U32)(value>>32))); }
static U32 read_apic(U32 offset) {
    return x2apic?(U32)rdmsr(0x800+offset/16):*(volatile U32 *)(apic_base+offset);
}
static void write_apic(U32 offset,U32 value) {
    if(x2apic) wrmsr(0x800+offset/16,value);
    else { *(volatile U32 *)(apic_base+offset)=value; (void)read_apic(0x20); }
}
int timer_start(void) {
    U32 a,b,c,d,spins; U8 previous; U64 base;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    if(!(d&(1U<<9))) return 0;
    base=rdmsr(0x1b); if(!(base&(1ULL<<11))) return 0;
    x2apic=!!(base&(1ULL<<10)); apic_base=base&0x000ffffffffff000ULL;
    if(!x2apic && (!apic_base || apic_base>=0x100000000ULL)) return 0;
    /* Exceptions/IDT are already owned. Mask legacy PIC interrupts. */
    out8(0x21,0xff);out8(0xa1,0xff);
    write_apic(0xf0,0x100|255); write_apic(0x320,(1U<<16)|32);write_apic(0x3e0,3);
    previous=in8(0x61);out8(0x61,previous&~3U);
    out8(0x43,0xb0);out8(0x42,11932&255);out8(0x42,11932>>8);
    write_apic(0x380,0xffffffff);out8(0x61,(previous&~2U)|1);
    for(spins=0;spins<10000000;++spins) if(in8(0x61)&0x20) break;
    U32 elapsed=0xffffffff-read_apic(0x390);out8(0x61,previous);write_apic(0x380,0);
    if(spins==10000000 || !elapsed || elapsed>100000000) return 0;
    ticks=0;write_apic(0x320,(1U<<17)|32);write_apic(0x380,elapsed);
    return 1;
}
void timer_interrupt(void) { ++ticks;write_apic(0xb0,0); }
U64 timer_ticks(void) { return ticks; }

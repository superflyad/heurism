#ifndef COMPANION_X86_IO_H
#define COMPANION_X86_IO_H
#include "../../common/types.h"
static inline void out8(U16 port,U8 value) { __asm__ volatile("outb %0,%1"::"a"(value),"Nd"(port)); }
static inline U8 in8(U16 port) { U8 v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port)); return v; }
static inline void cpu_halt(void) { __asm__ volatile("cli; hlt"); }
void serial_init(void);
void serial_write(const char *);
void serial_hex(U64);
#endif

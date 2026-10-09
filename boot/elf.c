#include "elf.h"
typedef struct {
    U8 ident[16]; U16 type, machine; U32 version;
    U64 entry, phoff, shoff; U32 flags; U16 ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} ElfHeader;
typedef struct { U32 type, flags; U64 offset, vaddr, paddr, filesz, memsz, align; } ProgramHeader;
_Static_assert(sizeof(ElfHeader)==64 && sizeof(ProgramHeader)==56, "ELF64 ABI");
static U64 down(U64 n) { return n & ~4095ULL; }
static U64 up(U64 n) { return (n+4095) & ~4095ULL; }
int elf_plan(const void *buffer, U64 length, ElfPlan *plan) {
    const ElfHeader *h=buffer;
    U32 n, j; U64 low=~0ULL, high=0; int executable_entry=0;
    if (!buffer || !plan || length<sizeof(*h) || length>16*1024*1024) return 0;
    if (h->ident[0]!=0x7f || h->ident[1]!='E' || h->ident[2]!='L' || h->ident[3]!='F' ||
        h->ident[4]!=2 || h->ident[5]!=1 || h->ident[6]!=1 || h->ident[7]!=0 ||
        h->type!=2 || h->machine!=62 || h->version!=1 || h->flags ||
        h->ehsize!=64 || h->phentsize!=56 || !h->phnum || h->phnum>32 ||
        h->phoff<64 || h->phoff>length || (U64)h->phnum*56>length-h->phoff) return 0;
    plan->count=0;
    for (n=0; n<h->phnum; ++n) {
        /* Byte copy permits unaligned table offsets without unaligned struct access. */
        ProgramHeader p; U8 *d=(U8 *)&p; const U8 *s=(const U8 *)buffer+h->phoff+(U64)n*56;
        for (j=0; j<56; ++j) d[j]=s[j];
        if (p.type==2 || p.type==3 || p.type==7 || (p.type==0x6474e551 && (p.flags&1))) return 0;
        if (p.type!=1) continue;
        if (!p.memsz || plan->count==COMPANION_MAX_SEGMENTS || p.vaddr!=p.paddr ||
            p.paddr<0x2000000 || p.paddr>=0x4000000 || p.memsz>0x4000000-p.paddr ||
            p.filesz>p.memsz || p.offset>length || p.filesz>length-p.offset ||
            p.flags&~7U || !(p.flags&4) || (p.flags&3)==3 ||
            !p.align || (p.align&(p.align-1)) || p.align>0x200000 ||
            (p.vaddr&(p.align-1))!=(p.offset&(p.align-1))) return 0;
        for (j=0; j<plan->count; ++j) {
            const ElfSegment *q=&plan->segments[j];
            if (down(p.paddr)<up(q->address+q->memory_size) && down(q->address)<up(p.paddr+p.memsz)) return 0;
        }
        if ((p.flags&1) && h->entry>=p.paddr && h->entry-p.paddr<p.filesz) executable_entry=1;
        plan->segments[plan->count++]=(ElfSegment){p.offset,p.paddr,p.filesz,p.memsz,p.flags};
        if (down(p.paddr)<low) low=down(p.paddr);
        if (up(p.paddr+p.memsz)>high) high=up(p.paddr+p.memsz);
    }
    if (!plan->count || !executable_entry || high-low>16*1024*1024) return 0;
    plan->entry=h->entry; plan->base=low; plan->size=high-low;
    return 1;
}

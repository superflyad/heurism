#include "paging.h"
#include "device_memory.h"
#include "sparse_identity.h"
/* Bootstrap supervisor identity map. No userspace mappings or higher-half yet. */
static U64 pml4[512] __attribute__((aligned(4096)));
static U64 pdpt[512] __attribute__((aligned(4096)));
static U64 directories[64][512] __attribute__((aligned(4096)));
static U64 kernel_pages[512] __attribute__((aligned(4096)));
static SparseIdentity sparse;
#define NX (1ULL<<63)
#define LIMIT (64ULL*1024*1024*1024)
static int mapped(U64 base,U64 size) { return base<LIMIT && size<=LIMIT-base; }
int paging_device_range(const BootInfo *boot,U64 base,U64 size) {
    MemoryRange r;if(!device_memory_envelope(boot,base,size,&r)) return 0;
    /* Single CPU, interrupts disabled, no secondary aliases. Flush prior cache
     * lines before changing WB to UC and invalidate every old translation. */
    U64 flags;__asm__ volatile("pushfq; popq %0":"=r"(flags));if(flags&(1ULL<<9)) return 0;
    __asm__ volatile("wbinvd":::"memory");
    if(!sparse_identity_uc(&sparse,r.base,r.size)) return 0;
    U64 cr4;__asm__ volatile("mov %%cr4,%0":"=r"(cr4));
    __asm__ volatile("mov %0,%%cr4"::"r"(cr4&~(1ULL<<7)):"memory");
    __asm__ volatile("mov %0,%%cr3"::"r"((U64)pml4):"memory");
    __asm__ volatile("mov %0,%%cr4"::"r"(cr4):"memory");
    __asm__ volatile("wbinvd":::"memory");return 1;
}
int paging_init(const BootInfo *b) {
    U64 cr4,cr0,efer; U32 a,c,d,unused,max_leaf,bits=36;
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4));if(cr4&(1ULL<<12)) return 0;
    __asm__ volatile("cpuid":"=a"(a),"=b"(unused),"=c"(c),"=d"(d):"a"(0x80000000),"c"(0));max_leaf=a;if(a<0x80000001) return 0;
    __asm__ volatile("cpuid":"=a"(a),"=b"(unused),"=c"(c),"=d"(d):"a"(0x80000001),"c"(0));if(!(d&(1U<<20))) return 0;
    if(!mapped((U64)b,sizeof(*b)) || !mapped(b->memory_map,b->memory_map_size) ||
       !mapped(b->stack_base,b->stack_size) ||
       (U64)__text_start!=0x2000000 || (U64)__kernel_end>0x2200000) return 0;
    for(U32 i=0;i<512;++i) pml4[i]=pdpt[i]=0;
    pml4[0]=(U64)pdpt|3;
    for(U32 i=0;i<64;++i) {
        pdpt[i]=(U64)directories[i]|3;
        for(U32 j=0;j<512;++j) {
            U64 physical=(U64)i*0x40000000+(U64)j*0x200000,flags=0x83|NX;
            if(physical==0xfee00000) flags|=0x18; /* APIC: uncached with the default PAT. */
            directories[i][j]=physical|flags;
        }
    }
    for(U32 i=0;i<512;++i) {
        U64 physical=0x2000000+(U64)i*4096,flags=3|NX;
        if(physical>=(U64)__text_start && physical<(U64)__text_end) flags=1;
        else if(physical>=(U64)__rodata_start && physical<(U64)__rodata_end) flags=1|NX;
        kernel_pages[i]=physical|flags;
    }
    directories[0][16]=(U64)kernel_pages|3;
    if(max_leaf>=0x80000008) {
        __asm__ volatile("cpuid":"=a"(a),"=b"(unused),"=c"(c),"=d"(d):"a"(0x80000008),"c"(0));bits=a&255;
    }
    if(!sparse_identity_init(&sparse,pml4,pdpt,&directories[0][0],bits)) return 0;
    U64 base=b->framebuffer.base&~4095ULL,end;
    if(!b->framebuffer.size || b->framebuffer.base>~0ULL-b->framebuffer.size-4095) return 0;
    end=(b->framebuffer.base+b->framebuffer.size+4095)&~4095ULL;
    MemoryRange envelope;
    if(!device_memory_envelope(b,base,end-base,&envelope) || !sparse_identity_uc(&sparse,envelope.base,envelope.size)) return 0;
    U32 lo,hi;
    __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000080));efer=((U64)hi<<32|lo)|(1ULL<<11);
    __asm__ volatile("wrmsr"::"c"(0xc0000080),"a"((U32)efer),"d"((U32)(efer>>32)));
    /* Own a known PAT, then flush firmware global translations and use CR3. */
    __asm__ volatile("wrmsr"::"c"(0x277),"a"(0x00070406U),"d"(0x00070406U));
    __asm__ volatile("mov %0,%%cr4"::"r"(cr4&~(1ULL<<7)):"memory");
    __asm__ volatile("mov %0,%%cr3"::"r"((U64)pml4):"memory");
    __asm__ volatile("mov %0,%%cr4"::"r"(cr4):"memory");
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0));cr0|=1ULL<<16;
    __asm__ volatile("mov %0,%%cr0"::"r"(cr0):"memory");
    return 1;
}

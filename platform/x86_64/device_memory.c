#include "device_memory.h"
static U64 little(const U8 *p,U32 n) { U64 v=0;for(U32 i=0;i<n;++i) v|=(U64)p[i]<<(8*i);return v; }
static int overlap(U64 a,U64 size,U64 b,U64 length) { return a<b+length && b<a+size; }
int device_memory_envelope(const BootInfo *boot,U64 base,U64 size,MemoryRange *out) {
    /* Identity-mapped low canonical half; CPU width is checked by the mapper. */
    const U64 limit=1ULL<<47,mask=0x1fffff;
    if(!boot || !out || !base || !size || ((base|size)&4095) || base>=limit || size>limit-base ||
       !boot->memory_map || !boot->memory_map_size || boot->memory_map_size>256*1024 ||
       boot->descriptor_version!=1 || boot->descriptor_size<40 || boot->descriptor_size>256 ||
       boot->memory_map_size%boot->descriptor_size || boot->memory_map>~0ULL-boot->memory_map_size ||
       boot->reserved_count>COMPANION_MAX_RESERVED) return 0;
    U64 start=base&~mask,end=(base+size+mask)&~mask;
    if(start<0x100000 || end>limit || end<=start) return 0;
    for(U32 i=0;i<boot->reserved_count;++i) {
        MemoryRange r=boot->reserved[i];
        if(r.size>~0ULL-r.base || overlap(start,end-start,r.base,r.size)) return 0;
    }
    for(U64 offset=0;offset<boot->memory_map_size;offset+=boot->descriptor_size) {
        const U8 *d=(const U8 *)boot->memory_map+offset;U32 type=(U32)little(d,4);
        U64 physical=little(d+8,8),pages=little(d+24,8);
        if(pages>(~0ULL-physical)/4096) return 0;
        /* Unlisted/reserved/MMIO may describe an ECAM window. Never convert
         * loader, runtime, conventional, ACPI, persistent or unknown memory. */
        if(type!=0 && type!=11 && type!=12 && overlap(start,end-start,physical,pages*4096)) return 0;
    }
    *out=(MemoryRange){start,end-start};return 1;
}

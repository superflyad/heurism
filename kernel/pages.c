#include "pages.h"
#define PAGE_COUNT (0x100000000ULL/4096)
/* Initially manage conventional RAM below 4 GiB; never reclaim firmware data. */
static U64 available[PAGE_COUNT/64], allocated[PAGE_COUNT/64], free_count;
typedef struct { U32 type,pad; U64 physical,virtual_address,pages,attributes; } MapDescriptor;
static int overlaps(U64 base,U64 size,const BootInfo *b) {
    for(U32 n=0;n<b->reserved_count;++n) {
        MemoryRange r=b->reserved[n];
        if(r.base>~0ULL-r.size || (base<r.base+r.size && r.base<base+size)) return 1;
    }
    return 0;
}
int pages_init(const BootInfo *b) {
    if(!b || b->descriptor_version!=1 || !b->memory_map || b->memory_map_size>256*1024 || b->descriptor_size<40 ||
       b->descriptor_size>256 || !b->memory_map_size || b->memory_map_size%b->descriptor_size ||
       b->memory_map>~0ULL-b->memory_map_size || b->reserved_count>COMPANION_MAX_RESERVED) return 0;
    for(U64 i=0;i<PAGE_COUNT/64;++i) available[i]=allocated[i]=0;
    free_count=0;
    for(U64 at=0;at<b->memory_map_size;at+=b->descriptor_size) {
        MapDescriptor d; U8 *p=(U8 *)&d; const U8 *s=(const U8 *)b->memory_map+at;
        for(U32 n=0;n<40;++n) p[n]=s[n];
        if(d.type!=7 || (d.attributes>>63)) continue;
        if((d.physical&4095) || d.pages>(~0ULL-d.physical)/4096) return 0;
        U64 end=d.physical+d.pages*4096;
        if(end>0x100000000ULL) end=0x100000000ULL;
        for(U64 addr=d.physical<0x100000?0x100000:d.physical;addr<end;addr+=4096) {
            if(overlaps(addr,4096,b)) continue;
            U64 index=addr/4096,mask=1ULL<<(index%64);
            if(!(available[index/64]&mask)) { available[index/64]|=mask; ++free_count; }
        }
    }
    return free_count!=0;
}
U64 page_alloc(void) {
    for(U64 i=0;i<PAGE_COUNT/64;++i) if(available[i]) {
        U32 bit=(U32)__builtin_ctzll(available[i]); U64 mask=1ULL<<bit;
        available[i]&=~mask; allocated[i]|=mask; --free_count; return (i*64+bit)*4096;
    }
    return 0;
}
int page_free(U64 address) {
    if(address<0x100000 || address>=0x100000000ULL || (address&4095)) return 0;
    U64 i=address/4096,mask=1ULL<<(i%64);
    if(!(allocated[i/64]&mask)) return 0;
    allocated[i/64]&=~mask; available[i/64]|=mask; ++free_count; return 1;
}
U64 pages_available(void) { return free_count; }

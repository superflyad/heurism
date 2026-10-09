#include "acpi_memory.h"
static U64 little(const U8 *p,U32 n) { U64 v=0;for(U32 i=0;i<n;++i) v|=(U64)p[i]<<(8*i);return v; }
const U8 *acpi_memory_read(void *context,U64 address,U64 size) {
    const BootInfo *b=context;const U64 limit=64ULL*1024*1024*1024;
    if(!b || !address || !size || address>=limit || size>limit-address || !b->memory_map ||
       b->descriptor_version!=1 || b->descriptor_size<40 || b->descriptor_size>256 ||
       b->memory_map_size%b->descriptor_size) return 0;
    U64 cursor=address,end=address+size;
    /* A table may cross adjacent ACPI descriptors. Every byte must be covered. */
    while(cursor<end) {
        U64 next=cursor;
        for(U64 offset=0;offset<b->memory_map_size;offset+=b->descriptor_size) {
            const U8 *d=(const U8 *)b->memory_map+offset;U32 type=(U32)little(d,4);
            U64 base=little(d+8,8),pages=little(d+24,8);
            if((type!=9 && type!=10) || pages>~0ULL/4096 || pages*4096>~0ULL-base) continue;
            U64 stop=base+pages*4096;
            if(base<=cursor && stop>next) next=stop;
        }
        if(next==cursor) return 0;cursor=next<end?next:end;
    }
    return (const U8 *)address;
}

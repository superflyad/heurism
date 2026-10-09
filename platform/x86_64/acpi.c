#include "acpi.h"
static U64 little(const U8 *p,U32 n) { U64 v=0; for(U32 i=0;i<n;++i) v|=(U64)p[i]<<(i*8);return v; }
static int signature(const U8 *p,const char *s,U32 n) { for(U32 i=0;i<n;++i) if(p[i]!=(U8)s[i]) return 0;return 1; }
static int checksum(const U8 *p,U32 n) { U8 sum=0;while(n--) sum+=*p++;return !sum; }
static const U8 *table(U64 address,AcpiRead read,void *context,U64 *budget,U32 *length) {
    const U8 *p=read(context,address,36);if(!p) return 0;
    U32 size=(U32)little(p+4,4);
    if(size<36 || size>1024*1024 || size>*budget) return 0;
    p=read(context,address,size);if(!p || !checksum(p,size)) return 0;
    *budget-=size;*length=size;return p;
}
static int madt(const U8 *p,U32 size,AcpiInventory *out) {
    if(size<44) return 0;
    out->local_apic=little(p+36,4);
    for(U32 offset=44;offset<size;) {
        if(size-offset<2 || p[offset+1]<2 || p[offset+1]>size-offset) return 0;
        U32 type=p[offset],n=p[offset+1];const U8 *entry=p+offset;
        if(type==0) { if(n<8) return 0;if(little(entry+4,4)&3) ++out->processor_count; }
        if(type==1) { if(n<12) return 0;++out->io_apic_count; }
        if(type==5) { if(n<12) return 0;out->local_apic=little(entry+4,8); }
        if(type==9) { if(n<16) return 0;if(little(entry+8,4)&3) ++out->processor_count; }
        offset+=n;
    }
    return 1;
}
static int mcfg(const U8 *p,U32 size,AcpiInventory *out) {
    if(size<44 || (size-44)%16 || (size-44)/16>ACPI_MAX_ECAM) return 0;
    for(U32 i=0;i<8;++i) if(p[36+i]) return 0;
    for(U32 offset=44;offset<size;offset+=16) {
        AcpiEcam e={little(p+offset,8),(U16)little(p+offset+8,2),p[offset+10],p[offset+11]};
        if(!e.base || (e.base&0xfffff) || e.first_bus>e.last_bus || little(p+offset+12,4) ||
           e.base>~0ULL-(((U64)e.last_bus+1)<<20)) return 0;
        for(U32 i=0;i<out->ecam_count;++i) {
            const AcpiEcam *old=&out->ecam[i];
            if(old->segment==e.segment && e.first_bus<=old->last_bus && old->first_bus<=e.last_bus) return 0;
        }
        out->ecam[out->ecam_count++]=e;
    }
    return 1;
}
int acpi_inventory(U64 rsdp,AcpiRead read,void *context,AcpiInventory *out) {
    if(!out) return -1;
    *out=(AcpiInventory){0};if(!rsdp) return 0;if(!read) return -1;
    const U8 *p=read(context,rsdp,36);
    if(!p || !signature(p,"RSD PTR ",8) || !checksum(p,20) || p[15]<2) return -1;
    U32 size=(U32)little(p+20,4);
    if(size<36 || size>4096) return -1;
    p=read(context,rsdp,size);if(!p || !checksum(p,size)) return -1;
    AcpiInventory found={0};found.xsdt=little(p+24,8);
    U64 budget=8*1024*1024;
    const U8 *root=table(found.xsdt,read,context,&budget,&size);
    if(!root || !signature(root,"XSDT",4) || (size-36)%8 || (size-36)/8>ACPI_MAX_TABLES) return -1;
    int seen_madt=0,seen_mcfg=0;
    for(U32 offset=36;offset<size;offset+=8) {
        U64 address=little(root+offset,8);U32 length;
        if(!address || address==found.xsdt) return -1;
        for(U32 i=0;i<found.table_count;++i) if(found.tables[i].address==address) return -1;
        p=table(address,read,context,&budget,&length);if(!p) return -1;
        AcpiTable *entry=&found.tables[found.table_count++];entry->address=address;entry->length=length;
        for(U32 i=0;i<4;++i) entry->signature[i]=(char)p[i];
        if(signature(p,"APIC",4)) { if(seen_madt++ || !madt(p,length,&found)) return -1; }
        if(signature(p,"MCFG",4)) { if(seen_mcfg++ || !mcfg(p,length,&found)) return -1; }
    }
    *out=found;return 1;
}

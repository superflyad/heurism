#include "pci.h"
int pci_configuration_address(const AcpiEcam *e,U32 bus,U32 slot,U32 function,U32 offset,U64 *out) {
    if(!e || !out || !e->base || (e->base&0xfffff) || e->first_bus>e->last_bus ||
       bus<e->first_bus || bus>e->last_bus || slot>31 || function>7 || offset>4092 || (offset&3)) return 0;
    U64 delta=((U64)bus<<20)|((U64)slot<<15)|((U64)function<<12)|offset;
    if(e->base>~0ULL-delta || e->base+delta>~0ULL-4) return 0;
    *out=e->base+delta;return 1;
}
static int read_config(const AcpiEcam *e,U32 bus,U32 slot,U32 function,U32 offset,PciRead32 read,void *context,U32 *value,U32 *reads) {
    U64 address;if(!pci_configuration_address(e,bus,slot,function,offset,&address)) return 0;
    ++*reads;return read(context,address,value);
}
int pci_inventory(const AcpiInventory *acpi,PciRead32 read,void *context,PciInventory *out) {
    if(!out) return 0;*out=(PciInventory){0};
    if(!acpi || !read || !acpi->ecam_count || acpi->ecam_count>ACPI_MAX_ECAM) return 0;
    /* Validate all windows before issuing even one configuration read. */
    for(U32 i=0;i<acpi->ecam_count;++i) {
        const AcpiEcam *e=&acpi->ecam[i];U64 address;
        if(!pci_configuration_address(e,e->last_bus,31,7,4092,&address)) return 0;
        for(U32 j=0;j<i;++j) {
            const AcpiEcam *p=&acpi->ecam[j];
            if(p->segment==e->segment && p->first_bus<=e->last_bus && e->first_bus<=p->last_bus) return 0;
            U64 a=e->base+((U64)e->first_bus<<20),b=p->base+((U64)p->first_bus<<20);
            U64 end=e->base+(((U64)e->last_bus+1)<<20),old_end=p->base+(((U64)p->last_bus+1)<<20);
            if(a<old_end && b<end) return 0;
        }
    }
    U32 count=0,reads=0;
    for(U32 i=0;i<acpi->ecam_count;++i) for(U32 bus=acpi->ecam[i].first_bus;bus<=acpi->ecam[i].last_bus;++bus) for(U32 slot=0;slot<32;++slot) {
        U32 functions=1;
        for(U32 function=0;function<functions;++function) {
            const AcpiEcam *e=&acpi->ecam[i];U32 id,classes,header;
            if(!read_config(e,bus,slot,function,0,read,context,&id,&reads)) goto failed;
            if((id&65535)==65535) continue;
            if(!(id&65535) || count==PCI_MAX_DEVICES ||
               !read_config(e,bus,slot,function,8,read,context,&classes,&reads) ||
               !read_config(e,bus,slot,function,12,read,context,&header,&reads)) goto failed;
            PciDevice *d=&out->devices[count++];
            d->segment=e->segment;d->vendor=(U16)id;d->device=(U16)(id>>16);
            d->bus=(U8)bus;d->slot=(U8)slot;d->function=(U8)function;
            d->revision=(U8)classes;d->programming_interface=(U8)(classes>>8);
            d->subclass=(U8)(classes>>16);d->class_code=(U8)(classes>>24);d->header_type=(U8)(header>>16);
            if(!function && (d->header_type&0x80)) functions=8;
            if(!pci_configuration_address(e,bus,slot,function,0,&d->configuration)) goto failed;
            U32 bar_count=(d->header_type&0x7f)==0?6:(d->header_type&0x7f)==1?2:0;
            for(U32 bar=0;bar<bar_count;++bar) if(!read_config(e,bus,slot,function,16+4*bar,read,context,&d->bars[bar],&reads)) goto failed;
        }
    }
    out->count=count;out->reads=reads;return 1;
failed:
    *out=(PciInventory){0};return 0;
}

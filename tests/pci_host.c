#include "../platform/x86_64/pci.h"
#include "../platform/x86_64/device_memory.h"
#define CHECK(x) do { if(!(x)) return __LINE__; } while(0)
static U32 mode,calls;
static PciInventory out;
static int mock_read(void *context,U64 address,U32 *value) {
    (void)context;++calls;if(mode==1 && calls==5) return 0;
    U32 bus=(U32)((address-0xb0000000)>>20),slot=(U32)(address>>15)&31,function=(U32)(address>>12)&7,offset=(U32)address&4095;
    int exists=mode==2 || (bus==7 && ((slot==0 && !function) || (slot==1 && (function==0 || function==3))));
    if(offset==0) *value=exists?0x12348086:0xffffffff;
    else if(offset==8) *value=slot==0?0x06000001:0x0c033002;
    else if(offset==12) *value=slot==1 && !function?0x00800000:0;
    else if(offset==16) *value=0xd0000004;
    else if(offset==20) *value=1;
    else *value=0;
    return 1;
}
int run_pci_tests(void) {
    AcpiInventory acpi={0};acpi.ecam_count=1;acpi.ecam[0]=(AcpiEcam){0xb0000000,2,7,8};U64 address;
    CHECK(pci_configuration_address(&acpi.ecam[0],7,1,3,4092,&address) && address==0xb070bffc);
    CHECK(!pci_configuration_address(&acpi.ecam[0],6,1,3,0,&address));
    CHECK(!pci_configuration_address(&acpi.ecam[0],7,32,0,0,&address));
    CHECK(!pci_configuration_address(&acpi.ecam[0],7,0,8,0,&address));
    CHECK(!pci_configuration_address(&acpi.ecam[0],7,0,0,4096,&address));
    CHECK(!pci_configuration_address(&acpi.ecam[0],7,0,0,1,&address));
    mode=calls=0;CHECK(pci_inventory(&acpi,mock_read,0,&out));
    CHECK(out.count==3 && out.reads==calls && out.devices[2].function==3 && out.devices[0].segment==2);
    CHECK(out.devices[0].vendor==0x8086 && out.devices[0].device==0x1234 && out.devices[1].programming_interface==0x30);
    CHECK(out.devices[2].bars[0]==0xd0000004 && out.devices[2].bars[1]==1);
    mode=1;calls=0;CHECK(!pci_inventory(&acpi,mock_read,0,&out) && !out.count);
    mode=2;calls=0;acpi.ecam[0].first_bus=0;acpi.ecam[0].last_bus=8;
    CHECK(!pci_inventory(&acpi,mock_read,0,&out) && !out.count && calls<3000);
    mode=calls=0;acpi.ecam_count=2;acpi.ecam[1]=acpi.ecam[0];
    CHECK(!pci_inventory(&acpi,mock_read,0,&out) && !calls);
    acpi.ecam[1].segment=3;CHECK(!pci_inventory(&acpi,mock_read,0,&out) && !calls);
    acpi.ecam_count=1;acpi.ecam[0].base=0xfffffffffff00000ULL;
    CHECK(!pci_inventory(&acpi,mock_read,0,&out) && !calls);
    acpi.ecam[0].base=0xb0000001;CHECK(!pci_inventory(&acpi,mock_read,0,&out) && !calls);
    acpi.ecam_count=0;CHECK(!pci_inventory(&acpi,mock_read,0,&out));
    U64 descriptors[10]={7,0x100000,0,128,0,11,0xc0000000,0,512,0};
    BootInfo boot={0};boot.memory_map=(U64)descriptors;boot.memory_map_size=sizeof(descriptors);boot.descriptor_size=40;boot.descriptor_version=1;
    MemoryRange r;
    CHECK(device_memory_envelope(&boot,0xc0100000,0x100000,&r) && r.base==0xc0000000 && r.size==0x200000);
    CHECK(!device_memory_envelope(&boot,0x100000,0x100000,&r));
    descriptors[5]=7;CHECK(!device_memory_envelope(&boot,0xc0100000,0x100000,&r));
    descriptors[5]=11;boot.reserved_count=1;boot.reserved[0]=(MemoryRange){0xc0000000,4096};
    CHECK(!device_memory_envelope(&boot,0xc0100000,0x100000,&r));
    boot.reserved_count=0;CHECK(!device_memory_envelope(&boot,~0ULL-4095,4096,&r));
    CHECK(!device_memory_envelope(&boot,64ULL*1024*1024*1024,4096,&r));
    CHECK(!device_memory_envelope(&boot,0xc0000001,4096,&r));
    descriptors[8]=~0ULL;CHECK(!device_memory_envelope(&boot,0xc0000000,4096,&r));
    return 0;
}

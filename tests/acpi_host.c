#include "../platform/x86_64/acpi.h"
#include "../platform/x86_64/acpi_memory.h"
#define CHECK(x) do { if(!(x)) return __LINE__; } while(0)
static U8 bytes[4096];
static U64 denied;
static void put(U32 offset,U64 value,U32 size) { for(U32 i=0;i<size;++i) bytes[offset+i]=(U8)(value>>(i*8)); }
static void sum(U32 offset,U32 size,U32 at) {
    bytes[offset+at]=0;U8 value=0;for(U32 i=0;i<size;++i) value+=bytes[offset+i];bytes[offset+at]=(U8)-value;
}
static void header(U32 offset,const char *s,U32 size) {
    for(U32 i=0;i<4;++i) bytes[offset+i]=(U8)s[i];put(offset+4,size,4);bytes[offset+8]=1;
}
static void fixture(void) {
    for(U32 i=0;i<sizeof(bytes);++i) bytes[i]=0;denied=0;
    const char *rsdp="RSD PTR ";for(U32 i=0;i<8;++i) bytes[i]=(U8)rsdp[i];bytes[15]=2;
    put(20,36,4);put(24,0x1040,8);sum(0,20,8);sum(0,36,32);
    header(64,"XSDT",52);put(100,0x1100,8);put(108,0x1200,8);sum(64,52,9);
    header(256,"APIC",64);put(292,0xfee00000,4);bytes[301]=8;put(304,1,4);
    bytes[308]=1;bytes[309]=12;put(312,0xfec00000,4);sum(256,64,9);
    header(512,"MCFG",60);put(556,0xb0000000,8);bytes[567]=255;sum(512,60,9);
}
static const U8 *read_memory(void *context,U64 address,U64 size) {
    (void)context;
    if(address<0x1000 || address-0x1000>sizeof(bytes) || size>sizeof(bytes)-(address-0x1000) ||
       (denied && address<=denied && size>denied-address)) return 0;
    return bytes+address-0x1000;
}
static int rejected(void) { AcpiInventory out;int result=acpi_inventory(0x1000,read_memory,0,&out);return result==-1 && !out.table_count && !out.xsdt; }
int run_acpi_tests(void) {
    AcpiInventory out;fixture();CHECK(acpi_inventory(0x1000,read_memory,0,&out)==1);
    CHECK(out.table_count==2 && out.processor_count==1 && out.io_apic_count==1 && out.ecam_count==1 && out.local_apic==0xfee00000);
    CHECK(out.ecam[0].base==0xb0000000 && out.ecam[0].first_bus==0 && out.ecam[0].last_bus==255);
    CHECK(acpi_inventory(0,read_memory,0,&out)==0 && !out.table_count);
    CHECK(acpi_inventory(0x1000,0,0,&out)==-1);
    fixture();bytes[0]='X';CHECK(rejected());
    fixture();bytes[8]++;CHECK(rejected());
    fixture();bytes[32]++;CHECK(rejected());
    fixture();put(20,4097,4);sum(0,20,8);sum(0,36,32);CHECK(rejected());
    fixture();denied=0x1020;CHECK(rejected());
    fixture();bytes[64]='R';sum(64,52,9);CHECK(rejected());
    fixture();put(68,51,4);sum(64,51,9);CHECK(rejected());
    fixture();put(100,0xfffffffffffffff0ULL,8);sum(64,52,9);CHECK(rejected());
    fixture();put(108,0x1100,8);sum(64,52,9);CHECK(rejected());
    fixture();bytes[265]++;CHECK(rejected());
    fixture();put(260,1024*1024+1,4);CHECK(rejected());
    fixture();bytes[301]=0;sum(256,64,9);CHECK(rejected());
    fixture();bytes[309]=13;sum(256,64,9);CHECK(rejected());
    fixture();put(516,59,4);sum(512,59,9);CHECK(rejected());
    fixture();bytes[556]=1;sum(512,60,9);CHECK(rejected());
    fixture();bytes[566]=200;bytes[567]=100;sum(512,60,9);CHECK(rejected());
    fixture();bytes[548]=1;sum(512,60,9);CHECK(rejected());
    fixture();denied=0x123b;CHECK(rejected());
    /* The physical reader returns a pointer only after checking the map. */
    U64 descriptors[15]={9,0x1000,0,1,0,10,0x2000,0,1,0,7,0x3000,0,1,0};
    BootInfo boot={0};boot.memory_map=(U64)descriptors;boot.memory_map_size=sizeof(descriptors);boot.descriptor_size=40;boot.descriptor_version=1;
    CHECK(acpi_memory_read(&boot,0x1ff0,32)==(const U8 *)0x1ff0);
    CHECK(!acpi_memory_read(&boot,0x2ff0,32));
    CHECK(!acpi_memory_read(&boot,0xfffffffffffffff0ULL,32));
    CHECK(!acpi_memory_read(&boot,64ULL*1024*1024*1024,1));
    CHECK(!acpi_memory_read(&boot,0x1000,0));
    descriptors[8]=~0ULL;CHECK(!acpi_memory_read(&boot,0x1ff0,32));
    boot.descriptor_version=2;CHECK(!acpi_memory_read(&boot,0x1000,20));
    return 0;
}

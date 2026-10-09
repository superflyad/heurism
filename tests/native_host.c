#include "../boot/kernel_handoff.h"
#include "../kernel/pages.h"
#define CHECK(x) do { if(!(x)) return __LINE__; } while(0)
static U32 maps,exits,mode,partial;
static U64 expected_key;
static EFI_STATUS EFIAPI mock_map(UINTN *size,void *buffer,UINTN *key,UINTN *stride,U32 *version) {
    (void)buffer; ++maps;
    if(mode==4 || (mode==5 && partial)) { *size=999999; return EFI_BUFFER_TOO_SMALL; }
    *size=144; *stride=48; *version=1; expected_key=maps*10; *key=expected_key;
    if(mode==6) *stride=39;
    if(mode==7) *stride=264;
    if(mode==8) *size=143;
    return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI mock_exit(EFI_HANDLE image,UINTN key) {
    (void)image; ++exits; partial=1;
    if(mode==3) return EFI_UNSUPPORTED;
    if(mode==2 || mode==5 || key!=expected_key) return EFI_INVALID_PARAMETER;
    return EFI_SUCCESS;
}
int run_handoff_tests(void) {
    EFI_BOOT_SERVICES bs={0}; BootInfo b={0}; U32 attempts; EFI_STATUS s;
    bs.get_memory_map=(void *)mock_map; bs.exit_boot_services=(void *)mock_exit; b.memory_map=0x1000;
    for(mode=0;mode<9;++mode) {
        maps=exits=partial=0;
        s=kernel_exit_boot_services(&bs,(void *)1,&b,1024,&attempts,mode==1);
        switch(mode) {
        case 0: CHECK(s==0 && maps==1 && exits==1 && attempts==1); break;
        case 1: CHECK(s==0 && maps==2 && exits==2 && attempts==2); break;
        case 2: CHECK(s==EFI_INVALID_PARAMETER && maps==3 && exits==3 && attempts==3); break;
        case 3: CHECK(s==EFI_UNSUPPORTED && maps==1 && exits==1); break;
        case 4: CHECK(s==EFI_BUFFER_TOO_SMALL && maps==1 && exits==0); break;
        case 5: CHECK(s==EFI_BUFFER_TOO_SMALL && maps==2 && exits==1); break;
        default: CHECK(s==EFI_LOAD_ERROR && maps==1 && exits==0); break;
        }
    }
    return 0;
}
int run_page_tests(void) {
    EfiMemoryDescriptor descriptors[4]={{7,0,0x100000,0,8,0},{7,0,0x100000,0,8,0},{6,0,0x200000,0,8,0},{7,0,0x300000,0,8,1ULL<<63}};
    BootInfo b={0}; U64 pages[8]; b.memory_map=(U64)descriptors;b.memory_map_size=sizeof(descriptors);b.descriptor_size=40;b.descriptor_version=1;
    b.reserved_count=1;b.reserved[0]=(MemoryRange){0x102fff,2};
    CHECK(pages_init(&b) && pages_available()==6);
    CHECK(!page_free(0x100000) && !page_free(0x100001) && !page_free(0));
    for(U32 i=0;i<6;++i) { pages[i]=page_alloc(); CHECK(pages[i]>=0x100000 && pages[i]<0x108000 && pages[i]!=0x102000 && pages[i]!=0x103000); for(U32 j=0;j<i;++j) CHECK(pages[i]!=pages[j]); }
    CHECK(!page_alloc() && pages_available()==0);
    for(U32 i=0;i<6;++i) CHECK(page_free(pages[i]) && !page_free(pages[i]));
    CHECK(pages_available()==6);
    descriptors[0].physical=0xfffffffffffff000;descriptors[0].pages=2;
    CHECK(!pages_init(&b));
    b.descriptor_size=39;CHECK(!pages_init(&b));
    b.descriptor_size=40;b.reserved_count=COMPANION_MAX_RESERVED+1;CHECK(!pages_init(&b));
    return 0;
}

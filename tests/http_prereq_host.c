#ifdef TEST_HTTP_CLEANUP_PROBE
#include "../boot/http_cleanup_probe.c"
#else
#include "../boot/http_prereq_probe.c"
#endif
static U8 path_fixture[41],image_fixture[4096];
static U32 reads,mode;
static EFI_HANDLE fixture_handles[1]={(EFI_HANDLE)1};
static PrereqLoaded fixture_loaded;
static U32 absent_dhcp,invalid_policy,limit_handles;
static EFI_STATUS EFIAPI fixture_locate(U32 type,EFI_GUID *g,void *key,UINTN *count,EFI_HANDLE **out) {
 if(type!=2 || key)return EFI_UNSUPPORTED;
 if(equal(g,&fv_guid))return 0x800000000000000eULL;
 *count=limit_handles?513:1;*out=fixture_handles;return 0;
}
static EFI_STATUS EFIAPI fixture_handle(EFI_HANDLE handle,EFI_GUID *g,void **out) {
 (void)handle;
 if(equal(g,&loaded_guid))*out=&fixture_loaded;
 else if(equal(g,&path_guid))*out=path_fixture;
 else if(equal(g,&dhcp4_binding) || equal(g,&dhcp6_binding)) {
  if(absent_dhcp)return 0x800000000000000eULL;*out=fixture_handles;
 } else return EFI_UNSUPPORTED;
 return 0;
}
static EFI_STATUS EFIAPI fixture_policy(EFI_GUID *g,void *key,void **out) {
 if(!equal(g,&policy_interface) || key)return EFI_UNSUPPORTED;
 *out=invalid_policy?(void *)1:image_fixture+0xc38;return 0;
}
static EFI_STATUS EFIAPI fixture_free(void *p) {return p==fixture_handles?0:EFI_UNSUPPORTED;}
static U8 contains(const char *needle) {
 for(UINTN i=0;i<used;i++) {UINTN j=0;while(needle[j] && i+j<used && output[i+j]==needle[j])j++;if(!needle[j])return 1;}
 return 0;
}
static EFI_STATUS EFIAPI read_fixture(Fv *fv,EFI_GUID *name,U8 type,UINTN instance,void **out,UINTN *size,U32 *auth) {
 (void)fv;(void)name;(void)type;
 if(instance || *out!=section_buffer || *size!=sizeof(section_buffer))return EFI_UNSUPPORTED;
 reads++;*auth=0;*size=sizeof(depex_pin);
 for(UINTN i=0;i<*size;i++)section_buffer[i]=depex_pin[i];
 if(mode==1)section_buffer[0]^=1;
 if(mode==2)*size=65537;
 if(mode==3)*out=0;
 return mode==4?4:0;
}
int prereq_run_tests(void) {
 Fv fv={0};fv.read_section=(void *)read_fixture;
 for(mode=0;mode<5;mode++) {
  used=0;read_pinned_section(&fv,&http_file,0x13,depex_pin,sizeof(depex_pin),"TEST");
  if(section_matches(mode==4?4:0,mode==3?0:section_buffer,mode==2?65537:sizeof(depex_pin),depex_pin,sizeof(depex_pin))!=(mode==0))return 1;
 }
 if(reads!=5)return 2;
 path_fixture[0]=3;path_fixture[1]=11;path_fixture[2]=37;
 const U8 mac[6]={0x7c,0xc2,0xc6,0x1d,0xb2,0xf5};
 for(U32 i=0;i<6;i++)path_fixture[4+i]=mac[i];
 path_fixture[37]=0x7f;path_fixture[38]=0xff;path_fixture[39]=4;
 if(!path_matches(path_fixture,0,1))return 3;
 path_fixture[4]^=1;if(path_matches(path_fixture,0,1))return 4;
 path_fixture[2]=0;if(path_matches(path_fixture,0,1))return 5;
 path_fixture[0]=4;path_fixture[1]=6;path_fixture[2]=20;
 for(U32 i=0;i<16;i++)path_fixture[4+i]=((U8 *)&provider_file)[i];
 path_fixture[20]=0x7f;path_fixture[21]=0xff;path_fixture[22]=4;path_fixture[23]=0;
 if(!path_matches(path_fixture,&provider_file,0))return 6;
 path_fixture[4]^=1;if(path_matches(path_fixture,&provider_file,0))return 7;
 PrereqLoaded image={0};image.base=image_fixture;image.size=sizeof(image_fixture);
 if(!range_contains(&image,(U64)image_fixture+4088,8))return 8;
 if(range_contains(&image,(U64)image_fixture+4089,8) || range_contains(&image,(U64)image_fixture-1,8))return 9;
 image.size=0x1000001;if(range_contains(&image,(U64)image_fixture,8))return 10;
 /* Full inventory with same-controller bindings and an image-owned interface.
    All execution/attachment callbacks are NULL: the inventory must never use them. */
 static EFI_BOOT_SERVICES bs;static EFI_SYSTEM_TABLE st;st.boot_services=&bs;
 bs.locate_handle_buffer=(void *)fixture_locate;bs.handle_protocol=fixture_handle;
 bs.locate_protocol=fixture_policy;bs.free_pool=(void *)fixture_free;
 fixture_loaded.base=image_fixture;fixture_loaded.size=sizeof(image_fixture);
 fixture_loaded.path=path_fixture;
 for(U32 i=0;i<16;i++)path_fixture[4+i]=((U8 *)&provider_file)[i];
 U64 method=(U64)image_fixture+0x894;
 for(U32 i=0;i<8;i++)image_fixture[0xc38+i]=(U8)(method>>(i*8));
 used=0;inspect_prerequisites(&st,0);
 if(!contains("owns_interface=0x0000000000000001") || !contains("method_rva=0x0000000000000894") || !contains("dhcp4_present=0x0000000000000001"))return 11;
 absent_dhcp=1;invalid_policy=1;used=0;inspect_prerequisites(&st,0);
 if(!contains("dhcp4_present=0x0000000000000000") || !contains("POLICY_INTERFACE_IMAGE_OWNERS count=0x0000000000000000"))return 12;
 limit_handles=1;used=0;inspect_prerequisites(&st,0);
 if(contains("HTTP_CONTROLLER index=") || contains("POLICY_IMAGE index="))return 13;
 return 0;
}

#define TEST_HTTP_ETHERNET_PROBE
#include "http_init_host.c"
static U32 nic_count,wrong_mac,check_calls;
static EFI_STATUS check_status;
static U8 nic_path[41];
static EFI_STATUS EFIAPI nic_locate(U32 type,EFI_GUID *g,void *key,UINTN *count,EFI_HANDLE **out) {
 if(type!=2 || key || !equal(g,&http_binding))return EFI_UNSUPPORTED;
 *out=list;*count=nic_count;list[0]=(EFI_HANDLE)50;list[1]=(EFI_HANDLE)51;return 0;
}
static EFI_STATUS EFIAPI nic_handle(EFI_HANDLE h,EFI_GUID *g,void **out) {
 if(h!=(EFI_HANDLE)50 && h!=(EFI_HANDLE)51)return EFI_UNSUPPORTED;
 if(equal(g,&path_guid))*out=nic_path;
 else if(equal(g,&dhcp4_binding) || equal(g,&dhcp6_binding))*out=provider_image;
 else return EFI_UNSUPPORTED;
 return 0;
}
static EFI_STATUS EFIAPI check_fixture(InitBinding *b,EFI_HANDLE nic,void *path) {
 if(b!=&bindings[0] || nic!=(EFI_HANDLE)50 || path)return EFI_UNSUPPORTED;
 check_calls++;return check_status;
}
int ethernet_run_tests(void) {
 int init=init_run_tests();if(init)return init;
 reset();fixture_bs.locate_handle_buffer=(void *)nic_locate;fixture_bs.handle_protocol=nic_handle;
 nic_path[0]=3;nic_path[1]=11;nic_path[2]=37;
 static const U8 mac[6]={0x7c,0xc2,0xc6,0x1d,0xb2,0xf5};
 for(U32 i=0;i<6;i++)nic_path[4+i]=mac[i];
 nic_path[37]=0x7f;nic_path[38]=0xff;nic_path[39]=4;
 nic_count=1;if(ethernet_target(&fixture_st)!=(EFI_HANDLE)50)return 12;
 nic_count=2;if(ethernet_target(&fixture_st))return 13;
 nic_count=1;wrong_mac=1;nic_path[4]^=(U8)wrong_mac;if(ethernet_target(&fixture_st))return 14;
 bindings[0].supported=(void *)check_fixture;check_status=0;check_calls=0;
 if(supported_check(&bindings[0],(EFI_HANDLE)50)!=0 || check_calls!=1)return 15;
 check_status=0x800000000000000fULL;
 if(supported_check(&bindings[0],(EFI_HANDLE)50)!=check_status || check_calls!=2)return 16;
 static U8 image[sizeof(http_pin)];PrereqLoaded loaded={0};loaded.base=image;loaded.size=sizeof(image);
 InitBinding *b=(InitBinding *)(image+0xc188);
 b->supported=image+0xf54;b->start=image+0xffc;b->stop=image+0x13e0;b->version=10;b->binding=(EFI_HANDLE)52;
 if(!ethernet_binding_matches(&loaded,b,(EFI_HANDLE)52,0))return 17;
 b->version=0x10;if(ethernet_binding_matches(&loaded,b,(EFI_HANDLE)52,0))return 18;
 b->version=10;b->supported=image+0xf55;if(ethernet_binding_matches(&loaded,b,(EFI_HANDLE)52,0))return 19;
 return 0;
}

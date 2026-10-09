#include "../boot/network_probe.c"
static EFI_BOOT_SERVICES mock_bs;
static EFI_SYSTEM_TABLE mock_st;
static Network mock_network;
static NetworkMode mock_mode;
static EFI_HANDLE mock_handles[1]={(EFI_HANDLE)1};
static U8 good_path[]={3,11,4,0,0x7f,0xff,4,0};
static U32 frees,calls,scenario,failed;
static EFI_STATUS EFIAPI locate(U32 type,EFI_GUID *g,void *search,UINTN *count,EFI_HANDLE **handles) {
 if(type!=2 || search || !g)failed=1;calls++;
 if(scenario==1)return EFI_UNSUPPORTED;
 *count=scenario==2?33:1;*handles=mock_handles;return 0;
}
static EFI_STATUS EFIAPI protocol(EFI_HANDLE handle,EFI_GUID *g,void **out) {
 if(handle!=(EFI_HANDLE)1)failed=2;
 if(g==&path_guid){*out=good_path;return 0;}
 if(g==&network_guids[0].guid){*out=&mock_network;return 0;}
 return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI release(void *p){if(p!=mock_handles)failed=3;frees++;return 0;}
int network_run_tests(void) {
 mock_bs.locate_handle_buffer=(void *)locate;mock_bs.handle_protocol=protocol;mock_bs.free_pool=(void *)release;
 mock_st.boot_services=&mock_bs;mock_network.mode=&mock_mode;mock_mode.address_size=6;
 network_inventory(&mock_st,0);if(failed || calls!=7 || frees!=7)return 10;
 scenario=1;network_inventory(&mock_st,0);if(failed || calls!=14 || frees!=7)return 20;
 scenario=2;network_inventory(&mock_st,0);if(failed || calls!=21 || frees!=14)return 30;
 /* Invalid length must stop before walking outside the first node. */
 U8 bad[]={3,11,0,0};used=0;network_path(bad);
 if(used!=17)return 40;
 return 0;
}

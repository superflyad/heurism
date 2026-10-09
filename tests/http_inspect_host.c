#include "../boot/http_inspect_probe.c"
static EFI_BOOT_SERVICES bs;
static EFI_SYSTEM_TABLE st;
static HiiDatabase db;
static InspectLoaded loaded;
static InspectBinding binding;
static EFI_HANDLE handles[1]={(EFI_HANDLE)1};
static U8 dp[24]={4,6,20,0},package[64];
static U32 scenario,fail,exports,frees;
static U8 contains(const char *word) {
 output[used]=0;UINTN length=0;while(word[length])length++;
 for(UINTN i=0;i+length<=used;i++) {
  U8 yes=1;for(UINTN j=0;j<length;j++)if(output[i+j]!=word[j])yes=0;if(yes)return 1;
 }
 return 0;
}
static EFI_STATUS EFIAPI mock_handles(U32 type,EFI_GUID *g,void *search,UINTN *count,EFI_HANDLE **out) {
 if(type!=2 || search || !g)fail=1;*count=1;*out=handles;
 if(scenario==1)return 0x800000000000000eULL;
 if(scenario==2)*count=300;
 return 0;
}
static EFI_STATUS EFIAPI mock_free(void *p){if(p!=handles)fail=2;frees++;return 0;}
static EFI_STATUS EFIAPI mock_handle(EFI_HANDLE h,EFI_GUID *g,void **out) {
 if(h!=(EFI_HANDLE)1 && h!=(EFI_HANDLE)2)fail=3;
 if(equal(g,&path_guid)){*out=dp;return 0;}
 if(equal(g,&binding_guid)){*out=&binding;return 0;}
 if(equal(g,&loaded_guid)){*out=&loaded;return 0;}
 return 0x800000000000000eULL;
}
static EFI_STATUS EFIAPI mock_database(EFI_GUID *g,void *key,void **out) {
 if(!equal(g,&database_guid) || key)fail=4;*out=&db;
 return scenario==1?0x800000000000000eULL:0;
}
static EFI_STATUS EFIAPI mock_list(HiiDatabase *self,U8 type,EFI_GUID *g,UINTN *bytes,EFI_HANDLE *out) {
 if(self!=&db || type || g || *bytes!=sizeof(hii_handles))fail=5;
 *bytes=scenario==2?sizeof(hii_handles)+8:sizeof(EFI_HANDLE);out[0]=(EFI_HANDLE)3;return 0;
}
static EFI_STATUS EFIAPI mock_driver(HiiDatabase *self,EFI_HANDLE h,EFI_HANDLE *out) {
 if(self!=&db || h!=(EFI_HANDLE)3)fail=6;*out=(EFI_HANDLE)1;return 0;
}
static EFI_STATUS EFIAPI mock_export(HiiDatabase *self,EFI_HANDLE h,UINTN *bytes,void *out) {
 if(self!=&db || h!=(EFI_HANDLE)3 || *bytes!=sizeof(hii_package))fail=7;
 exports++;*bytes=scenario==3?sizeof(hii_package)+1:sizeof(package);
 if(scenario==3)return 0x8000000000000009ULL;
 for(UINTN i=0;i<sizeof(package);i++)((U8 *)out)[i]=package[i];return 0;
}
int inspect_run_tests(void) {
 for(UINTN i=0;i<16;i++)dp[i+4]=((U8 *)&http_image_guid)[i];dp[20]=0x7f;dp[21]=0xff;dp[22]=4;
 binding.image=(EFI_HANDLE)2;loaded.path=dp;
 bs.handle_protocol=mock_handle;bs.locate_handle_buffer=(void *)mock_handles;bs.free_pool=(void *)mock_free;bs.locate_protocol=mock_database;
 st.boot_services=&bs;db.list=mock_list;db.export_list=mock_export;db.driver=mock_driver;
 package[16]=64;package[20]=44;package[23]=4;package[24]='H';package[25]='T';package[26]='T';package[27]='P';
 for(scenario=0;scenario<4;scenario++) {
  used=fail=exports=frees=0;inspect_http(&st,0);
  if(fail || !contains("HTTP_INSPECTION_COMPLETE"))return 10+(int)scenario;
  if(scenario==0 && (exports!=1 || frees!=8 || !contains("http_boot_image=0x0000000000000001") || !contains("http_keyword=0x0000000000000001")))return 20;
  if(scenario==1 && (exports || frees))return 21;
  if(scenario==2 && (exports || frees!=8 || !contains("INSPECT_HANDLE_LIMIT")))return 22;
  if(scenario==3 && (exports!=1 || !contains("HII_EXPORT_SKIPPED")))return 23;
 }
 used=0;for(UINTN i=0;i<64;i++)hii_package[i]=0;
 package_metadata(19);if(!contains("HII_BAD_LENGTH"))return 30;
 used=0;hii_package[16]=24;hii_package[20]=0;package_metadata(24);
 if(!contains("HII_PACKAGE_INVALID"))return 31;
 used=0;hii_package[20]=4;hii_package[23]=2;package_metadata(24);
 if(contains("INVALID"))return 32;
 used=0;hii_package[16]=28;hii_package[20]=8;hii_package[24]=0x0e;hii_package[25]=0;package_metadata(28);
 if(!contains("HII_IFR_INVALID"))return 33;
 used=0;hii_package[16]=24;hii_package[20]=4;package_metadata(sizeof(hii_package));
 if(contains("HII_BAD_LENGTH"))return 35;
 used=0;U8 bad[]={4,6,0,0};inspect_path(bad);if(!contains("PATH_INVALID"))return 34;
 return 0;
}

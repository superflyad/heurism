#include "../boot/nv_startup.c"
static StartupRuntime rt;
static EFI_BOOT_SERVICES bs;
static EFI_SYSTEM_TABLE st;
static CompanionExtension child_service;
static StartupRecord observed;
static U32 scenario,loads,starts,installs,unloads,markers,fail,new_reads,legacy_reads;
static EFI_STATUS EFIAPI mock_get(U16 *name,EFI_GUID *guid,U32 *attrs,UINTN *bytes,void *out) {
 if(guid!=&startup_nv_guid || *bytes!=sizeof(expected_payload))fail=1;
 if(name==startup_nv_name) {
  new_reads++;
  if(scenario<13 || scenario==17)return 0x800000000000000eULL;
  *attrs=scenario==15?6:7;*bytes=scenario==16?2047:2048;
 } else if(name==startup_legacy_nv_name) {
  legacy_reads++;
  if(scenario==1 || scenario==17)return 0x800000000000000eULL;
  *attrs=scenario==3?6:7;*bytes=scenario==4?2047:2048;
 } else {fail=1;return EFI_UNSUPPORTED;}
 for(UINTN i=0;i<sizeof(expected_payload);i++)((U8 *)out)[i]=expected_payload[i];
 if(scenario==2 || scenario==18 || (name==startup_nv_name && scenario==14))((U8 *)out)[10]^=1;
 return 0;
}
static EFI_STATUS EFIAPI mock_set(U16 *name,EFI_GUID *guid,U32 attrs,UINTN bytes,void *data) {
 if(name!=startup_status_name || guid!=&startup_nv_guid || attrs!=6 || bytes!=sizeof(observed))fail=2;
 observed=*(StartupRecord *)data;markers++;
 return scenario==10?EFI_UNSUPPORTED:0;
}
static EFI_STATUS EFIAPI mock_load(U8 policy,EFI_HANDLE parent,void *path,void *source,UINTN bytes,EFI_HANDLE *child) {
 if(policy || parent!=(EFI_HANDLE)1 || path || source!=startup_payload || source==(void *)expected_payload || bytes!=2048)fail=3;
 for(UINTN i=0;i<bytes;i++)if(((U8 *)source)[i]!=expected_payload[i])fail=4;
 loads++;*child=scenario==12?0:(EFI_HANDLE)7;
 return scenario==5?EFI_UNSUPPORTED:0;
}
static EFI_STATUS EFIAPI mock_start(EFI_HANDLE child,UINTN *bytes,U16 **data) {
 if(child!=(EFI_HANDLE)7 || bytes || data)fail=5;starts++;return scenario==6?EFI_UNSUPPORTED:0;
}
static EFI_STATUS EFIAPI mock_info(CompanionExtension *self,UINTN *bytes,CompanionExtensionInfo *out) {
 if(self!=&child_service || *bytes!=sizeof(*out))fail=6;
 out->magic=scenario==8?0:COMPANION_EXTENSION_MAGIC;out->revision=1;out->capabilities=1;
 if(scenario==11)*bytes=31;return 0;
}
static EFI_STATUS EFIAPI mock_protocol(EFI_HANDLE child,EFI_GUID *guid,void **out) {
 if(child!=(EFI_HANDLE)7 || guid!=&extension_guid)fail=7;
 *out=&child_service;return scenario==7?EFI_UNSUPPORTED:0;
}
static EFI_STATUS EFIAPI mock_install(EFI_HANDLE *handle,EFI_GUID *guid,U32 type,void *svc) {
 if(*handle!=(EFI_HANDLE)1 || guid!=&extension_guid || type || svc!=&service)fail=8;
 installs++;return 0;
}
static EFI_STATUS EFIAPI mock_unload(EFI_HANDLE child) {
 if(child!=(EFI_HANDLE)7)fail=9;unloads++;return EFI_UNSUPPORTED;
}
int nv_startup_run_tests(void) {
 rt.get_variable=mock_get;rt.set_variable=mock_set;
 bs.load_image=(void *)mock_load;bs.start_image=(void *)mock_start;bs.unload_image=(void *)mock_unload;
 bs.handle_protocol=mock_protocol;bs.install_protocol_interface=(void *)mock_install;
 st.boot_services=&bs;st.runtime_services=&rt;child_service.revision=1;child_service.get_info=mock_info;
 for(scenario=0;scenario<=18;scenario++) {
  loads=starts=installs=unloads=markers=fail=new_reads=legacy_reads=0;
  rt.header.header_size=scenario==9?0:sizeof(rt);
  EFI_STATUS result=efi_main((EFI_HANDLE)1,&st);
  U32 fallback=scenario!=0 && scenario!=10 && scenario!=13 && scenario!=14 && scenario!=15 && scenario!=16;
  if(result || fail || installs!=fallback)return 100+(int)scenario;
  U64 path=fallback?2:(scenario==13?3:1);
  if(scenario!=9 && (markers!=1 || observed.magic!=STARTUP_MAGIC || observed.version!=1 || observed.path!=path))return 200+(int)scenario;
  if(scenario!=9 && (new_reads!=1 || legacy_reads!=(scenario==13?0:1)))return 900+(int)scenario;
  if(scenario>=1 && scenario<=4 && (loads || starts))return 300+(int)scenario;
  if((scenario==17 || scenario==18) && (loads || starts))return 300+(int)scenario;
  if(scenario==9 && (loads || starts || markers))return 409;
  if((scenario==5 || scenario==12) && (loads!=1 || starts))return 500+(int)scenario;
  if((scenario==6 || scenario==7 || scenario==8 || scenario==11) && (starts!=1 || unloads!=1))return 600+(int)scenario;
  if((scenario==0 || scenario==10) && (loads!=1 || starts!=1 || unloads || observed.info_status))return 700+(int)scenario;
  if((scenario>=13 && scenario<=16) && (loads!=1 || starts!=1 || unloads || observed.info_status))return 700+(int)scenario;
 }
 if(efi_main(0,&st)!=COMPANION_INVALID_PARAMETER || efi_main((EFI_HANDLE)1,0)!=COMPANION_INVALID_PARAMETER)return 800;
 return 0;
}

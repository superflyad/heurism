#include "../boot/nv_extension_probe.c"
static NvRuntime mock_rt;
static EFI_BOOT_SERVICES mock_bs;
static EFI_SYSTEM_TABLE mock_st;
static CompanionExtension mock_service;
static U8 stored[sizeof(expected_payload)];
static U32 present,writes,loads,starts,fail,low_space,bad_payload,bad_attr;
static EFI_STATUS write_status,load_status,start_status;
static EFI_STATUS EFIAPI capacity(U32 attrs,U64 *maximum,U64 *remaining,U64 *single) {
 if(attrs!=7)fail=1;*maximum=0x40000;*remaining=low_space?0x1000:0x20000;*single=0x10000;return 0;
}
static EFI_STATUS EFIAPI get_variable(U16 *name,EFI_GUID *guid,U32 *attrs,UINTN *bytes,void *out) {
 if(name!=nv_name || guid!=&nv_guid || *bytes!=sizeof(stored))fail=2;
 if(!present)return 0x800000000000000eULL;
 *attrs=bad_attr?6:7;*bytes=sizeof(stored);
 for(UINTN i=0;i<sizeof(stored);i++)((U8 *)out)[i]=stored[i];
 if(bad_payload)((U8 *)out)[0]^=1;return 0;
}
static EFI_STATUS EFIAPI set_variable(U16 *name,EFI_GUID *guid,U32 attrs,UINTN bytes,void *data) {
 if(name!=nv_name || guid!=&nv_guid || attrs!=7 || bytes!=sizeof(stored) || data!=(void *)expected_payload || present)fail=3;
 writes++;if(EFI_ERROR(write_status))return write_status;
 for(UINTN i=0;i<bytes;i++)stored[i]=((U8 *)data)[i];present=1;return 0;
}
static EFI_STATUS EFIAPI load(U8 policy,EFI_HANDLE parent,void *path,void *buffer,UINTN bytes,EFI_HANDLE *child) {
 if(policy || parent!=(EFI_HANDLE)1 || path || buffer!=retrieved_payload || buffer==(void *)expected_payload || bytes!=sizeof(stored))fail=4;
 loads++;*child=(EFI_HANDLE)7;return load_status;
}
static EFI_STATUS EFIAPI start(EFI_HANDLE child,UINTN *size,U16 **data) {
 if(child!=(EFI_HANDLE)7 || size || data)fail=5;starts++;return start_status;
}
static EFI_STATUS EFIAPI child_protocol(EFI_HANDLE child,EFI_GUID *guid,void **out) {
 if(child!=(EFI_HANDLE)7 || guid!=&nv_extension_guid)fail=6;*out=&mock_service;return 0;
}
static EFI_STATUS EFIAPI child_info(CompanionExtension *service,UINTN *bytes,CompanionExtensionInfo *out) {
 if(service!=&mock_service || *bytes!=sizeof(*out))fail=7;
 out->magic=COMPANION_EXTENSION_MAGIC;out->revision=1;out->capabilities=1;return 0;
}
int nv_run_tests(void) {
 mock_rt.header.header_size=sizeof(mock_rt);mock_rt.query_variable_info=capacity;
 mock_rt.get_variable=get_variable;mock_rt.set_variable=set_variable;
 mock_bs.load_image=(void *)load;mock_bs.start_image=(void *)start;mock_bs.handle_protocol=child_protocol;
 mock_st.boot_services=&mock_bs;mock_st.runtime_services=&mock_rt;
 mock_service.revision=1;mock_service.get_info=child_info;
 used=0;nv_inventory((EFI_HANDLE)1,&mock_st,0);
 if(fail || writes!=1 || loads!=1 || starts!=1)return 10+(int)fail;
 /* Already persisted image executes without a second write. */
 used=0;nv_inventory((EFI_HANDLE)1,&mock_st,0);
 if(fail || writes!=1 || loads!=2 || starts!=2)return 20+(int)fail;
 bad_payload=1;used=0;nv_inventory((EFI_HANDLE)1,&mock_st,0);
 if(fail || writes!=1 || loads!=2)return 30+(int)fail;
 bad_payload=0;bad_attr=1;used=0;nv_inventory((EFI_HANDLE)1,&mock_st,0);
 if(fail || writes!=1 || loads!=2)return 40+(int)fail;
 bad_attr=0;present=0;low_space=1;used=0;nv_inventory((EFI_HANDLE)1,&mock_st,0);
 if(fail || writes!=1 || loads!=2)return 50+(int)fail;
 low_space=0;write_status=EFI_UNSUPPORTED;used=0;nv_inventory((EFI_HANDLE)1,&mock_st,0);
 if(fail || writes!=2 || loads!=2)return 60+(int)fail;
 write_status=0;load_status=EFI_UNSUPPORTED;used=0;nv_inventory((EFI_HANDLE)1,&mock_st,0);
 if(fail || writes!=3 || loads!=3 || starts!=2)return 70+(int)fail;
 load_status=0;start_status=EFI_UNSUPPORTED;used=0;nv_inventory((EFI_HANDLE)1,&mock_st,0);
 if(fail || writes!=3 || loads!=4 || starts!=3)return 80+(int)fail;
 return 0;
}

#include "../boot/extension.c"
static EFI_BOOT_SERVICES bs;
static EFI_SYSTEM_TABLE st;
static EFI_STATUS install_result;
static U32 calls,fail;
static EFI_STATUS EFIAPI install(EFI_HANDLE *handle,EFI_GUID *guid,U32 type,void *interface) {
 if(*handle!=(EFI_HANDLE)1 || guid!=&extension_guid || type || interface!=&service)fail=1;
 calls++;return install_result;
}
int extension_run_tests(void) {
 st.boot_services=&bs;st.firmware_revision=0x12300;bs.header.revision=0x20070;
 bs.install_protocol_interface=(void *)install;
 if(efi_main(0,&st)!=COMPANION_INVALID_PARAMETER || calls)return 1;
 if(efi_main((EFI_HANDLE)1,0)!=COMPANION_INVALID_PARAMETER || calls)return 2;
 if(efi_main((EFI_HANDLE)1,&st)!=0 || calls!=1 || fail)return 3;
 CompanionExtensionInfo out={0};UINTN bytes=0;
 if(service.get_info(&service,&bytes,&out)!=COMPANION_BUFFER_TOO_SMALL || bytes!=sizeof(out) || out.magic)return 4;
 if(service.get_info(0,&bytes,&out)!=COMPANION_INVALID_PARAMETER)return 5;
 if(service.get_info(&service,0,&out)!=COMPANION_INVALID_PARAMETER)return 6;
 if(service.get_info(&service,&bytes,0)!=COMPANION_INVALID_PARAMETER)return 7;
 if(service.get_info(&service,&bytes,&out)!=0 || out.magic!=COMPANION_EXTENSION_MAGIC ||
    out.revision!=1 || out.firmware_revision!=0x12300 || out.boot_services_revision!=0x20070 || out.capabilities!=1)return 8;
 install_result=EFI_UNSUPPORTED;
 if(efi_main((EFI_HANDLE)1,&st)!=EFI_UNSUPPORTED || calls!=2 || fail)return 9;
 return 0;
}

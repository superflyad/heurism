/* An additive UEFI boot-service driver. Publishes one read-only owner protocol.
 * No flash access, firmware patching, network, disk or variable writes.
 */
#include "extension.h"
typedef EFI_STATUS (EFIAPI *InstallProtocol)(EFI_HANDLE *,EFI_GUID *,U32,void *);
static EFI_GUID extension_guid=COMPANION_EXTENSION_GUID;
static CompanionExtensionInfo info;
static CompanionExtension service;
static EFI_STATUS EFIAPI get_info(CompanionExtension *self,UINTN *bytes,CompanionExtensionInfo *out) {
 if(self!=&service || !bytes)return COMPANION_INVALID_PARAMETER;
 if(*bytes<sizeof(info)) { *bytes=sizeof(info);return COMPANION_BUFFER_TOO_SMALL; }
 if(!out)return COMPANION_INVALID_PARAMETER;
 *bytes=sizeof(info);
 const U8 *from=(const U8 *)&info;U8 *to=(U8 *)out;
 for(UINTN i=0;i<sizeof(info);i++)to[i]=from[i];
 return EFI_SUCCESS;
}
/* This function pointer produces the relocation needed by the firmware loader. */
static CompanionExtension service={1,get_info};
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *st) {
 if(!image || !st || !st->boot_services || !st->boot_services->install_protocol_interface)
  return COMPANION_INVALID_PARAMETER;
 info.magic=COMPANION_EXTENSION_MAGIC;info.revision=1;
 info.firmware_revision=st->firmware_revision;
 info.boot_services_revision=st->boot_services->header.revision;
 info.reserved=0;info.capabilities=1; /* Read-only information service. */
 EFI_HANDLE handle=image;
 return ((InstallProtocol)st->boot_services->install_protocol_interface)(&handle,&extension_guid,0,&service);
}
_Static_assert(__builtin_offsetof(EFI_BOOT_SERVICES,install_protocol_interface)==128,"install protocol ABI");

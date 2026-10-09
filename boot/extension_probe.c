/* Observe an already-loaded Companion driver. Does not load/start the driver. */
#include "extension.h"
typedef struct File File;
static void extension_inventory(EFI_SYSTEM_TABLE *,File *);
#define PROBE_EXTENSION extension_inventory
#define PROBE_TAG "COMPANION_EXTENSION_PROBE_01"
#define PROBE_REPORT_PATH u"\\EFI\\companion\\extension-probe-01.txt"
#define PROBE_GRUB_PATH u"\\EFI\\companion\\recoveryx64.efi"
#include "firmware_probe.c"
static EFI_GUID extension_guid=COMPANION_EXTENSION_GUID;
static void extension_inventory(EFI_SYSTEM_TABLE *st,File *file) {
 CompanionExtension *service=0;
 EFI_STATUS result=st->boot_services->locate_protocol(&extension_guid,0,(void **)&service);
 number("EXTENSION_LOCATE status=",result);text("\n");save(file);
 if(EFI_ERROR(result) || !service || service->revision!=1 || !service->get_info)return;
 CompanionExtensionInfo info={0};UINTN bytes=sizeof(info);
 result=service->get_info(service,&bytes,&info);
 number("EXTENSION_INFO status=",result);number(" bytes=",bytes);
 if(!EFI_ERROR(result) && bytes==sizeof(info)) {
  number(" magic=",info.magic);number(" revision=",info.revision);
  number(" firmware_revision=",info.firmware_revision);
  number(" boot_services_revision=",info.boot_services_revision);
  number(" capabilities=",info.capabilities);
 }
 text("\n");save(file);
}

/* Bounded experiment: store one exact 2 KiB owner image as a nonvolatile EFI
 * variable, retrieve it, then load the retrieved buffer as a boot-service driver.
 * This app is still the SSD-resident bootstrap. No BIOS volume is modified.
 */
#include "extension.h"
#include "../build/nv-extension/payload.h"
typedef struct File File;
static void nv_inventory(EFI_HANDLE,EFI_SYSTEM_TABLE *,File *);
#define PROBE_EXTENSION(st,file) nv_inventory(image,st,file)
#define PROBE_TAG "COMPANION_NV_EXTENSION_PROBE_01"
#define PROBE_DESCRIPTION "Owner NVRAM payload test; ESP bootstrap and report.\n"
#define PROBE_REPORT_PATH u"\\EFI\\companion\\nv-probe-01.txt"
#define PROBE_GRUB_PATH u"\\EFI\\companion\\recoveryx64.efi"
#include "firmware_probe.c"
typedef struct {
 EFI_TABLE_HEADER header;
 void *get_time,*set_time,*get_wake,*set_wake,*set_virtual,*convert;
 EFI_STATUS (EFIAPI *get_variable)(U16 *,EFI_GUID *,U32 *,UINTN *,void *);
 void *get_next;
 EFI_STATUS (EFIAPI *set_variable)(U16 *,EFI_GUID *,U32,UINTN,void *);
 void *get_monotonic,*reset,*update_capsule,*query_capsule;
 EFI_STATUS (EFIAPI *query_variable_info)(U32,U64 *,U64 *,U64 *);
} NvRuntime;
static EFI_GUID nv_guid={0x1d8ce97b,0x55e6,0x4b2e,{0x92,0x76,0xbb,0x1e,0x9b,0x66,0x15,0xa1}};
static EFI_GUID nv_extension_guid=COMPANION_EXTENSION_GUID;
static U16 nv_name[]=u"CompanionExtensionImage01";
static U8 retrieved_payload[sizeof(expected_payload)];
static U8 payload_matches(UINTN bytes,U32 attrs) {
 if(bytes!=sizeof(expected_payload) || attrs!=7)return 0;
 for(UINTN i=0;i<bytes;i++)if(retrieved_payload[i]!=expected_payload[i])return 0;
 return 1;
}
static void nv_inventory(EFI_HANDLE parent,EFI_SYSTEM_TABLE *st,File *file) {
 NvRuntime *rt=(NvRuntime *)st->runtime_services;EFI_BOOT_SERVICES *bs=st->boot_services;
 if(!rt || rt->header.header_size<sizeof(NvRuntime) || !rt->get_variable || !rt->set_variable || !rt->query_variable_info) {
  text("NV_RUNTIME_UNAVAILABLE\n");save(file);return;
 }
 U64 maximum=0,remaining=0,per_variable=0;
 EFI_STATUS result=rt->query_variable_info(7,&maximum,&remaining,&per_variable);
 number("NV_CAPACITY status=",result);number(" maximum=",maximum);
 number(" remaining=",remaining);number(" per_variable=",per_variable);text("\n");save(file);
 if(EFI_ERROR(result))return;
 U32 attrs=0;UINTN bytes=sizeof(retrieved_payload);
 result=rt->get_variable(nv_name,&nv_guid,&attrs,&bytes,retrieved_payload);
 number("NV_INITIAL_READ status=",result);number(" bytes=",bytes);number(" attrs=",attrs);text("\n");save(file);
 if(result==0x800000000000000eULL) {
  if(remaining<0x10000+sizeof(expected_payload) || per_variable<sizeof(expected_payload)+64) {
   text("NV_CAPACITY_GUARD\n");save(file);return;
  }
  /* Create only when absent. Never overwrite an existing differing payload. */
  result=rt->set_variable(nv_name,&nv_guid,7,sizeof(expected_payload),(void *)expected_payload);
  number("NV_CREATE status=",result);number(" bytes=",sizeof(expected_payload));text("\n");save(file);
  if(EFI_ERROR(result))return;
  bytes=sizeof(retrieved_payload);attrs=0;
  result=rt->get_variable(nv_name,&nv_guid,&attrs,&bytes,retrieved_payload);
  number("NV_READBACK status=",result);number(" bytes=",bytes);number(" attrs=",attrs);text("\n");save(file);
 }
 if(EFI_ERROR(result) || !payload_matches(bytes,attrs)) {
  text("NV_PAYLOAD_REJECTED\n");save(file);return;
 }
 text("NV_PAYLOAD_EXACT_MATCH\n");save(file);
 EFI_HANDLE child=0;
 result=((LoadImage)bs->load_image)(0,parent,0,retrieved_payload,bytes,&child);
 number("NV_LOAD_IMAGE status=",result);text("\n");save(file);
 if(EFI_ERROR(result) || !child)return;
 result=((StartImage)bs->start_image)(child,0,0);
 number("NV_START_IMAGE status=",result);text("\n");save(file);
 if(EFI_ERROR(result))return;
 CompanionExtension *service=0;
 /* Query the new child handle, not the existing SSD driver's protocol. */
 result=bs->handle_protocol(child,&nv_extension_guid,(void **)&service);
 number("NV_CHILD_PROTOCOL status=",result);text("\n");save(file);
 if(EFI_ERROR(result) || !service || service->revision!=1 || !service->get_info)return;
 CompanionExtensionInfo info={0};bytes=sizeof(info);
 result=service->get_info(service,&bytes,&info);
 number("NV_CHILD_INFO status=",result);number(" bytes=",bytes);number(" magic=",info.magic);
 number(" revision=",info.revision);number(" capabilities=",info.capabilities);text("\n");save(file);
}
_Static_assert(__builtin_offsetof(NvRuntime,get_variable)==72,"GetVariable ABI");
_Static_assert(__builtin_offsetof(NvRuntime,set_variable)==88,"SetVariable ABI");
_Static_assert(__builtin_offsetof(NvRuntime,query_variable_info)==128,"QueryVariableInfo ABI");

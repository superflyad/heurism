/* SSD bootstrap for the pinned motherboard-variable extension.
 * Reads owner NVRAM only. Writes one volatile diagnostic variable (attrs 6).
 * On any rejected payload/service, supplies the existing embedded SSD service.
 * No boot configuration, network, flash-volume or watchdog operations.
 */
#define efi_main ssd_fallback_main
#include "extension.c"
#undef efi_main
#include "../build/nv-extension/payload.h"

typedef struct {
 EFI_TABLE_HEADER header;
 void *get_time,*set_time,*get_wake,*set_wake,*set_virtual,*convert;
 EFI_STATUS (EFIAPI *get_variable)(U16 *,EFI_GUID *,U32 *,UINTN *,void *);
 void *get_next;
 EFI_STATUS (EFIAPI *set_variable)(U16 *,EFI_GUID *,U32,UINTN,void *);
} StartupRuntime;
typedef EFI_STATUS (EFIAPI *StartupLoad)(U8,EFI_HANDLE,void *,void *,UINTN,EFI_HANDLE *);
typedef EFI_STATUS (EFIAPI *StartupStart)(EFI_HANDLE,UINTN *,U16 **);
typedef EFI_STATUS (EFIAPI *StartupUnload)(EFI_HANDLE);
static EFI_GUID startup_nv_guid={0x1d8ce97b,0x55e6,0x4b2e,{0x92,0x76,0xbb,0x1e,0x9b,0x66,0x15,0xa1}};
static U16 startup_nv_name[]=u"CompanionExtensionImage01";
static U16 startup_status_name[]=u"CompanionNvStartup01";
static U8 startup_payload[sizeof(expected_payload)];
/* path: 1=verified child loaded from NV, 2=embedded SSD fallback.
 * magic/version make OS readback unambiguous; no pointer values are retained. */
typedef struct {
 U64 magic,version,path,read_status,load_status,start_status,info_status,fallback_status;
} StartupRecord;
#define STARTUP_MAGIC 0x31564e504d4f4343ULL
static EFI_STATUS startup_from_nv(EFI_HANDLE parent,EFI_SYSTEM_TABLE *st,StartupRecord *r) {
 StartupRuntime *rt=(StartupRuntime *)st->runtime_services;EFI_BOOT_SERVICES *bs=st->boot_services;
 if(!rt || rt->header.header_size<sizeof(StartupRuntime) || !rt->get_variable ||
    !bs->load_image || !bs->start_image || !bs->handle_protocol || !bs->unload_image)
  return EFI_UNSUPPORTED;
 UINTN bytes=sizeof(startup_payload);U32 attrs=0;
 r->read_status=rt->get_variable(startup_nv_name,&startup_nv_guid,&attrs,&bytes,startup_payload);
 if(EFI_ERROR(r->read_status))return r->read_status;
 if(bytes!=sizeof(expected_payload) || attrs!=7)return COMPANION_INVALID_PARAMETER;
 for(UINTN i=0;i<bytes;i++)if(startup_payload[i]!=expected_payload[i])return COMPANION_INVALID_PARAMETER;
 EFI_HANDLE child=0;
 r->load_status=((StartupLoad)bs->load_image)(0,parent,0,startup_payload,bytes,&child);
 if(EFI_ERROR(r->load_status) || !child)return EFI_UNSUPPORTED;
 r->start_status=((StartupStart)bs->start_image)(child,0,0);
 if(!EFI_ERROR(r->start_status)) {
  CompanionExtension *service=0;
  r->info_status=bs->handle_protocol(child,&extension_guid,(void **)&service);
  if(!EFI_ERROR(r->info_status) && service && service->revision==1 && service->get_info) {
   CompanionExtensionInfo child_info={0};bytes=sizeof(child_info);
   r->info_status=service->get_info(service,&bytes,&child_info);
   if(!EFI_ERROR(r->info_status) && bytes==sizeof(child_info) &&
      child_info.magic==COMPANION_EXTENSION_MAGIC && child_info.revision==1 && child_info.capabilities==1)
    return EFI_SUCCESS;
  }
 }
 /* The pinned payload has no unload callback; failed StartImage normally
  * unloads it already. An UnloadImage error does not block management. */
 ((StartupUnload)bs->unload_image)(child);
 return EFI_UNSUPPORTED;
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *st) {
 if(!image || !st || !st->boot_services)return COMPANION_INVALID_PARAMETER;
 StartupRecord r={STARTUP_MAGIC,1,0,EFI_UNSUPPORTED,EFI_UNSUPPORTED,EFI_UNSUPPORTED,EFI_UNSUPPORTED,EFI_UNSUPPORTED};
 EFI_STATUS result=startup_from_nv(image,st,&r);
 if(!EFI_ERROR(result))r.path=1;
 else {r.path=2;r.fallback_status=ssd_fallback_main(image,st);result=r.fallback_status;}
 StartupRuntime *rt=(StartupRuntime *)st->runtime_services;
 if(rt && rt->header.header_size>=sizeof(StartupRuntime) && rt->set_variable)
  /* BOOTSERVICE_ACCESS|RUNTIME_ACCESS only: deliberately not NON_VOLATILE. */
  rt->set_variable(startup_status_name,&startup_nv_guid,6,sizeof(r),&r);
 return result;
}
_Static_assert(sizeof(StartupRecord)==64,"startup marker ABI");
_Static_assert(__builtin_offsetof(StartupRuntime,get_variable)==72,"runtime get ABI");
_Static_assert(__builtin_offsetof(StartupRuntime,set_variable)==88,"runtime set ABI");

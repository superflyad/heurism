/* VM-only fixtures: never deploy this app to the physical target. */
#include "../boot/extension.h"
#include "../build/nv-extension/payload.h"
#include "../build/nv-startup/driver.h"
typedef struct {
 EFI_TABLE_HEADER header;void *reserved[6];
 EFI_STATUS (EFIAPI *get)(U16 *,EFI_GUID *,U32 *,UINTN *,void *);
 void *next;
 EFI_STATUS (EFIAPI *set)(U16 *,EFI_GUID *,U32,UINTN,void *);
} VmRuntime;
typedef EFI_STATUS (EFIAPI *VmLoad)(U8,EFI_HANDLE,void *,void *,UINTN,EFI_HANDLE *);
typedef EFI_STATUS (EFIAPI *VmStart)(EFI_HANDLE,UINTN *,U16 **);
static EFI_GUID nv={0x1d8ce97b,0x55e6,0x4b2e,{0x92,0x76,0xbb,0x1e,0x9b,0x66,0x15,0xa1}};
static EFI_GUID protocol=COMPANION_EXTENSION_GUID;
static U16 marker[]=u"CompanionNvStartup01";
#if STARTUP_VM_CASE!=1
static U16 old_name[]=u"CompanionExtensionImage01";
static U16 new_name[]=u"HeurismExtensionImage01";
static U8 fixture[2048];
#endif
static void log(const char *s) {for(UINTN i=0;s[i];i++)__asm__ volatile("outb %0,%1"::"a"((U8)s[i]),"Nd"((U16)0xe9));}
static EFI_STATUS finish(U32 code) {
 log(code==16?"NV_STARTUP_VM_PASS\n":"NV_STARTUP_VM_FAIL\n");
 __asm__ volatile("outl %0,%1"::"a"(code),"Nd"((U16)0xf4));return EFI_UNSUPPORTED;
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *st) {
 VmRuntime *rt=(VmRuntime *)st->runtime_services;EFI_BOOT_SERVICES *bs=st->boot_services;
 log("NV_STARTUP_VM_BEGIN\n");bs->set_watchdog_timer(0,0,0,0);
#if STARTUP_VM_CASE!=1
 for(UINTN i=0;i<sizeof(fixture);i++)fixture[i]=expected_payload[i];
#if STARTUP_VM_CASE==2 || STARTUP_VM_CASE==5
 fixture[10]^=1;
#endif
 if(STARTUP_VM_CASE!=3 && EFI_ERROR(rt->set(old_name,&nv,7,sizeof(fixture),fixture)))return finish(21);
#if STARTUP_VM_CASE==4
 fixture[10]^=1;
#endif
 if(STARTUP_VM_CASE>=3 && EFI_ERROR(rt->set(new_name,&nv,7,sizeof(fixture),fixture)))return finish(31);
#endif
 EFI_HANDLE driver=0;
 if(EFI_ERROR(((VmLoad)bs->load_image)(0,image,0,(void *)vm_driver,sizeof(vm_driver),&driver)))return finish(22);
 if(EFI_ERROR(((VmStart)bs->start_image)(driver,0,0)))return finish(23);
 U64 record[8]={0};UINTN bytes=sizeof(record);U32 attrs=0;
 if(EFI_ERROR(rt->get(marker,&nv,&attrs,&bytes,record)))return finish(24);
 if(bytes!=sizeof(record) || attrs!=6 || record[0]!=0x31564e504d4f4343ULL || record[1]!=1)return finish(25);
 U64 expected_path=(STARTUP_VM_CASE==3)?3:((STARTUP_VM_CASE==0 || STARTUP_VM_CASE==4)?1:2);
 if(record[2]!=expected_path)return finish(26);
 if(expected_path!=2 && (record[3] || record[4] || record[5] || record[6]))return finish(27);
 if(expected_path==2 && record[7])return finish(28);
 CompanionExtension *service=0;CompanionExtensionInfo info={0};bytes=sizeof(info);
 if(EFI_ERROR(bs->locate_protocol(&protocol,0,(void **)&service)) || !service || !service->get_info)return finish(29);
 if(EFI_ERROR(service->get_info(service,&bytes,&info)) || info.magic!=COMPANION_EXTENSION_MAGIC || info.revision!=1 || info.capabilities!=1)return finish(30);
 return finish(16);
}

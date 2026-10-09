/* Inventory the exact protocols consulted by the saved Dell update validator.
 * Only LocateProtocol and the two fields read by its original ed34 routine.
 * No proprietary method, SMI, capsule submission, flash or MSR write.
 */
#include "uefi.h"
typedef struct File File;
static void policy_inventory(EFI_SYSTEM_TABLE *, File *);
#define PROBE_EXTENSION policy_inventory
#define PROBE_TAG "COMPANION_POLICY_PROBE_01"
#define PROBE_REPORT_PATH u"\\EFI\\companion\\policy-probe-01.txt"
#define PROBE_GRUB_PATH u"\\EFI\\companion\\recoveryx64.efi"
#include "firmware_probe.c"
static EFI_GUID policy_guids[]={
 {0xd67be471,0xdf7c,0x4a3a,{0xaf,0x56,0xad,0x9a,0xc8,0xfb,0x7f,0xf8}},
 {0xef48ffe8,0x9e24,0x4eb8,{0x82,0x8d,0x2e,0xc1,0x1a,0x9d,0xf8,0xdd}},
 {0x2c650f84,0x2fa4,0x453a,{0x90,0x6b,0x10,0x08,0x94,0xff,0xad,0x19}},
 {0x57fa1a50,0x4b98,0x4547,{0xb5,0xa6,0xbe,0xdf,0x95,0x11,0xb1,0xab}},
 {0x71db7b7e,0x4165,0x48fa,{0xac,0x9d,0xf9,0xaf,0x4c,0xef,0xc5,0x34}}
};
static void policy_inventory(EFI_SYSTEM_TABLE *st,File *file) {
 for(UINTN i=0;i<sizeof(policy_guids)/sizeof(policy_guids[0]);i++) {
  void *interface=0;
  EFI_STATUS result=st->boot_services->locate_protocol(&policy_guids[i],0,&interface);
  number("UPDATE_POLICY index=",i);text(" guid=");guid((U8 *)&policy_guids[i]);
  number(" status=",result);number(" present=",!EFI_ERROR(result)&&interface!=0);
  if(!EFI_ERROR(result)&&interface) {
   /* These exact offsets are read by DellFlashUpdate2Dxe RVA ed34. */
   if(i==0)number(" signature_policy_byte=",((volatile U8 *)interface)[1]);
   if(i==1)number(" fallback_version=",u32((U8 *)interface+4));
  }
  text("\n");save(file);
 }
}

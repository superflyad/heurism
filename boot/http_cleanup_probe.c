/* Read existing NIC and child protocol open records. Never attach a driver. */
#include "uefi.h"
static void inspect_cleanup(EFI_SYSTEM_TABLE *,void *);
#define PROBE_EXTENSION(st,file) inspect_cleanup(st,file)
#define PROBE_TAG "COMPANION_HTTP_CLEANUP_01"
#define PROBE_REPORT_PATH u"\\EFI\\companion\\http-cleanup-probe-01.txt"
#define PROBE_DESCRIPTION "Read-only NIC cleanup relationships; SSD management handoff.\n"
#include "http_prereq_probe.c"
typedef struct {EFI_HANDLE agent,controller;U32 attributes,open_count;} OpenRecord;
typedef EFI_STATUS (EFIAPI *OpenInformation)(EFI_HANDLE,EFI_GUID *,OpenRecord **,UINTN *);
static EFI_GUID mnp_protocol={0x7a59b29b,0x910b,0x4171,{0x82,0x42,0xa8,0x5a,0x0d,0xf2,0x5b,0x5b}};
static EFI_GUID dhcp_protocol={0x8a219718,0x4ef5,0x4761,{0x91,0xc8,0xc0,0xf0,0x4b,0xda,0x9e,0x56}};
static EFI_GUID snp_protocol={0xa19832b9,0xac25,0x11d3,{0x9a,0x2d,0,0x90,0x27,0x3f,0xc1,0x4d}};
static EFI_HANDLE cleanup_target(EFI_SYSTEM_TABLE *st) {
 EFI_BOOT_SERVICES *bs=st->boot_services;EFI_HANDLE *handles=0,result=0;UINTN count=0;U32 matches=0;
 EFI_STATUS status=((LocateHandles)bs->locate_handle_buffer)(2,&http_binding,0,&count,&handles);
 if(status==0 && handles && count<=128)for(UINTN i=0;i<count;i++) {
  void *path=0,*dhcp=0;
  if(bs->handle_protocol(handles[i],&path_guid,&path)==0 && path_matches(path,0,1) &&
     bs->handle_protocol(handles[i],&dhcp4_binding,&dhcp)==0 && dhcp){matches++;result=handles[i];}
 }
 if(handles)((FreePool)bs->free_pool)(handles);
 number("CLEANUP_TARGET_MATCHES count=",matches);text("\n");return matches==1?result:0;
}
static void open_records(EFI_SYSTEM_TABLE *st,EFI_HANDLE handle,EFI_GUID *g,EFI_HANDLE nic,const char *label) {
 EFI_BOOT_SERVICES *bs=st->boot_services;OpenRecord *records=0;UINTN count=0;
 EFI_STATUS status=((OpenInformation)bs->open_protocol_information)(handle,g,&records,&count);
 text(label);number(" status=",status);number(" count=",count);text("\n");
 if(status==0 && count<=128 && (records || !count))for(UINTN i=0;i<count;i++) {
  number("OPEN_RECORD index=",i);number(" agent=",(U64)records[i].agent);
  number(" controller=",(U64)records[i].controller);number(" target_controller=",records[i].controller==nic);
  number(" attributes=",records[i].attributes);number(" open_count=",records[i].open_count);text("\n");
 }
 else if(status==0)text("OPEN_RECORD_BOUNDS_REJECTED\n");
 if(records)((FreePool)bs->free_pool)(records);
}
static void inspect_cleanup(EFI_SYSTEM_TABLE *st,void *report) {
 inspect_prerequisites(st,report);
 EFI_BOOT_SERVICES *bs=st->boot_services;EFI_HANDLE nic=cleanup_target(st);
 if(!nic || !bs->open_protocol_information) {
  text("CLEANUP_INSPECTION_GUARD_STOP\nHTTP_CLEANUP_INSPECTION_COMPLETE\n");save(report);return;
 }
 text("CLEANUP_TARGET_MAC 7c:c2:c6:1d:b2:f5\n");number("CLEANUP_TARGET handle=",(U64)nic);text("\n");
 open_records(st,nic,&mnp_protocol,nic,"NIC_MNP");
 open_records(st,nic,&dhcp_protocol,nic,"NIC_DHCP4");
 open_records(st,nic,&dhcp4_binding,nic,"NIC_DHCP4_SERVICE");
 open_records(st,nic,&http_binding,nic,"NIC_HTTP_SERVICE");
 open_records(st,nic,&snp_protocol,nic,"NIC_SNP");
 open_records(st,nic,&http_file,nic,"NIC_HTTP_PRIVATE");save(report);
 EFI_GUID *child_types[2]={&mnp_protocol,&dhcp_protocol};
 for(U32 family=0;family<2;family++) {
  EFI_HANDLE *handles=0;UINTN count=0;
  EFI_STATUS status=((LocateHandles)bs->locate_handle_buffer)(2,child_types[family],0,&count,&handles);
  text(family?"DHCP4_PROTOCOL_HANDLES":"MNP_PROTOCOL_HANDLES");number(" status=",status);number(" count=",count);text("\n");
  if(status==0 && handles && count<=128)for(UINTN i=0;i<count;i++) {
   number("CHILD_HANDLE index=",i);number(" handle=",(U64)handles[i]);text("\n");
   open_records(st,handles[i],child_types[family],nic,"CHILD_OPEN_INFORMATION");
  }
  if(handles)((FreePool)bs->free_pool)(handles);save(report);
 }
 text("NO_DRIVER_LOAD_OR_CONTROLLER_START\nHTTP_CLEANUP_INSPECTION_COMPLETE\n");save(report);
}
_Static_assert(sizeof(OpenRecord)==24,"OpenProtocolInformation ABI");

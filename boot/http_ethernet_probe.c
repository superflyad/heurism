/* Execute the pinned driver's Supported checks on one MAC-selected NIC.
 * No controller Start/Stop, ConnectController, LoadFile or network request.
 * The shared initializer removes owned bindings and resumes SSD management. */
#include "uefi.h"
static void check_ethernet(EFI_SYSTEM_TABLE *,EFI_HANDLE,void *);
#define HTTP_CONTROLLER_CHECK(st,child,report) check_ethernet(st,child,report)
#define PROBE_TAG "COMPANION_HTTP_ETHERNET_02"
#define PROBE_REPORT_PATH u"\\EFI\\companion\\http-ethernet-probe-02.txt"
#define PROBE_DESCRIPTION "Targeted Ethernet Supported checks; no controller attachment; SSD management preserved.\n"
#include "http_init_probe.c"
typedef EFI_STATUS (EFIAPI *SupportedCheck)(InitBinding *,EFI_HANDLE,void *);
static EFI_STATUS supported_check(InitBinding *binding,EFI_HANDLE nic) {
 return ((SupportedCheck)binding->supported)(binding,nic,0);
}
static U8 ethernet_binding_matches(PrereqLoaded *owner,InitBinding *b,EFI_HANDLE handle,U32 family) {
 return family<2 && range_contains(owner,(U64)b,sizeof(*b)) &&
  (U64)b-(U64)owner->base==(family?0xc158:0xc188) && b->version==10 && b->binding==handle &&
  (U64)b->supported==(U64)owner->base+(family?0x1510:0xf54) &&
  (U64)b->start==(U64)owner->base+(family?0x15b8:0xffc) &&
  (U64)b->stop==(U64)owner->base+(family?0x1a30:0x13e0);
}
static EFI_HANDLE ethernet_target(EFI_SYSTEM_TABLE *st) {
 EFI_BOOT_SERVICES *bs=st->boot_services;EFI_HANDLE *handles=0,result=0;UINTN count=0;U32 matches=0;
 EFI_STATUS status=((LocateHandles)bs->locate_handle_buffer)(2,&http_binding,0,&count,&handles);
 if(status==0 && handles && count<=128)for(UINTN i=0;i<count;i++) {
  void *path=0,*d4=0,*d6=0;
  if(bs->handle_protocol(handles[i],&path_guid,&path)==0 && path_matches(path,0,1) &&
     bs->handle_protocol(handles[i],&dhcp4_binding,&d4)==0 && d4 &&
     bs->handle_protocol(handles[i],&dhcp6_binding,&d6)==0 && d6) {result=handles[i];matches++;}
 }
 if(handles)((FreePool)bs->free_pool)(handles);
 number("ETHERNET_TARGET_MATCHES count=",matches);text("\n");return matches==1?result:0;
}
static void check_ethernet(EFI_SYSTEM_TABLE *st,EFI_HANDLE child,void *report) {
 EFI_BOOT_SERVICES *bs=st->boot_services;PrereqLoaded *owner=0;
 EFI_HANDLE nic=ethernet_target(st),*handles=0;UINTN count=0;InitBinding *selected[2]={0,0};
 if(!nic || bs->handle_protocol(child,&loaded_guid,(void **)&owner)!=0 || !owner ||
    owner->size!=sizeof(http_pin) || !path_matches(owner->path,&http_file,0)) {
  text("ETHERNET_GUARD_STOP\n");save(report);return;
 }
 EFI_STATUS status=((LocateHandles)bs->locate_handle_buffer)(2,&init_binding_guid,0,&count,&handles);
 U32 matches=0;
 if(status==0 && handles && count<=256)for(UINTN i=0;i<count;i++) {
  InitBinding *b=0;
  if(bs->handle_protocol(handles[i],&init_binding_guid,(void **)&b)!=0 || !b || b->image!=child)continue;
  U64 offset=(U64)b-(U64)owner->base;U32 family=offset==0xc188?0:offset==0xc158?1:2;
  if(family>1 || selected[family] || !ethernet_binding_matches(owner,b,handles[i],family)) {matches=99;break;}
  selected[family]=b;matches++;
 }
 if(handles)((FreePool)bs->free_pool)(handles);
 if(matches!=2 || !selected[0] || !selected[1]){text("ETHERNET_BINDING_GUARD_STOP\n");save(report);return;}
 text("ETHERNET_MAC 7c:c2:c6:1d:b2:f5\nETHERNET_SUPPORTED_BEGIN\n");save(report);
 for(U32 family=0;family<2;family++) {
  status=supported_check(selected[family],nic);
  number(family?"ETHERNET_IPV6_SUPPORTED status=":"ETHERNET_IPV4_SUPPORTED status=",status);text("\n");save(report);
 }
 text("ETHERNET_CONTROLLER_START_NOT_CALLED\nETHERNET_SUPPORTED_COMPLETE\n");save(report);
}

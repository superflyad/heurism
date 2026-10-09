/* Read-only prerequisites: no driver execution, policy calls or NIC attachment. */
#include "uefi.h"
static void inspect_prerequisites(EFI_SYSTEM_TABLE *, void *);
#ifndef PROBE_EXTENSION
#define PROBE_EXTENSION(st,file) inspect_prerequisites(st,file)
#endif
#ifndef PROBE_TAG
#define PROBE_TAG "COMPANION_HTTP_PREREQ_01"
#endif
#ifndef PROBE_REPORT_PATH
#define PROBE_REPORT_PATH u"\\EFI\\companion\\http-prereq-probe-01.txt"
#endif
#ifndef PROBE_DESCRIPTION
#define PROBE_DESCRIPTION "Read-only HTTP prerequisites; SSD management handoff.\n"
#endif
#include "firmware_probe.c"
#include "http_prereq_pins.h"
typedef EFI_STATUS (EFIAPI *ReadSection)(Fv *,EFI_GUID *,U8,UINTN,void **,UINTN *,U32 *);
typedef struct {
 U32 revision; EFI_HANDLE parent; EFI_SYSTEM_TABLE *st; EFI_HANDLE device;
 void *path,*reserved; U32 options_size; void *options,*base; U64 size;
 U32 code_type,data_type; void *unload;
} PrereqLoaded;
static EFI_GUID http_file={0xecebcb00,0xd9c8,0x11e4,{0xaf,0x3d,0x8c,0xdc,0xd4,0x26,0xc9,0x73}};
static EFI_GUID provider_file={0xa2ee1af9,0xcfdb,0x4f73,{0x82,0x9f,0x3d,0x2c,0xf7,0xe5,0x14,0x72}};
static EFI_GUID policy_interface={0x8f63ff6d,0xb7d4,0x474d,{0x8c,0x53,0x68,0x25,0x8a,0x22,0x4d,0x58}};
static EFI_GUID dhcp4_binding={0x9d9a39d8,0xbd42,0x4a73,{0xa4,0xd5,0x8e,0xe9,0x4b,0xe1,0x13,0x80}};
static EFI_GUID dhcp6_binding={0x9fb9a8a1,0x2f4a,0x43a6,{0x88,0x9c,0xd0,0xf7,0xb6,0xc4,0x7a,0xd5}};
static EFI_GUID http_binding={0xbdc8e6af,0xd9bc,0x4379,{0xa7,0x2a,0xe0,0xc4,0xe7,0x5d,0xae,0x1c}};
static U8 section_buffer[65536];
static U8 bytes_equal(const U8 *a,const U8 *b,UINTN n) {
 for(UINTN i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;
}
static U8 section_matches(EFI_STATUS status,void *buffer,UINTN size,const U8 *pin,UINTN expected) {
 return status==0 && buffer==section_buffer && size==expected && bytes_equal(buffer,pin,size);
}
static U8 path_matches(const U8 *p,EFI_GUID *file,U8 mac) {
 static const U8 expected_mac[6]={0x7c,0xc2,0xc6,0x1d,0xb2,0xf5};
 if(!p)return 0;
 UINTN at=0;U8 match=0;
 for(U32 nodes=0;nodes<64;nodes++) {
  if(at+4>2048)return 0;
  U16 n=u16(p+at+2);if(n<4 || n>2048-at)return 0;
  if(p[at]==0x7f)return p[at+1]==0xff && n==4 ? match:0;
  if(mac && p[at]==3 && p[at+1]==11 && n==37 && bytes_equal(p+at+4,expected_mac,6))match=1;
  if(file && p[at]==4 && p[at+1]==6 && n==20 && equal((EFI_GUID *)(p+at+4),file))match=1;
  at+=n;
 }
 return 0;
}
static U8 range_contains(PrereqLoaded *image,U64 address,UINTN bytes) {
 U64 base=(U64)image->base;
 return base && image->size && image->size<=0x1000000 && address>=base &&
        address-base<=image->size && bytes<=image->size-(address-base);
}
static void read_pinned_section(Fv *fv,EFI_GUID *name,U8 type,const U8 *pin,UINTN expected,const char *label) {
 void *buffer=section_buffer;UINTN size=sizeof(section_buffer);U32 auth=0;
 EFI_STATUS status=((ReadSection)fv->read_section)(fv,name,type,0,&buffer,&size,&auth);
 text(label);number(" status=",status);number(" bytes=",size);number(" auth=",auth);
 number(" exact_pin=",section_matches(status,buffer,size,pin,expected));text("\n");
}
static void inspect_prerequisites(EFI_SYSTEM_TABLE *st,void *report) {
 EFI_BOOT_SERVICES *bs=st->boot_services;EFI_HANDLE *handles=0;UINTN count=0;
 EFI_STATUS status=((LocateHandles)bs->locate_handle_buffer)(2,&fv_guid,0,&count,&handles);
 number("PREREQ_FV status=",status);number(" count=",count);text("\n");
 if(status==0 && handles && count<=128)for(UINTN i=0;i<count;i++) {
  Fv *fv=0;if(EFI_ERROR(bs->handle_protocol(handles[i],&fv_guid,(void **)&fv)) || !fv || !fv->read_section)continue;
  number("PREREQ_VOLUME index=",i);text("\n");
  read_pinned_section(fv,&http_file,0x10,http_pin,sizeof(http_pin),"HTTP_PE");
  read_pinned_section(fv,&http_file,0x13,depex_pin,sizeof(depex_pin),"HTTP_DEPEX");
  read_pinned_section(fv,&provider_file,0x10,provider_pin,sizeof(provider_pin),"POLICY_PE");
  save(report);
 }
 if(handles)((FreePool)bs->free_pool)(handles);
 handles=0;count=0;
 status=((LocateHandles)bs->locate_handle_buffer)(2,&http_binding,0,&count,&handles);
 number("HTTP_CONTROLLERS status=",status);number(" count=",count);text("\n");
 if(status==0 && handles && count<=128)for(UINTN i=0;i<count;i++) {
  void *dp=0,*d4=0,*d6=0;
  EFI_STATUS path=bs->handle_protocol(handles[i],&path_guid,&dp);
  EFI_STATUS s4=bs->handle_protocol(handles[i],&dhcp4_binding,&d4);
  EFI_STATUS s6=bs->handle_protocol(handles[i],&dhcp6_binding,&d6);
  number("HTTP_CONTROLLER index=",i);number(" path_status=",path);
  number(" target_mac=",path==0 && path_matches(dp,0,1));
  number(" dhcp4_status=",s4);number(" dhcp4_present=",s4==0 && d4!=0);
  number(" dhcp6_status=",s6);number(" dhcp6_present=",s6==0 && d6!=0);text("\n");
 }
 if(handles)((FreePool)bs->free_pool)(handles);
 void *policy=0;status=bs->locate_protocol(&policy_interface,0,&policy);
 number("POLICY_INTERFACE status=",status);number(" pointer=",(U64)policy);text("\n");
 handles=0;count=0;
 EFI_STATUS images=((LocateHandles)bs->locate_handle_buffer)(2,&loaded_guid,0,&count,&handles);
 number("LOADED_IMAGES status=",images);number(" count=",count);text("\n");
 U32 owners=0;
 if(images==0 && handles && count<=512)for(UINTN i=0;i<count;i++) {
  PrereqLoaded *image=0;
  if(EFI_ERROR(bs->handle_protocol(handles[i],&loaded_guid,(void **)&image)) || !image)continue;
  U8 provider=path_matches(image->path,&provider_file,0);
  U8 owns=status==0 && policy && range_contains(image,(U64)policy,8);
  if(!provider && !owns)continue;
  number("POLICY_IMAGE index=",i);number(" provider_path=",provider);number(" size=",image->size);
  number(" owns_interface=",owns);
  if(owns) {
   /* Only dereference the interface after its eight bytes are inside an image. */
   U64 method=u64(policy);U8 method_owned=range_contains(image,method,1);owners++;
   number(" interface_rva=",(U64)policy-(U64)image->base);
   number(" method_owned=",method_owned);
   if(method_owned)number(" method_rva=",method-(U64)image->base);
  }
  text("\n");
 }
 if(handles)((FreePool)bs->free_pool)(handles);
 number("POLICY_INTERFACE_IMAGE_OWNERS count=",owners);text("\nHTTP_PREREQUISITES_COMPLETE\n");save(report);
}
_Static_assert(__builtin_offsetof(PrereqLoaded,base)==64,"loaded base ABI");
_Static_assert(__builtin_offsetof(PrereqLoaded,size)==72,"loaded size ABI");
_Static_assert(__builtin_offsetof(Fv,read_section)==24,"read section ABI");

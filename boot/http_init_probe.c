/* Initialize only a pinned vendor HTTP boot driver. No explicit controller
 * connect/Start, LoadFile, HTTP Request, firmware dispatch or variable writes.
 * Remove this image's bindings before returning to SSD management.
 */
#include "uefi.h"
static void initialize_http(EFI_HANDLE,EFI_SYSTEM_TABLE *,void *);
#ifndef PROBE_EXTENSION
#define PROBE_EXTENSION(st,file) initialize_http(image,st,file)
#endif
#ifndef PROBE_TAG
#define PROBE_TAG "COMPANION_HTTP_INIT_01"
#endif
#ifndef PROBE_REPORT_PATH
#define PROBE_REPORT_PATH u"\\EFI\\companion\\http-init-probe-01.txt"
#endif
#ifndef PROBE_DESCRIPTION
#define PROBE_DESCRIPTION "Pinned HTTP driver registration experiment; SSD management preserved.\n"
#endif
#ifndef HTTP_CONTROLLER_CHECK
#define HTTP_CONTROLLER_CHECK(st,child,report) ((void)0)
#endif
#define PROBE_OUTPUT_SIZE 131072
#include "http_prereq_probe.c"
typedef EFI_STATUS (EFIAPI *GetMemoryMap)(UINTN *,void *,UINTN *,UINTN *,U32 *);
typedef EFI_STATUS (EFIAPI *UninstallProtocol)(EFI_HANDLE,EFI_GUID *,void *);
typedef struct {void *supported,*start,*stop;U32 version;EFI_HANDLE image,binding;} InitBinding;
static EFI_GUID init_binding_guid={0x18a031ab,0xb443,0x4d1a,{0xa5,0xc0,0x0c,0x09,0x26,0x1e,0x9f,0x71}};
static EFI_GUID init_names[2]={
 {0x107a772c,0xd5e1,0x11d4,{0x9a,0x46,0,0x90,0x27,0x3f,0xc1,0x4d}},
 {0x6a7a5cff,0xe8d9,0x4f70,{0xba,0xda,0x75,0xab,0x30,0x25,0xce,0x14}}
};
static EFI_GUID request_guid={0xa686d83e,0xe7e7,0x4c01,{0x83,0x35,0x6f,0xcc,0xb8,0xe7,0xc0,0x24}};
static U8 memory_map[65536],driver_buffer[sizeof(http_pin)],driver_path[4096];
static UINTN map_size,map_stride;
static U32 cleanup_errors;
typedef struct {
 EFI_TABLE_HEADER header;
 void *get_time,*set_time,*get_wake,*set_wake,*set_virtual,*convert;
 void *get_variable,*get_next,*set_variable,*get_monotonic;
 void (EFIAPI *reset_system)(U32,EFI_STATUS,UINTN,void *);
} InitRuntime;
static U8 readable_range(U64 address,UINTN length) {
 if(!length || !map_stride || map_stride<40 || map_size>sizeof(memory_map) || map_size%map_stride)return 0;
 for(UINTN at=0;at<map_size;at+=map_stride) {
  U8 *d=memory_map+at;U32 type=u32(d);U64 base=u64(d+8),pages=u64(d+24);
  if(type<1 || type>6 || pages>0xfffffffffffffULL || (u64(d+32)&0x2000))continue;
  U64 bytes=pages*4096;
  if(address>=base && address-base<=bytes && length<=bytes-(address-base))return 1;
 }
 return 0;
}
static U8 policy_list_clear(U64 head,const char *label) {
 if(!readable_range(head,16))return 0;
 U64 next=u64((U8 *)head),previous=head;U32 count=0;
 while(next!=head && count<128) {
  if(next<8 || !readable_range(next-8,48) || u64((U8 *)next+8)!=previous) {
   text("POLICY_LIST_INVALID\n");return 0;
  }
  U8 *node=(U8 *)(next-8);
  if(u32(node)!=0x48504244) {text("POLICY_NODE_SIGNATURE_MISMATCH\n");return 0;}
  if(equal((EFI_GUID *)(node+24),&request_guid)) {
   text("POLICY_HTTP_CALLBACK_PRESENT\n");return 0;
  }
  previous=next;next=u64((U8 *)next);count++;
 }
 number(label,count);text("\n");
 return next==head && u64((U8 *)head+8)==previous;
}
static U8 policy_gate(EFI_SYSTEM_TABLE *st) {
 EFI_BOOT_SERVICES *bs=st->boot_services;void *policy=0;
 if(bs->locate_protocol(&policy_interface,0,&policy)!=0 || !policy)return 0;
 EFI_HANDLE *handles=0;UINTN count=0;
 if(((LocateHandles)bs->locate_handle_buffer)(2,&loaded_guid,0,&count,&handles)!=0)return 0;
 PrereqLoaded *owner=0;
 if(handles && count<=512)for(UINTN i=0;i<count;i++) {
  PrereqLoaded *loaded=0;
  if(bs->handle_protocol(handles[i],&loaded_guid,(void **)&loaded)!=0 || !loaded)continue;
  if(path_matches(loaded->path,&provider_file,0) && range_contains(loaded,(U64)policy,8)) {
   if(owner){owner=0;break;}owner=loaded;
  }
 }
 ((FreePool)bs->free_pool)(handles);
 if(!owner || (U64)policy-(U64)owner->base!=0xc38 || owner->size!=sizeof(provider_pin) ||
    u64(policy)!=(U64)owner->base+0x894 || !range_contains(owner,(U64)owner->base+0x894,0x134) ||
    !bytes_equal((U8 *)owner->base+0x894,provider_pin+0x894,0x134)) {
  text("POLICY_IDENTITY_OR_CODE_MISMATCH\n");return 0;
 }
 if(!bs->get_memory_map)return 0;
 map_size=sizeof(memory_map);UINTN key=0;U32 version=0;map_stride=0;
 EFI_STATUS status=((GetMemoryMap)bs->get_memory_map)(&map_size,memory_map,&key,&map_stride,&version);
 number("POLICY_MEMORY_MAP status=",status);number(" size=",map_size);number(" stride=",map_stride);text("\n");
 if(status!=0 || version!=1)return 0;
 U64 base=(U64)owner->base;
 return policy_list_clear(base+0xc50,"POLICY_PENDING_CALLBACKS count=") &&
        policy_list_clear(base+0xc40,"POLICY_ACTIVE_CALLBACKS count=");
}
static U8 make_driver_path(U8 *path) {
 UINTN at=0;if(!path)return 0;
 for(U32 n=0;n<64;n++) {
  if(at+4>2048)return 0;U16 size=u16(path+at+2);
  if(size<4 || size>2048-at)return 0;
  if(path[at]==0x7f) {
   if(path[at+1]!=0xff || size!=4)return 0;
   driver_path[at]=4;driver_path[at+1]=6;driver_path[at+2]=20;driver_path[at+3]=0;
   for(U32 j=0;j<16;j++)driver_path[at+4+j]=((U8 *)&http_file)[j];
   driver_path[at+20]=0x7f;driver_path[at+21]=0xff;driver_path[at+22]=4;driver_path[at+23]=0;
   return 1;
  }
  for(U16 j=0;j<size;j++)driver_path[at+j]=path[at+j];at+=size;
 }
 return 0;
}
static U8 find_driver(EFI_SYSTEM_TABLE *st) {
 EFI_BOOT_SERVICES *bs=st->boot_services;EFI_HANDLE *handles=0;UINTN count=0;U32 found=0;
 if(((LocateHandles)bs->locate_handle_buffer)(2,&fv_guid,0,&count,&handles)!=0)return 0;
 if(handles && count<=128)for(UINTN i=0;i<count;i++) {
  Fv *fv=0;void *dp=0;if(bs->handle_protocol(handles[i],&fv_guid,(void **)&fv)!=0 || !fv || !fv->read_section)continue;
  void *buffer=section_buffer;UINTN size=sizeof(section_buffer);U32 auth=0;
  EFI_STATUS status=((ReadSection)fv->read_section)(fv,&http_file,0x13,0,&buffer,&size,&auth);
  if(!section_matches(status,buffer,size,depex_pin,sizeof(depex_pin)) || auth!=0)continue;
  buffer=section_buffer;size=sizeof(section_buffer);
  status=((ReadSection)fv->read_section)(fv,&http_file,0x10,0,&buffer,&size,&auth);
  if(!section_matches(status,buffer,size,http_pin,sizeof(http_pin)) || auth!=0)continue;
  if(bs->handle_protocol(handles[i],&path_guid,&dp)!=0 || !make_driver_path(dp))continue;
  for(UINTN j=0;j<sizeof(driver_buffer);j++)driver_buffer[j]=section_buffer[j];found++;
 }
 if(handles)((FreePool)bs->free_pool)(handles);
 number("PINNED_DRIVER_VOLUMES count=",found);text("\n");return found==1;
}
static U32 owned_bindings(EFI_SYSTEM_TABLE *st,EFI_HANDLE child,U8 remove) {
 EFI_BOOT_SERVICES *bs=st->boot_services;EFI_HANDLE *handles=0;UINTN count=0;U32 found=0;
 EFI_STATUS status=((LocateHandles)bs->locate_handle_buffer)(2,&init_binding_guid,0,&count,&handles);
 if(status!=0 || !handles || count>256) {if(handles)((FreePool)bs->free_pool)(handles);return 0xffffffff;}
 for(UINTN i=0;i<count;i++) {
  InitBinding *binding=0;PrereqLoaded *image=0;
  if(bs->handle_protocol(handles[i],&init_binding_guid,(void **)&binding)!=0 || !binding)continue;
  if(child) {if(binding->image!=child)continue;}
  else {
   if(bs->handle_protocol(binding->image,&loaded_guid,(void **)&image)!=0 || !image || !path_matches(image->path,&http_file,0))continue;
  }
  found++;number(remove?"REMOVE_BINDING index=":"HTTP_BOOT_BINDING index=",i);number(" version=",binding->version);text("\n");
  if(remove) {
   EFI_HANDLE handle=handles[i];
   for(U32 j=0;j<2;j++) {
    void *interface=0;status=bs->handle_protocol(handle,&init_names[j],&interface);
    if(status==0 && interface)status=((UninstallProtocol)bs->uninstall_protocol_interface)(handle,&init_names[j],interface);
    if(status!=0)cleanup_errors++;
    number("REMOVE_NAME status=",status);text("\n");
   }
   status=((UninstallProtocol)bs->uninstall_protocol_interface)(handle,&init_binding_guid,binding);
   if(status!=0)cleanup_errors++;
   number("REMOVE_DRIVER_BINDING status=",status);text("\n");
  }
 }
 ((FreePool)bs->free_pool)(handles);return found;
}
static void initialize_http(EFI_HANDLE parent,EFI_SYSTEM_TABLE *st,void *report) {
 inspect_prerequisites(st,report);
 EFI_BOOT_SERVICES *bs=st->boot_services;
 cleanup_errors=0;
 if(!report || !bs->uninstall_protocol_interface || owned_bindings(st,0,0)!=0 || !policy_gate(st) || !find_driver(st)) {
  text("HTTP_INIT_GUARD_STOP\nHTTP_INITIALIZATION_COMPLETE\n");save(report);return;
 }
 text("HTTP_INIT_GATES_PASSED\nHTTP_LOAD_BEGIN\n");save(report);
 EFI_HANDLE child=0;EFI_STATUS status=((LoadImage)bs->load_image)(0,parent,driver_path,driver_buffer,sizeof(driver_buffer),&child);
 number("HTTP_LOAD status=",status);text("\n");save(report);
 if(status==0 && child) {
  text("HTTP_START_BEGIN\n");save(report);
  status=((StartImage)bs->start_image)(child,0,0);
  number("HTTP_START status=",status);text("\n");save(report);
  if(status==0) {
   number("HTTP_INITIALIZED_BINDINGS count=",owned_bindings(st,child,0));text("\n");save(report);
   HTTP_CONTROLLER_CHECK(st,child,report);
   owned_bindings(st,child,1);
   U32 remaining=owned_bindings(st,child,0);
   number("HTTP_BINDINGS_AFTER_REMOVAL count=",remaining);number(" cleanup_errors=",cleanup_errors);text("\n");
   /* Do not call the vendor unload callback: it enumerates/disconnects every
    * controller and invokes an unverified packed cleanup protocol. With this
    * image's binding/name interfaces removed, keep its boot-service code/data
    * resident until ExitBootServices instead of freeing referenced memory. */
   text("HTTP_IMAGE_RETAINED_UNTIL_EXIT_BOOT_SERVICES\n");
   if(remaining || cleanup_errors) {
    /* BootNext was consumed to reach this app. A clean firmware reboot now
     * uses the unchanged SSD defaults instead of handing off active bindings. */
    text("HTTP_CLEANUP_RESET\nHTTP_INITIALIZATION_COMPLETE\nPROBE_COMPLETE\n");save(report);
    InitRuntime *rt=(InitRuntime *)st->runtime_services;
    if(rt && rt->reset_system)rt->reset_system(0,0,0,0);
    text("HTTP_CLEANUP_RESET_RETURNED\n");save(report);
   }
  }
 }
 text("HTTP_INITIALIZATION_COMPLETE\n");save(report);
}
_Static_assert(__builtin_offsetof(InitBinding,image)==32,"binding image ABI");
_Static_assert(__builtin_offsetof(InitRuntime,reset_system)==104,"reset ABI");

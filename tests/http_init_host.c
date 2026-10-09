#ifdef TEST_HTTP_ETHERNET_PROBE
#include "../boot/http_ethernet_probe.c"
#else
#include "../boot/http_init_probe.c"
#endif
static EFI_BOOT_SERVICES fixture_bs;
static EFI_SYSTEM_TABLE fixture_st;
static PrereqLoaded policy_loaded,other_loaded;
static InitBinding bindings[3];
static Fv fixture_fv;
static U8 provider_image[4096],nodes[144];
static U8 provider_path[24],fv_path[4]={0x7f,0xff,4,0},other_path[4]={0x7f,0xff,4,0};
static EFI_HANDLE list[4];
static U32 active,loads,starts,removes,fail_load,fail_start,existing,corrupt_http,invalid_map,failed_remove;
static InitRuntime fixture_runtime;
static U32 resets;
static void EFIAPI fixture_reset(U32 type,EFI_STATUS status,UINTN length,void *data) {
 if(type || status || length || data)resets=100;else resets++;
}
static void put64(U8 *p,U64 value) {for(U32 i=0;i<8;i++)p[i]=(U8)(value>>(i*8));}
static U8 contains(const char *needle) {
 for(UINTN i=0;i<used;i++){UINTN j=0;while(needle[j] && i+j<used && output[i+j]==needle[j])j++;if(!needle[j])return 1;}return 0;
}
static EFI_STATUS EFIAPI fixture_locate(U32 type,EFI_GUID *g,void *key,UINTN *count,EFI_HANDLE **out) {
 if(type!=2 || key)return EFI_UNSUPPORTED;
 *out=list;
 if(equal(g,&fv_guid)){*count=1;list[0]=(EFI_HANDLE)11;}
 else if(equal(g,&loaded_guid)){*count=2;list[0]=(EFI_HANDLE)13;list[1]=(EFI_HANDLE)99;}
 else if(equal(g,&http_binding)){return 0x800000000000000eULL;}
 else if(equal(g,&init_binding_guid)) {
  *count=1;list[0]=(EFI_HANDLE)20;
  if(active&1)list[(*count)++]=(EFI_HANDLE)21;
  if(active&2)list[(*count)++]=(EFI_HANDLE)22;
 }else return EFI_UNSUPPORTED;
 return 0;
}
static EFI_STATUS EFIAPI fixture_handle(EFI_HANDLE handle,EFI_GUID *g,void **out) {
 U64 h=(U64)handle;
 if(equal(g,&loaded_guid)) {
  if(h==13)*out=&policy_loaded;
  else if(h==99)*out=existing?&policy_loaded:&other_loaded;
  else return EFI_UNSUPPORTED;
 }else if(equal(g,&fv_guid) && h==11)*out=&fixture_fv;
 else if(equal(g,&path_guid) && h==11)*out=fv_path;
 else if(equal(g,&init_binding_guid) && h>=20 && h<=22)*out=&bindings[h-20];
 else if((equal(g,&init_names[0]) || equal(g,&init_names[1])) && h>=21 && h<=22)*out=provider_image;
 else return EFI_UNSUPPORTED;
 return 0;
}
static EFI_STATUS EFIAPI fixture_policy(EFI_GUID *g,void *key,void **out) {
 if(!equal(g,&policy_interface) || key)return EFI_UNSUPPORTED;
 *out=provider_image+0xc38;return 0;
}
static EFI_STATUS EFIAPI fixture_free(void *p){return p==list?0:EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI fixture_map(UINTN *size,void *out,UINTN *key,UINTN *stride,U32 *version) {
 if(*size!=sizeof(memory_map) || out!=memory_map)return EFI_UNSUPPORTED;
 for(U32 i=0;i<80;i++)memory_map[i]=0;
 memory_map[0]=3;put64(memory_map+8,(U64)provider_image);put64(memory_map+24,1);
 memory_map[40]=4;put64(memory_map+48,(U64)nodes);put64(memory_map+64,1);
 *size=80;*key=123;*stride=invalid_map?16:40;*version=1;return 0;
}
static EFI_STATUS EFIAPI fixture_section(Fv *fv,EFI_GUID *g,U8 type,UINTN instance,void **out,UINTN *size,U32 *auth) {
 if(fv!=&fixture_fv || instance || *out!=section_buffer)return EFI_UNSUPPORTED;
 const U8 *pin=0;UINTN bytes=0;
 if(equal(g,&http_file) && type==0x10){pin=http_pin;bytes=sizeof(http_pin);}
 else if(equal(g,&http_file) && type==0x13){pin=depex_pin;bytes=sizeof(depex_pin);}
 else if(equal(g,&provider_file) && type==0x10){pin=provider_pin;bytes=sizeof(provider_pin);}
 else return 0x800000000000000eULL;
 if(*size<bytes)return EFI_UNSUPPORTED;
 for(UINTN i=0;i<bytes;i++)section_buffer[i]=pin[i];
 if(corrupt_http && pin==http_pin)section_buffer[0]^=1;
 *size=bytes;*auth=0;return 0;
}
static EFI_STATUS EFIAPI fixture_load(U8 policy,EFI_HANDLE parent,void *path,void *buffer,UINTN size,EFI_HANDLE *out) {
 if(policy || parent!=(EFI_HANDLE)1 || buffer!=driver_buffer || size!=sizeof(http_pin) ||
    !bytes_equal(buffer,http_pin,size) || !path_matches(path,&http_file,0))return EFI_UNSUPPORTED;
 loads++;if(fail_load)return EFI_UNSUPPORTED;*out=(EFI_HANDLE)30;return 0;
}
static EFI_STATUS EFIAPI fixture_start(EFI_HANDLE child,UINTN *size,U16 **data) {
 if(child!=(EFI_HANDLE)30 || size || data)return EFI_UNSUPPORTED;
 starts++;if(fail_start)return EFI_UNSUPPORTED;active=3;return 0;
}
static EFI_STATUS EFIAPI fixture_remove(EFI_HANDLE handle,EFI_GUID *g,void *interface) {
 U64 h=(U64)handle;if(h<21 || h>22)return EFI_UNSUPPORTED;
 if(equal(g,&init_binding_guid)) {
  if(interface!=&bindings[h-20])return EFI_UNSUPPORTED;
  if(failed_remove)return EFI_UNSUPPORTED;
  active&=~(1U<<(h-21));removes++;
 }else if(!equal(g,&init_names[0]) && !equal(g,&init_names[1]))return EFI_UNSUPPORTED;
 return 0;
}
static void reset(void) {
 active=loads=starts=removes=fail_load=fail_start=existing=corrupt_http=invalid_map=failed_remove=resets=0;
 for(UINTN i=0;i<sizeof(provider_pin);i++)provider_image[i]=provider_pin[i];
 provider_path[0]=4;provider_path[1]=6;provider_path[2]=20;
 for(U32 i=0;i<16;i++)provider_path[4+i]=((U8 *)&provider_file)[i];
 provider_path[20]=0x7f;provider_path[21]=0xff;provider_path[22]=4;
 policy_loaded.path=provider_path;policy_loaded.base=provider_image;policy_loaded.size=sizeof(provider_pin);
 other_loaded.path=other_path;
 put64(provider_image+0xc38,(U64)provider_image+0x894);
 U64 head=(U64)provider_image+0xc50;
 put64(provider_image+0xc50,(U64)nodes+8);put64(provider_image+0xc58,(U64)nodes+104);
 put64(provider_image+0xc40,(U64)provider_image+0xc40);put64(provider_image+0xc48,(U64)provider_image+0xc40);
 for(U32 i=0;i<3;i++) {
  U8 *node=nodes+i*48;put64(node+8,i==2?head:(U64)node+56);
  node[0]=0x44;node[1]=0x42;node[2]=0x50;node[3]=0x48;
  put64(node+16,i==0?head:(U64)node-40);
  for(U32 j=0;j<16;j++)node[24+j]=0;
  node[24]=(U8)(i+1);
 }
 for(U32 i=0;i<3;i++) {bindings[i].image=(EFI_HANDLE)(U64)(i?30:99);bindings[i].binding=(EFI_HANDLE)(U64)(20+i);}
 fixture_st.boot_services=&fixture_bs;
 fixture_st.runtime_services=&fixture_runtime;fixture_runtime.reset_system=fixture_reset;
 fixture_bs.locate_handle_buffer=(void *)fixture_locate;fixture_bs.handle_protocol=fixture_handle;
 fixture_bs.locate_protocol=fixture_policy;fixture_bs.free_pool=(void *)fixture_free;
 fixture_bs.get_memory_map=(void *)fixture_map;fixture_bs.load_image=(void *)fixture_load;
 fixture_bs.start_image=(void *)fixture_start;fixture_bs.uninstall_protocol_interface=(void *)fixture_remove;
 fixture_fv.read_section=(void *)fixture_section;used=0;
}
/* A report File with harmless ABI-matching write/flush callbacks. */
static EFI_STATUS EFIAPI position(File *f,U64 at){(void)f;return at?EFI_UNSUPPORTED:0;}
static EFI_STATUS EFIAPI write_report(File *f,UINTN *size,void *data){(void)f;(void)size;(void)data;return 0;}
static EFI_STATUS EFIAPI flush_report(File *f){(void)f;return 0;}
int init_run_tests(void) {
 static File report;report.set_position=position;report.write=write_report;report.flush=flush_report;
 reset();initialize_http((EFI_HANDLE)1,&fixture_st,&report);
 if(loads!=1 || starts!=1 || removes!=2 || active || !contains("HTTP_BINDINGS_AFTER_REMOVAL count=0x0000000000000000"))return 1;
 reset();for(U32 i=0;i<16;i++)nodes[24+i]=((U8 *)&request_guid)[i];
 initialize_http((EFI_HANDLE)1,&fixture_st,&report);if(loads || !contains("POLICY_HTTP_CALLBACK_PRESENT"))return 2;
 reset();provider_image[0x894]^=1;initialize_http((EFI_HANDLE)1,&fixture_st,&report);if(loads)return 3;
 reset();invalid_map=1;initialize_http((EFI_HANDLE)1,&fixture_st,&report);if(loads)return 4;
 reset();put64(nodes+8,1);initialize_http((EFI_HANDLE)1,&fixture_st,&report);if(loads)return 5;
 reset();corrupt_http=1;initialize_http((EFI_HANDLE)1,&fixture_st,&report);if(loads)return 6;
 reset();fail_load=1;initialize_http((EFI_HANDLE)1,&fixture_st,&report);if(loads!=1 || starts)return 7;
 reset();fail_start=1;initialize_http((EFI_HANDLE)1,&fixture_st,&report);if(starts!=1 || removes)return 8;
 reset();failed_remove=1;initialize_http((EFI_HANDLE)1,&fixture_st,&report);
 if(active!=3 || resets!=1 || !contains("HTTP_BINDINGS_AFTER_REMOVAL count=0x0000000000000002"))return 9;
 reset();initialize_http((EFI_HANDLE)1,&fixture_st,0);if(loads)return 10;
 reset();fv_path[2]=0;initialize_http((EFI_HANDLE)1,&fixture_st,&report);fv_path[2]=4;if(loads)return 11;
 return 0;
}

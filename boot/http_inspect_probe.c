/* Inventory standard interfaces only. No LoadFile, network-start, HII
 * ExtractConfig/RouteConfig/Callback, certificate or variable-write calls.
 * HII ExportPackageLists can notify providers to refresh package metadata.
 * Only summary metadata is saved, never raw strings or configuration values.
 */
#include "uefi.h"
typedef struct File File;
static void inspect_http(EFI_SYSTEM_TABLE *,File *);
#define PROBE_EXTENSION(st,file) inspect_http(st,file)
#define PROBE_TAG "COMPANION_HTTP_INSPECT_01"
#define PROBE_DESCRIPTION "HTTP bootstrap provider and HII metadata inventory; no network transactions.\n"
#define PROBE_REPORT_PATH u"\\EFI\\companion\\http-inspect-probe-01.txt"
#define PROBE_GRUB_PATH u"\\EFI\\alpine\\grubx64.efi"
#define PROBE_OUTPUT_SIZE 131072
#include "firmware_probe.c"

typedef struct {U32 revision;EFI_HANDLE parent;EFI_SYSTEM_TABLE *st;EFI_HANDLE device;U8 *path;} InspectLoaded;
typedef struct {void *supported,*start,*stop;U32 version;EFI_HANDLE image,binding;} InspectBinding;
typedef struct HiiDatabase HiiDatabase;
struct HiiDatabase {
 void *new_list,*remove_list,*update_list;
 EFI_STATUS (EFIAPI *list)(HiiDatabase *,U8,EFI_GUID *,UINTN *,EFI_HANDLE *);
 EFI_STATUS (EFIAPI *export_list)(HiiDatabase *,EFI_HANDLE,UINTN *,void *);
 void *register_notify,*unregister_notify,*find_keyboard,*get_keyboard,*set_keyboard;
 EFI_STATUS (EFIAPI *driver)(HiiDatabase *,EFI_HANDLE,EFI_HANDLE *);
};
typedef struct {const char *name;EFI_GUID id;} InspectGuid;
static InspectGuid inspect_guids[]={
 {"LOAD_FILE",{0x56ec3091,0x954c,0x11d2,{0x8e,0x3f,0,0xa0,0xc9,0x69,0x72,0x3b}}},
 {"LOAD_FILE2",{0x4006c0c1,0xfcb3,0x403e,{0x99,0x6d,0x4a,0x6c,0x87,0x24,0xe0,0x6d}}},
 {"HTTP_BINDING",{0xbdc8e6af,0xd9bc,0x4379,{0xa7,0x2a,0xe0,0xc4,0xe7,0x5d,0xae,0x1c}}},
 {"HTTP",{0x7a59b29b,0x910b,0x4171,{0x82,0x42,0xa8,0x5a,0x0d,0xf2,0x5b,0x5b}}},
 {"TLS_BINDING",{0x952cb795,0xff36,0x48cf,{0xa2,0x49,0x4d,0xf4,0x86,0xd6,0xab,0x8d}}},
 {"TLS_CONFIG",{0x1682fe44,0xbd7a,0x4407,{0xb7,0xc7,0xdc,0xa3,0x7c,0xa3,0x92,0x2d}}},
 {"HII_CONFIG_ACCESS",{0x330d4706,0xf2a0,0x4e4f,{0xa3,0x69,0xb6,0x6f,0xa8,0xd5,0x43,0x85}}},
};
static EFI_GUID database_guid={0xef9fc172,0xa1b2,0x4693,{0xb3,0x27,0x6d,0x32,0xfc,0x41,0x60,0x42}};
static EFI_GUID binding_guid={0x18a031ab,0xb443,0x4d1a,{0xa5,0xc0,0x0c,0x09,0x26,0x1e,0x9f,0x71}};
static EFI_GUID http_image_guid={0xecebcb00,0xd9c8,0x11e4,{0xaf,0x3d,0x8c,0xdc,0xd4,0x26,0xc9,0x73}};
static EFI_HANDLE hii_handles[256];
static U8 hii_package[1048576];
static U8 inspect_path(U8 *p) {
 UINTN at=0;U8 found=0;if(!p){text(" PATH_ABSENT");return 0;}
 for(U32 n=0;n<64;n++) {
  if(at+4>2048){text(" PATH_LIMIT");return found;}
  U16 bytes=u16(p+at+2);if(bytes<4 || at+bytes>2048){text(" PATH_INVALID");return found;}
  text(" NODE=");hex(p[at],2);hex(p[at+1],2);number(" len=",bytes);
  if((p[at]==4 && p[at+1]==6) || (p[at]==1 && p[at+1]==4)) {
   if(bytes>=20){text(" guid=");guid(p+at+4);if(equal((EFI_GUID *)(p+at+4),&http_image_guid))found=1;}
  }
  if(p[at]==3 && p[at+1]==24)text(" URI_PRESENT");
  if(p[at]==0x7f){if(p[at+1]!=0xff || bytes!=4)text(" PATH_INVALID");return found;}
  at+=bytes;
 }
 text(" PATH_LIMIT");return found;
}
static U8 keyword(const U8 *p,UINTN bytes,const char *word) {
 UINTN length=0;while(word[length])length++;
 for(UINTN at=0;at<bytes;at++)for(UINTN stride=1;stride<=2;stride++) {
  if(length*stride>bytes-at)continue;U8 match=1;
  for(UINTN j=0;j<length;j++) {
   U8 c=p[at+j*stride];if(c>='a' && c<='z')c-=32;
   if(c!=(U8)word[j] || (stride==2 && p[at+j*stride+1])){match=0;break;}
  }
  if(match)return 1;
 }
 return 0;
}
static void package_metadata(UINTN bytes) {
 if(bytes<20 || u32(hii_package+16)<20 || u32(hii_package+16)>bytes){text(" HII_BAD_LENGTH\n");return;}
 /* Success need not reduce BufferSize to the actual copied list length. */
 bytes=u32(hii_package+16);number(" actual_package_bytes=",bytes);
 text(" package_guid=");guid(hii_package);
 U8 http=keyword(hii_package,bytes,"HTTP");number(" http_keyword=",http);
 number(" uri_keyword=",keyword(hii_package,bytes,"URI"));
 number(" certificate_keyword=",keyword(hii_package,bytes,"CERTIFICATE"));text("\n");
 UINTN at=20;
 for(U32 n=0;n<512 && at<bytes;n++) {
  if(bytes-at<4){text("HII_PACKAGE_INVALID\n");return;}
  U32 raw=u32(hii_package+at),length=raw&0xffffff,type=raw>>24;
  if(length<4 || length>bytes-at){text("HII_PACKAGE_INVALID\n");return;}
  if(type==2) {
   UINTN op=at+4,end=at+length;
   for(U32 count=0;count<4096 && op<end;count++) {
    if(end-op<2){text("HII_IFR_INVALID\n");return;}
    U8 code=hii_package[op],size=hii_package[op+1]&0x7f;
    if(size<2 || size>end-op){text("HII_IFR_INVALID\n");return;}
    if(code==0x0e && size>=23){text("HII_FORMSET ");guid(hii_package+op+2);text("\n");}
    if(http && code==0x24 && size>=23) {
     text("HII_HTTP_VARSTORE guid=");guid(hii_package+op+2);number(" id=",u16(hii_package+op+18));
     number(" size=",u16(hii_package+op+20));text(" name=");
     for(UINTN j=22;j<size && j<86;j++) {
      U8 c=hii_package[op+j];if(!c)break;
      if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_')){text("[REDACTED]");break;}
      char s[2]={(char)c,0};text(s);
     }
     text("\n");
    }
    op+=size;
   }
  }
  at+=length;
 }
 if(at!=bytes)text("HII_PACKAGE_LIMIT\n");
}
static void inspect_http(EFI_SYSTEM_TABLE *st,File *file) {
 EFI_BOOT_SERVICES *bs=st->boot_services;
 for(UINTN k=0;k<sizeof(inspect_guids)/sizeof(inspect_guids[0]);k++) {
  EFI_HANDLE *handles=0;UINTN count=0;
  EFI_STATUS s=((LocateHandles)bs->locate_handle_buffer)(2,&inspect_guids[k].id,0,&count,&handles);
  text("INSPECT_LOCATE ");text(inspect_guids[k].name);number(" status=",s);number(" count=",count);text("\n");
  if(!EFI_ERROR(s) && handles) {
   if(count<=128)for(UINTN i=0;i<count;i++) {
    U8 *path=0;void *pxe=0;s=bs->handle_protocol(handles[i],&path_guid,(void **)&path);
    text("INSPECT_INTERFACE ");text(inspect_guids[k].name);number(" index=",i);number(" path_status=",s);
    if(!EFI_ERROR(s))inspect_path(path);
    EFI_GUID pxe_guid={0x03c4e603,0xac28,0x11d3,{0x9a,0x2d,0,0x90,0x27,0x3f,0xc1,0x4d}};
    if(k==0)number(" pxe_interface_status=",bs->handle_protocol(handles[i],&pxe_guid,&pxe));
    text("\n");
   } else text("INSPECT_HANDLE_LIMIT\n");
   ((FreePool)bs->free_pool)(handles);
  }
  save(file);
 }
 EFI_HANDLE *bindings=0;UINTN count=0;EFI_STATUS s=((LocateHandles)bs->locate_handle_buffer)(2,&binding_guid,0,&count,&bindings);
 number("BINDING_LOCATE status=",s);number(" count=",count);text("\n");
 if(!EFI_ERROR(s) && bindings) {
  if(count<=256)for(UINTN i=0;i<count;i++) {
   InspectBinding *binding=0;InspectLoaded *loaded=0;
   if(EFI_ERROR(bs->handle_protocol(bindings[i],&binding_guid,(void **)&binding)) || !binding)continue;
   if(EFI_ERROR(bs->handle_protocol(binding->image,&loaded_guid,(void **)&loaded)) || !loaded)continue;
   number("BINDING_IMAGE index=",i);number(" version=",binding->version);
   U8 found=inspect_path(loaded->path);number(" http_boot_image=",found);text("\n");
  } else text("BINDING_HANDLE_LIMIT\n");
  ((FreePool)bs->free_pool)(bindings);
 }
 save(file);
 HiiDatabase *db=0;s=bs->locate_protocol(&database_guid,0,(void **)&db);
 number("HII_DATABASE status=",s);text("\n");save(file);
 if(!EFI_ERROR(s) && db && db->list && db->export_list && db->driver) {
  UINTN bytes=sizeof(hii_handles);s=db->list(db,0,0,&bytes,hii_handles);
  number("HII_LIST status=",s);number(" bytes=",bytes);text("\n");save(file);
  if(!EFI_ERROR(s) && bytes<=sizeof(hii_handles) && bytes%sizeof(EFI_HANDLE)==0)
   for(UINTN i=0;i<bytes/sizeof(EFI_HANDLE);i++) {
    EFI_HANDLE driver=0;U8 *path=0;number("HII_LIST_ITEM index=",i);
    s=db->driver(db,hii_handles[i],&driver);number(" driver_status=",s);
    if(!EFI_ERROR(s) && driver && !EFI_ERROR(bs->handle_protocol(driver,&path_guid,(void **)&path)))inspect_path(path);
    UINTN length=sizeof(hii_package);s=db->export_list(db,hii_handles[i],&length,hii_package);
    number(" export_status=",s);number(" bytes=",length);
    if(!EFI_ERROR(s) && length<=sizeof(hii_package))package_metadata(length);else text(" HII_EXPORT_SKIPPED\n");
    save(file);
   }
 }
 text("HTTP_INSPECTION_COMPLETE\n");save(file);
}
_Static_assert(__builtin_offsetof(InspectLoaded,path)==32,"loaded path ABI");
_Static_assert(__builtin_offsetof(InspectBinding,image)==32,"binding image ABI");
_Static_assert(__builtin_offsetof(HiiDatabase,driver)==80,"HII driver ABI");

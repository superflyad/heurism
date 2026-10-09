/* Inventory capsule-update providers. No SetImage/CheckImage/UpdateCapsule calls. */
#include "uefi.h"
typedef struct File File;
static void update_inventory(EFI_SYSTEM_TABLE *, File *);
#define PROBE_EXTENSION update_inventory
#define PROBE_TAG "COMPANION_UPDATE_PROBE_01"
#define PROBE_REPORT_PATH u"\\EFI\\companion\\update-probe-01.txt"
#define PROBE_GRUB_PATH u"\\EFI\\companion\\recoveryx64.efi"
#include "firmware_probe.c"

typedef struct Fmp Fmp;
struct Fmp {
    EFI_STATUS (EFIAPI *get_info)(Fmp *, UINTN *, void *, U32 *, U8 *, UINTN *, U32 *, U16 **);
    void *get_image, *set_image, *check_image, *get_package, *set_package;
};
typedef struct {
    U8 index; EFI_GUID type; U64 id; U16 *id_name; U32 version; U16 *version_name;
    UINTN size; U64 supported, setting, compatibility;
    U32 lowest, last_version, last_status; U64 instance; void *dependencies;
} Descriptor;
static EFI_GUID fmp_guid={0x86c77a67,0x0b97,0x4633,{0xa1,0x87,0x49,0x10,0x4d,0x06,0x85,0xc7}};
static U8 info_buffer[65536];
static void update_inventory(EFI_SYSTEM_TABLE *st, File *file) {
    EFI_BOOT_SERVICES *bs=st->boot_services;
    EFI_HANDLE *handles=0; UINTN count=0;
    EFI_STATUS result=((LocateHandles)bs->locate_handle_buffer)(2,&fmp_guid,0,&count,&handles);
    number("FMP_LOCATE status=",result); number(" count=",count); text("\n"); save(file);
    if(EFI_ERROR(result) || !handles || count>128) return;
    for(UINTN i=0;i<count;i++) {
        Fmp *fmp=0; result=bs->handle_protocol(handles[i],&fmp_guid,(void **)&fmp);
        number("FMP index=",i); number(" interface_status=",result); text("\n");
        if(EFI_ERROR(result) || !fmp || !fmp->get_info) {save(file);continue;}
        for(UINTN j=0;j<sizeof(info_buffer);j++)info_buffer[j]=0;
        UINTN bytes=sizeof(info_buffer), stride=0; U32 version=0, package=0;
        U8 images=0; U16 *package_name=0;
        result=fmp->get_info(fmp,&bytes,info_buffer,&version,&images,&stride,&package,&package_name);
        number("FMP_INFO status=",result); number(" bytes=",bytes); number(" descriptor_version=",version);
        number(" image_count=",images); number(" descriptor_size=",stride); number(" package_version=",package); text("\n");
        if(package_name)((FreePool)bs->free_pool)(package_name);
        if(EFI_ERROR(result)) {save(file);continue;}
        UINTN minimum=version==1?88:version==2?92:version>=3?112:0;
        if(!minimum || stride<minimum || bytes>sizeof(info_buffer) || stride>sizeof(info_buffer) ||
           (images && stride>bytes/images)) {text("FMP_BAD_BOUNDS\n");save(file);continue;}
        for(U32 j=0;j<images;j++) {
            U8 *d=info_buffer+j*stride;
            number("FMP_IMAGE provider=",i); number(" index=",d[0]); text(" type=");guid(d+4);
            number(" size=",u64(d+56));
            if(u64(d+56)) {
                number(" version=",u32(d+40)); number(" supported=",u64(d+64));
                number(" setting=",u64(d+72)); number(" compatibility=",u64(d+80));
                if(version>=2)number(" lowest=",u32(d+88));
                if(version>=3) {number(" last_version=",u32(d+92));number(" last_status=",u32(d+96));}
            }
            text("\n");
        }
        save(file);
    }
    ((FreePool)bs->free_pool)(handles);
}
_Static_assert(__builtin_offsetof(Descriptor,type)==4,"FMP descriptor GUID ABI");
_Static_assert(__builtin_offsetof(Descriptor,size)==56,"FMP descriptor size ABI");
_Static_assert(__builtin_offsetof(Descriptor,lowest)==88,"FMP descriptor version ABI");
_Static_assert(__builtin_offsetof(Descriptor,instance)==104,"FMP descriptor instance ABI");

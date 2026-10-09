/* Inventory only: never starts a NIC, DHCP, PXE or HTTP transaction. */
#include "uefi.h"
typedef struct File File;
static void network_inventory(EFI_SYSTEM_TABLE *,File *);
#define PROBE_EXTENSION(st,file) network_inventory(st,file)
#define PROBE_TAG "COMPANION_NETWORK_PROBE_01"
#define PROBE_DESCRIPTION "Read-only network interface inventory; SSD fallback retained.\n"
#define PROBE_REPORT_PATH u"\\EFI\\companion\\network-probe-01.txt"
#define PROBE_GRUB_PATH u"\\EFI\\companion\\recoveryx64.efi"
#include "firmware_probe.c"

typedef struct {
 U32 state,address_size,header_size,max_packet,nv_size,nv_access;
 U32 filter_mask,filter_setting,max_multicast,multicast_count;
 U8 multicast[16][32],current[32],broadcast[32],permanent[32];
 U8 type,changeable,multiple_tx,media_supported,media_present;
} NetworkMode;
typedef struct { U64 revision;void *methods[13];EFI_EVENT event;NetworkMode *mode; } Network;
typedef struct { U64 revision;void *methods[12];U8 *mode; } Pxe;
typedef struct { const char *name;EFI_GUID guid; } NetworkGuid;
static NetworkGuid network_guids[]={
 {"SNP",{0xa19832b9,0xac25,0x11d3,{0x9a,0x2d,0,0x90,0x27,0x3f,0xc1,0x4d}}},
 {"PXE",{0x03c4e603,0xac28,0x11d3,{0x9a,0x2d,0,0x90,0x27,0x3f,0xc1,0x4d}}},
 {"LOAD_FILE",{0x56ec3091,0x954c,0x11d2,{0x8e,0x3f,0,0xa0,0xc9,0x69,0x72,0x3b}}},
 {"HTTP_BINDING",{0xbdc8e6af,0xd9bc,0x4379,{0xa7,0x2a,0xe0,0xc4,0xe7,0x5d,0xae,0x1c}}},
 {"HTTP",{0x7a59b29b,0x910b,0x4171,{0x82,0x42,0xa8,0x5a,0x0d,0xf2,0x5b,0x5b}}},
 {"IP4_CONFIG2",{0x5b446ed1,0xe30b,0x4faa,{0x87,0x1a,0x36,0x54,0xec,0xa3,0x60,0x80}}},
 {"UDP4_BINDING",{0x83f01464,0x99bd,0x45e5,{0xb3,0x83,0xaf,0x63,0x05,0xd8,0xe9,0xe6}}},
};
static void network_path(U8 *p) {
 if(!p)return;
 UINTN at=0;
 for(U32 n=0;n<32;n++) {
  if(at+4>512){text(" NET_PATH_LIMIT");return;}
  U16 size=u16(p+at+2);
  if(size<4 || at+size>512){text(" NET_PATH_INVALID");return;}
  text(" NODE=");hex(p[at],2);hex(p[at+1],2);text(":");
  for(U16 j=0;j<size;j++)hex(p[at+j],2);
  if(p[at]==0x7f){if(p[at+1]!=0xff || size!=4)text(" NET_PATH_INVALID");return;}
  at+=size;
 }
 text(" NET_PATH_LIMIT");
}
static void network_inventory(EFI_SYSTEM_TABLE *st,File *file) {
 EFI_BOOT_SERVICES *bs=st->boot_services;
 for(UINTN k=0;k<sizeof(network_guids)/sizeof(network_guids[0]);k++) {
  EFI_HANDLE *handles=0;UINTN count=0;
  EFI_STATUS s=((LocateHandles)bs->locate_handle_buffer)(2,&network_guids[k].guid,0,&count,&handles);
  text("NET_LOCATE ");text(network_guids[k].name);number(" status=",s);number(" count=",count);text("\n");save(file);
  if(EFI_ERROR(s) || !handles)continue;
  if(count>32){text("NET_HANDLE_LIMIT\n");((FreePool)bs->free_pool)(handles);continue;}
  for(UINTN i=0;i<count;i++) {
   void *interface=0;U8 *path=0;
   s=bs->handle_protocol(handles[i],&network_guids[k].guid,&interface);
   text("NET_INTERFACE ");text(network_guids[k].name);number(" index=",i);number(" status=",s);
   if(!EFI_ERROR(s) && interface && k==0) {
    Network *nic=interface;number(" revision=",nic->revision);
    if(nic->mode){NetworkMode *m=nic->mode;number(" state=",m->state);number(" media_supported=",m->media_supported);
     number(" media=",m->media_present);number(" nv_size=",m->nv_size);text(" mac=");
     if(m->address_size<=32)for(U32 j=0;j<m->address_size;j++)hex(m->current[j],2);
    }
   } else if(!EFI_ERROR(s) && interface && k==1) {
    Pxe *pxe=interface;number(" revision=",pxe->revision);
    if(pxe->mode){number(" started=",pxe->mode[0]);number(" using_ipv6=",pxe->mode[3]);}
   }
   s=bs->handle_protocol(handles[i],&path_guid,(void **)&path);
   number(" path_status=",s);if(!EFI_ERROR(s))network_path(path);
   text("\n");save(file);
  }
  ((FreePool)bs->free_pool)(handles);
 }
 text("NETWORK_INVENTORY_COMPLETE\n");save(file);
}
_Static_assert(__builtin_offsetof(Network,mode)==120,"SNP mode ABI");
_Static_assert(__builtin_offsetof(NetworkMode,current)==552,"SNP MAC ABI");
_Static_assert(__builtin_offsetof(Pxe,mode)==104,"PXE mode ABI");

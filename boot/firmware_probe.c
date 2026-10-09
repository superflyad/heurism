/* Read-only firmware inventory. Only writes its report to the ESP, then starts GRUB. */
#include "uefi.h"

typedef struct File File;
struct File {
    U64 revision;
    EFI_STATUS (EFIAPI *open)(File *, File **, const U16 *, U64, U64);
    EFI_STATUS (EFIAPI *close)(File *);
    void *delete_file, *read;
    EFI_STATUS (EFIAPI *write)(File *, UINTN *, void *);
    void *get_position;
    EFI_STATUS (EFIAPI *set_position)(File *, U64);
    void *get_info, *set_info;
    EFI_STATUS (EFIAPI *flush)(File *);
};
typedef struct Fs {
    U64 revision;
    EFI_STATUS (EFIAPI *open_volume)(struct Fs *, File **);
} Fs;
typedef struct { U32 revision; EFI_HANDLE parent; EFI_SYSTEM_TABLE *st; EFI_HANDLE device; } Loaded;
typedef struct Fv Fv;
struct Fv {
    EFI_STATUS (EFIAPI *attributes)(Fv *, U64 *);
    void *set_attributes, *read_file, *read_section, *write_file;
    EFI_STATUS (EFIAPI *next)(Fv *, void *, U8 *, EFI_GUID *, U32 *, UINTN *);
    U32 key_size;
    EFI_HANDLE parent;
};
typedef struct Fvb Fvb;
struct Fvb {
    EFI_STATUS (EFIAPI *attributes)(Fvb *, U32 *);
    void *set_attributes;
    EFI_STATUS (EFIAPI *physical)(Fvb *, U64 *);
    void *block_size;
    EFI_STATUS (EFIAPI *read)(Fvb *, U64, UINTN, UINTN *, U8 *);
    void *write, *erase;
    EFI_HANDLE parent;
};
typedef struct { EFI_GUID guid; void *table; } Config;
typedef EFI_STATUS (EFIAPI *LocateHandles)(U32, EFI_GUID *, void *, UINTN *, EFI_HANDLE **);
typedef EFI_STATUS (EFIAPI *FreePool)(void *);
typedef EFI_STATUS (EFIAPI *LoadImage)(U8, EFI_HANDLE, void *, void *, UINTN, EFI_HANDLE *);
typedef EFI_STATUS (EFIAPI *StartImage)(EFI_HANDLE, UINTN *, U16 **);
static EFI_GUID loaded_guid = {0x5b1b31a1,0x9562,0x11d2,{0x8e,0x3f,0,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_GUID fs_guid = {0x964e5b22,0x6459,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_GUID path_guid = {0x09576e91,0x6d3f,0x11d2,{0x8e,0x39,0,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_GUID fv_guid = {0x220e73b6,0x6bdb,0x4413,{0x84,0x05,0xb9,0x74,0xb1,0x08,0x61,0x9a}};
static EFI_GUID fvb_guid = {0x8f644fa9,0xe850,0x4db1,{0x9c,0xe2,0x0b,0x44,0x69,0x8e,0x8d,0xa4}};
static EFI_GUID hob_guid = {0x7739f24c,0x93d7,0x11d4,{0x9a,0x3a,0,0x90,0x27,0x3f,0xc1,0x4d}};
static EFI_GUID *volatile relocation_anchor = &fv_guid;
#ifndef PROBE_OUTPUT_SIZE
#define PROBE_OUTPUT_SIZE 32768
#endif
static char output[PROBE_OUTPUT_SIZE];
static UINTN used;
static U8 key[1024], path_buffer[4096];
#ifndef PROBE_GRUB_PATH
#define PROBE_GRUB_PATH u"\\EFI\\alpine\\grubx64.efi"
#endif
#ifndef PROBE_REPORT_PATH
#define PROBE_REPORT_PATH u"\\EFI\\companion\\fv-probe-01.txt"
#endif
#ifndef PROBE_TAG
#define PROBE_TAG "COMPANION_FV_PROBE_01"
#endif
#ifndef PROBE_DESCRIPTION
#define PROBE_DESCRIPTION "Firmware read-only; ESP report only.\n"
#endif
static U16 grub_path[] = PROBE_GRUB_PATH;
static U16 report_path[] = PROBE_REPORT_PATH;
static U16 message[] = u"Companion: collecting firmware inventory, then starting management.\r\n";
static void text(const char *s) { while (*s && used + 1 < sizeof(output)) output[used++] = *s++; }
static void hex(U64 value, U32 digits) {
    const char *map = "0123456789abcdef";
    while (digits && used + 1 < sizeof(output)) output[used++] = map[(value >> (--digits * 4)) & 15];
}
static U16 u16(const U8 *p) { return (U16)(p[0] | ((U16)p[1] << 8)); }
static U32 u32(const U8 *p) { return (U32)u16(p) | ((U32)u16(p + 2) << 16); }
static U64 u64(const U8 *p) { return (U64)u32(p) | ((U64)u32(p + 4) << 32); }
static void guid(const U8 *p) {
    hex(u32(p),8); text("-"); hex(u16(p+4),4); text("-"); hex(u16(p+6),4);
    text("-"); hex(p[8],2); hex(p[9],2); text("-");
    for (U32 i=10;i<16;i++) hex(p[i],2);
}
static U8 equal(const EFI_GUID *a, const EFI_GUID *b) {
    const U8 *x=(const U8 *)a, *y=(const U8 *)b;
    for (U32 i=0;i<16;i++) if(x[i]!=y[i]) return 0;
    return 1;
}
static void number(const char *label, U64 n) { text(label); text("0x"); hex(n,16); }
static void save(File *file) {
    if (!file) return;
    UINTN count=used;
    if (!EFI_ERROR(file->set_position(file,0))) {
        EFI_STATUS result=file->write(file,&count,output);
        if (!EFI_ERROR(result) && count==used) file->flush(file);
    }
}
static void hobs(EFI_SYSTEM_TABLE *st) {
    Config *tables=(Config *)st->configuration_table;
    if (st->table_entries>1024) { text("CONFIG_LIMIT\n"); return; }
    for (UINTN i=0;i<st->table_entries;i++) {
        text("CONFIG "); guid((U8 *)&tables[i].guid); text("\n");
        if (!equal(&tables[i].guid,&hob_guid)) continue;
        U8 *p=tables[i].table;
        if (!p || u16(p)!=1 || u16(p+2)<56) { text("HOB_BAD_HEADER\n"); continue; }
        U64 end=u64(p+48), start=(U64)p;
        number("HOB_BOOT_MODE ",u32(p+12)); text("\n");
        if (end<start || end-start>0x100000) { text("HOB_BAD_RANGE\n"); continue; }
        for (U32 n=0;n<4096 && (U64)p<=end;n++) {
            U16 type=u16(p), length=u16(p+2);
            if(type==0xffff) {text("HOB_END\n"); break;}
            if(length<8 || (length&7) || (U64)p+length>end) {text("HOB_BAD_LENGTH\n"); break;}
            if ((type==5 || type==9 || type==12) && length>=24) {
                number("HOB_FV type=",type); number(" base=",u64(p+8)); number(" size=",u64(p+16));
                if(type==12 && length>=32) number(" auth=",u32(p+24));
                text("\n");
            } else if(type==4 && length>=24) {
                text("HOB_GUID "); guid(p+8); number(" bytes=",length-24); text("\n");
            }
            p+=length;
        }
    }
}
static void volumes(EFI_SYSTEM_TABLE *st, File *file) {
    EFI_BOOT_SERVICES *bs=st->boot_services;
    EFI_HANDLE *handles=0; UINTN count=0;
    EFI_STATUS result=((LocateHandles)bs->locate_handle_buffer)(2,relocation_anchor,0,&count,&handles);
    number("FV_LOCATE status=",result); number(" count=",count); text("\n");
    if(EFI_ERROR(result) || !handles || count>128) return;
    for(UINTN i=0;i<count;i++) {
        Fv *fv=0; Fvb *fvb=0; U64 attrs=0;
        result=bs->handle_protocol(handles[i],&fv_guid,(void **)&fv);
        number("FV index=",i); number(" interface_status=",result);
        if(EFI_ERROR(result) || !fv) {text("\n"); continue;}
        result=fv->attributes(fv,&attrs);
        number(" attr_status=",result); number(" attrs=",attrs); number(" key_size=",fv->key_size); text("\n");
        result=bs->handle_protocol(handles[i],&fvb_guid,(void **)&fvb);
        number("FVB interface_status=",result);
        if(!EFI_ERROR(result) && fvb) {
            U64 base=0; U32 block_attrs=0; U8 header[128]; UINTN bytes=sizeof(header);
            result=fvb->physical(fvb,&base); number(" physical_status=",result); number(" base=",base);
            result=fvb->attributes(fvb,&block_attrs); number(" attr_status=",result); number(" attrs=",block_attrs);
            result=fvb->read(fvb,0,0,&bytes,header); number(" header_status=",result); number(" header_bytes=",bytes);
            if(!EFI_ERROR(result) && bytes>=56 && u32(header+40)==0x4856465f) {
                number(" size=",u64(header+32)); text(" format="); guid(header+16);
                U16 ext=u16(header+52);
                if(ext && (UINTN)ext+20<=bytes) {text(" name="); guid(header+ext);}
            }
        }
        text("\n");
        if(fv->key_size<=sizeof(key)) {
            for(UINTN k=0;k<sizeof(key);k++) key[k]=0;
            U32 files=0, attrs_file=0; U8 type; EFI_GUID name; UINTN bytes;
            do {
                type=0; bytes=0;
                result=fv->next(fv,key,&type,&name,&attrs_file,&bytes);
                if(EFI_ERROR(result)) break;
                files++;
            } while(files<1024);
            number("FV_FILES count=",files); number(" stop_status=",result); text("\n");
        } else text("FV_FILES KEY_LIMIT\n");
        save(file);
    }
    ((FreePool)bs->free_pool)(handles);
}
static EFI_STATUS chainload(EFI_HANDLE image, Loaded *loaded, EFI_SYSTEM_TABLE *st) {
    U8 *source=0; UINTN at=0; EFI_BOOT_SERVICES *bs=st->boot_services;
    EFI_STATUS result=bs->handle_protocol(loaded->device,&path_guid,(void **)&source);
    if(EFI_ERROR(result) || !source) return EFI_UNSUPPORTED;
    for(U32 nodes=0;nodes<128;nodes++) {
        if(at+4>sizeof(path_buffer)) return EFI_UNSUPPORTED;
        U16 size=u16(source+at+2);
        if(size<4 || at+size>sizeof(path_buffer)-sizeof(grub_path)-8) return EFI_UNSUPPORTED;
        if(source[at]==0x7f) {
            if(source[at+1]!=0xff || size!=4) return EFI_UNSUPPORTED;
            break;
        }
        for(U16 j=0;j<size;j++) path_buffer[at+j]=source[at+j];
        at+=size;
    }
    if(source[at]!=0x7f || source[at+1]!=0xff) return EFI_UNSUPPORTED;
    U16 size=(U16)(4+sizeof(grub_path));
    path_buffer[at]=4; path_buffer[at+1]=4; path_buffer[at+2]=(U8)size; path_buffer[at+3]=(U8)(size>>8);
    for(UINTN j=0;j<sizeof(grub_path);j++) path_buffer[at+4+j]=((U8 *)grub_path)[j];
    at+=size;
    path_buffer[at]=0x7f; path_buffer[at+1]=0xff; path_buffer[at+2]=4; path_buffer[at+3]=0;
    EFI_HANDLE child=0;
    result=((LoadImage)bs->load_image)(0,image,path_buffer,0,0,&child);
    if(EFI_ERROR(result)) return result;
    return ((StartImage)bs->start_image)(child,0,0);
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    if(!st || !st->boot_services) return EFI_UNSUPPORTED;
    EFI_BOOT_SERVICES *bs=st->boot_services;
    bs->set_watchdog_timer(60,0,0,0);
    if(st->con_out) st->con_out->output_string(st->con_out,message);
    used=0;
    text(PROBE_TAG "\n" PROBE_DESCRIPTION);
    Loaded *loaded=0; Fs *fs=0; File *directory=0, *file=0;
    EFI_STATUS result=bs->handle_protocol(image,&loaded_guid,(void **)&loaded);
    if(EFI_ERROR(result) || !loaded) return result;
    result=bs->handle_protocol(loaded->device,&fs_guid,(void **)&fs);
    if(!EFI_ERROR(result) && fs && !EFI_ERROR(fs->open_volume(fs,&directory))) {
        result=directory->open(directory,&file,report_path,0x8000000000000003ULL,0);
        if(EFI_ERROR(result)) file=0;
    }
    save(file);
    hobs(st);
    save(file);
    volumes(st,file);
#ifdef PROBE_EXTENSION
    PROBE_EXTENSION(st,file);
#endif
    text("PROBE_COMPLETE\n"); save(file);
    if(file) file->close(file);
    if(directory) directory->close(directory);
    bs->set_watchdog_timer(0,0,0,0);
    /* A network-loaded probe returns to Boot Manager; appending a disk path
       to its network device could request the same PXE image recursively. */
    if(!directory) return 0;
    return chainload(image,loaded,st);
}
_Static_assert(__builtin_offsetof(Loaded,device)==24,"loaded image ABI");
_Static_assert(__builtin_offsetof(File,write)==40,"file write ABI");
_Static_assert(__builtin_offsetof(Fv,parent)==56,"FV parent ABI");
_Static_assert(__builtin_offsetof(Fvb,read)==32,"FVB read ABI");

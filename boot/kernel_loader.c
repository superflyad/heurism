#include "kernel_uefi.h"
#include "kernel_handoff.h"
#include "elf.h"
#include "../ui/framebuffer.h"
#include "../platform/x86_64/io.h"
#ifndef COMPANION_VM_DIAGNOSTICS
#define COMPANION_VM_DIAGNOSTICS 0
#endif
#ifndef COMPANION_STALE_MAP_TEST
#define COMPANION_STALE_MAP_TEST 0
#endif
#ifndef COMPANION_KERNEL_TEST_FLAGS
#define COMPANION_KERNEL_TEST_FLAGS 0
#endif
#ifndef COMPANION_I8042
#define COMPANION_I8042 0
#endif
#ifndef COMPANION_VM_NETWORK
#define COMPANION_VM_NETWORK 0
#endif
#ifndef COMPANION_VM_USB
#define COMPANION_VM_USB 0
#endif
#ifndef COMPANION_VM_USB_NETWORK
#define COMPANION_VM_USB_NETWORK 0
#endif
static EFI_GUID loaded_guid={0x5b1b31a1,0x9562,0x11d2,{0x8e,0x3f,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_GUID fs_guid={0x964e5b22,0x6459,0x11d2,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_GUID file_info_guid={0x09576e92,0x6d3f,0x11d2,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_GUID gop_guid={0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}};
static EFI_GUID acpi_guid={0x8868e871,0xe4f1,0x11d3,{0xbc,0x22,0x00,0x80,0xc7,0x3c,0x88,0x81}};
static EFI_GUID *volatile reloc_anchor=&loaded_guid;
extern void EFIAPI kernel_jump(U64, BootInfo *, U64) __attribute__((noreturn));
static void zero(void *pointer,U64 size) { U8 *p=pointer; while (size--) *p++=0; }
static void copy(void *dest,const void *source,U64 size) { U8 *d=dest; const U8 *s=source; while (size--) *d++=*s++; }
static int equal(const void *a,const void *b,U64 size) { const U8 *x=a,*y=b; while(size--) if(*x++!=*y++) return 0; return 1; }
static int checksum(const U8 *p,U32 size) { U8 sum=0; while(size--) sum+=*p++; return sum==0; }
static void message(EFI_SYSTEM_TABLE *st,const U16 *s) {
    if(st->con_out && st->con_out->output_string) st->con_out->output_string(st->con_out,s);
}
static void reserve(BootInfo *b,U64 base,U64 size) { b->reserved[b->reserved_count++]=(MemoryRange){base,size}; }
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    EFI_BOOT_SERVICES *bs; LoadedImage *loaded=0; SimpleFs *fs=0; EfiFile *volume=0,*file=0;
    EFI_GOP *gop=0; Framebuffer fb; ElfPlan plan; BootInfo *info=0;
    void *bytes=0,*map=0; U64 pages_base=0,stack=0; UINTN length=0; U32 attempts=0;
    U64 file_metadata[64]; UINTN metadata_size=sizeof(file_metadata); EFI_STATUS status=EFI_LOAD_ERROR;
    int pages_allocated=0,stack_allocated=0;
    if(!st || !st->boot_services) return EFI_UNSUPPORTED;
    bs=st->boot_services;
    if(COMPANION_VM_DIAGNOSTICS) serial_init();
    serial_write("LOADER_ENTER\n");
    if(!bs->allocate_pages || !bs->allocate_pool || !bs->free_pool || !bs->free_pages ||
       !bs->get_memory_map || !bs->exit_boot_services || !bs->handle_protocol || !bs->set_watchdog_timer) return EFI_UNSUPPORTED;
    status=bs->set_watchdog_timer(0,0,0,0); if(EFI_ERROR(status)) return status;
    status=bs->handle_protocol(image,reloc_anchor,(void **)&loaded);
    if(EFI_ERROR(status) || !loaded || !loaded->device) goto failed;
    status=bs->handle_protocol(loaded->device,&fs_guid,(void **)&fs);
    if(EFI_ERROR(status) || !fs || !fs->open_volume) goto failed;
    status=fs->open_volume(fs,&volume); if(EFI_ERROR(status) || !volume) goto failed;
    status=volume->open(volume,&file,(const U16 *)u"\\EFI\\Companion\\kernel.elf",1,0);
    if(EFI_ERROR(status) || !file) goto failed;
    status=file->get_info(file,&file_info_guid,&metadata_size,file_metadata);
    if(EFI_ERROR(status) || metadata_size<80 || file_metadata[0]<80 || file_metadata[0]>metadata_size ||
       file_metadata[1]<64 || file_metadata[1]>16*1024*1024) goto failed;
    length=file_metadata[1];
    status=((AllocatePoolFn)bs->allocate_pool)(2,length,&bytes); if(EFI_ERROR(status)) goto failed;
    status=file->read(file,&length,bytes); if(EFI_ERROR(status) || length!=file_metadata[1]) goto failed;
    file->close(file); file=0; volume->close(volume); volume=0;
    if(!elf_plan(bytes,length,&plan)) { status=EFI_LOAD_ERROR; goto failed; }
    serial_write("ELF_VALIDATED\n");
    pages_base=plan.base;
    status=((AllocatePagesFn)bs->allocate_pages)(2,2,plan.size/4096,&pages_base);
    if(EFI_ERROR(status)) goto failed;
    pages_allocated=1;
    zero((void *)pages_base,plan.size);
    for(U32 i=0;i<plan.count;++i) copy((void *)plan.segments[i].address,(U8 *)bytes+plan.segments[i].offset,plan.segments[i].file_size);
    ((FreePoolFn)bs->free_pool)(bytes); bytes=0;
    status=bs->handle_protocol(st->console_out_handle,&gop_guid,(void **)&gop);
    if(EFI_ERROR(status) || !gop) status=bs->locate_protocol(&gop_guid,0,(void **)&gop);
    if(EFI_ERROR(status) || !gop || !framebuffer_init(&fb,gop->mode)) { status=EFI_UNSUPPORTED; goto failed; }
    status=((AllocatePoolFn)bs->allocate_pool)(2,sizeof(*info),(void **)&info); if(EFI_ERROR(status)) goto failed;
    zero(info,sizeof(*info));
    info->magic=COMPANION_BOOT_MAGIC; info->version=COMPANION_BOOT_VERSION; info->size=sizeof(*info);
    info->flags=(COMPANION_VM_DIAGNOSTICS ? COMPANION_BOOT_VM_SERIAL : 0)|COMPANION_KERNEL_TEST_FLAGS|
                (COMPANION_I8042 ? COMPANION_BOOT_I8042 : 0)|(COMPANION_VM_NETWORK ? COMPANION_BOOT_VM_NETWORK : 0)|
                (COMPANION_VM_USB ? COMPANION_BOOT_VM_USB : 0)|
                (COMPANION_VM_USB_NETWORK ? COMPANION_BOOT_VM_USB_NETWORK : 0);
    info->framebuffer=(BootFramebuffer){(U64)fb.pixels,fb.size,fb.width,fb.height,fb.stride,fb.format};
    status=((AllocatePagesFn)bs->allocate_pages)(0,2,32,&stack); if(EFI_ERROR(status)) goto failed;
    stack_allocated=1; info->stack_base=stack; info->stack_size=32*4096;
    status=((AllocatePoolFn)bs->allocate_pool)(2,256*1024,&map); if(EFI_ERROR(status)) goto failed;
    info->memory_map=(U64)map;
    reserve(info,plan.base,plan.size); reserve(info,stack,info->stack_size);
    reserve(info,(U64)map,256*1024); reserve(info,(U64)info,sizeof(*info));
    reserve(info,(U64)loaded->image_base,loaded->image_size);
    if(st->configuration_table && st->table_entries<=1024) {
        const EfiConfigurationTable *tables=st->configuration_table;
        for(UINTN i=0;i<st->table_entries;++i) if(equal(&tables[i].guid,&acpi_guid,16) && tables[i].table) {
            const U8 *rsdp=tables[i].table; U32 size;
            if(!equal(rsdp,"RSD PTR ",8) || !checksum(rsdp,20) || rsdp[15]<2) break;
            copy(&size,rsdp+20,4);
            if(size<36 || size>4096 || !checksum(rsdp,size)) break;
            info->acpi_rsdp=(U64)rsdp; break;
        }
    }
    framebuffer_rect(&fb,0,0,fb.width,fb.height,0x101820);
    framebuffer_text(&fb,fb.width/4,fb.height/3,4,"COMPANION",0xf0f5f3);
    framebuffer_text(&fb,fb.width/4,fb.height/3+48,2,"STARTING NATIVE KERNEL",0x69e0bb);
    serial_write("EXIT_BOOT_SERVICES_BEGIN\n");
    status=kernel_exit_boot_services(bs,image,info,256*1024,&attempts,COMPANION_STALE_MAP_TEST);
    /* Never return to firmware or use console/protocol/free services after this point. */
    if(EFI_ERROR(status)) {
        serial_write("EXIT_BOOT_SERVICES_FAILED "); serial_hex(status); serial_write("\n");
        framebuffer_text(&fb,fb.width/4,fb.height/3+80,2,"HANDOFF FAILED",0xff7777);
        for(;;) cpu_halt();
    }
    serial_write("EXIT_BOOT_SERVICES_OK attempts="); serial_hex(attempts); serial_write("\n");
    kernel_jump(plan.entry,info,stack+info->stack_size);
failed:
    if(file) file->close(file);
    if(volume) volume->close(volume);
    if(bytes) ((FreePoolFn)bs->free_pool)(bytes);
    if(map) ((FreePoolFn)bs->free_pool)(map);
    if(info) ((FreePoolFn)bs->free_pool)(info);
    if(stack_allocated) ((FreePagesFn)bs->free_pages)(stack,32);
    if(pages_allocated) ((FreePagesFn)bs->free_pages)(pages_base,plan.size/4096);
    serial_write("LOADER_REJECTED "); serial_hex(status); serial_write("\n");
    message(st,(const U16 *)u"Companion kernel could not be loaded. Returning to firmware.\r\n");
    return EFI_ERROR(status)?status:EFI_LOAD_ERROR;
}

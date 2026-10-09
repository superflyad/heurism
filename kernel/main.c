#include "boot_info.h"
#include "pages.h"
#include "../ui/drawing.h"
#include "../platform/x86_64/io.h"
#include "../platform/x86_64/interrupts.h"
#include "../platform/x86_64/timer.h"
#include "../platform/x86_64/paging.h"
#include "../platform/x86_64/acpi.h"
#include "../platform/x86_64/acpi_memory.h"
#include "../platform/x86_64/pci.h"
#include "../drivers/input/i8042.h"
#include "vm_network.h"
#include "vm_usb.h"
static Framebuffer screen;
static AcpiInventory hardware;
static PciInventory devices;
static I8042Keyboard keyboard;
static U8 keyboard_read(void *context,U16 port) { (void)context;return in8(port); }
static void keyboard_write(void *context,U16 port,U8 byte) { (void)context;out8(port,byte); }
static U32 input_x,input_y,typed_length;
static char typed[64];
static void poll_keyboard(void) {
    KeyEvent event;
    for(U32 i=0;i<32 && i8042_poll(&keyboard,&event);++i) {
        serial_write("KEY_EVENT code=");serial_hex(event.code);serial_write(" pressed=");serial_hex(event.pressed);
        serial_write(" character=");serial_hex(event.character);serial_write("\n");
        if(!event.pressed || !event.character) continue;
        if(event.character==8) { if(typed_length) --typed_length; }
        else if(event.character==13 || event.character==27) typed_length=0;
        else if(event.character>=32 && event.character<127 && typed_length<sizeof(typed)-1) typed[typed_length++]=(char)event.character;
        typed[typed_length]=0;
        framebuffer_rect(&screen,input_x,input_y+20,screen.width-input_x,20,0x101820);
        framebuffer_text(&screen,input_x,input_y+20,2,typed,0xf0f5f3);
        serial_write("KERNEL_INPUT_TEXT ");serial_write(typed);serial_write("\n");
    }
}
static int pci_read(void *context,U64 address,U32 *value) {
    const AcpiInventory *a=context;
    if(address&3) return 0;
    for(U32 i=0;i<a->ecam_count;++i) {
        const AcpiEcam *e=&a->ecam[i];
        U64 begin=e->base+((U64)e->first_bus<<20),end=e->base+(((U64)e->last_bus+1)<<20);
        if(address>=begin && address<end && end-address>=4) {
            *value=*(volatile U32 *)address;return 1;
        }
    }
    return 0;
}
static void discover_pci(const BootInfo *boot) {
    for(U32 i=0;i<hardware.ecam_count;++i) {
        const AcpiEcam *e=&hardware.ecam[i];
        if(!paging_device_range(boot,e->base+((U64)e->first_bus<<20),((U64)e->last_bus-e->first_bus+1)<<20)) {
            serial_write("KERNEL_PCI_MAPPING_REJECTED\n");return;
        }
    }
    if(!pci_inventory(&hardware,pci_read,&hardware,&devices)) { serial_write("KERNEL_PCI_REJECTED\n");return; }
    serial_write("KERNEL_PCI_READY devices=");serial_hex(devices.count);serial_write(" reads=");serial_hex(devices.reads);serial_write("\n");
    for(U32 i=0;i<devices.count;++i) {
        const PciDevice *d=&devices.devices[i];
        serial_write("PCI_DEVICE segment=");serial_hex(d->segment);serial_write(" bus=");serial_hex(d->bus);
        serial_write(" slot=");serial_hex(d->slot);serial_write(" function=");serial_hex(d->function);
        serial_write(" vendor=");serial_hex(d->vendor);serial_write(" device=");serial_hex(d->device);
        serial_write(" class=");serial_hex(d->class_code);serial_write(" subclass=");serial_hex(d->subclass);serial_write("\n");
    }
}
void kernel_exception(const InterruptFrame *frame) {
    serial_write("KERNEL_EXCEPTION vector="); serial_hex(frame->vector);
    serial_write(" error=");serial_hex(frame->error);serial_write(" rip=");serial_hex(frame->rip);serial_write("\n");
    if(frame->vector==14) { U64 address;__asm__ volatile("mov %%cr2,%0":"=r"(address));serial_write("KERNEL_PAGEFAULT address=");serial_hex(address);serial_write("\n"); }
    framebuffer_text(&screen,24,24,2,"KERNEL EXCEPTION",0xff7777);
    for(;;) cpu_halt();
}
static void stop(const char *why) {
    serial_write("KERNEL_FATAL "); serial_write(why); serial_write("\n");
    for(;;) cpu_halt();
}
static int framebuffer_valid(const BootFramebuffer *f) {
    U64 pixels=(U64)f->stride*f->height;
    return f->base && !(f->base&3) && f->width && f->height && f->stride>=f->width && f->format<=1 &&
           pixels<=~0ULL/4 && pixels*4<=f->size && f->base<=~0ULL-f->size;
}
void kernel_main(const BootInfo *boot) {
    U64 count=0; char number[17]; const char *digits="0123456789ABCDEF";
    if(!boot || boot->magic!=COMPANION_BOOT_MAGIC || boot->version!=COMPANION_BOOT_VERSION || boot->size!=sizeof(*boot)) stop("BOOT_CONTRACT");
    if(boot->flags&COMPANION_BOOT_VM_SERIAL) serial_init();
    serial_write("KERNEL_ENTER firmware_services=off\n");
    if(!framebuffer_valid(&boot->framebuffer)) stop("FRAMEBUFFER");
    if(!boot->memory_map || boot->descriptor_size<40 || boot->descriptor_size>256 ||
       !boot->memory_map_size || boot->memory_map_size%boot->descriptor_size ||
       boot->reserved_count>COMPANION_MAX_RESERVED) stop("MEMORY_MAP");
    screen=(Framebuffer){(volatile U32 *)boot->framebuffer.base,boot->framebuffer.width,boot->framebuffer.height,
                         boot->framebuffer.stride,boot->framebuffer.format,boot->framebuffer.size};
    interrupts_init(boot->stack_base+boot->stack_size);
    serial_write("KERNEL_EXCEPTIONS_READY\n");
    if(!paging_init(boot)) stop("PAGE_TABLES");
    serial_write("KERNEL_PAGE_TABLES_READY wx=separated\n");
    if(!pages_init(boot)) stop("PAGE_ALLOCATOR");
    U64 before=pages_available(),page=page_alloc();
    if(!page || pages_available()!=before-1) stop("PAGE_ALLOCATE");
    volatile U64 *memory=(volatile U64 *)page; *memory=0x434f4d50414e494f;
    if(*memory!=0x434f4d50414e494f || !page_free(page) || pages_available()!=before || page_free(page)) stop("PAGE_FREE");
    serial_write("KERNEL_PAGES_READY available=");serial_hex(before);serial_write("\n");
    int acpi=acpi_inventory(boot->acpi_rsdp,acpi_memory_read,(void *)boot,&hardware);
    if(acpi==1) {
        serial_write("KERNEL_ACPI_READY tables=");serial_hex(hardware.table_count);
        serial_write(" processors=");serial_hex(hardware.processor_count);
        serial_write(" io_apics=");serial_hex(hardware.io_apic_count);
        serial_write(" ecam_regions=");serial_hex(hardware.ecam_count);serial_write("\n");
        for(U32 i=0;i<hardware.table_count;++i) {
            serial_write("ACPI_TABLE ");serial_write(hardware.tables[i].signature);
            serial_write(" length=");serial_hex(hardware.tables[i].length);serial_write("\n");
        }
        discover_pci(boot);
    } else serial_write(acpi==0?"KERNEL_ACPI_ABSENT\n":"KERNEL_ACPI_REJECTED\n");
    U32 scale=screen.width>=960 && screen.height>=600?5:2;
    U32 x=screen.width>54*scale?(screen.width-54*scale)/2:0, y=screen.height/3;
    framebuffer_rect(&screen,0,0,screen.width,screen.height,0x101820);
    framebuffer_rect(&screen,x,y-20,54*scale,3,0x69e0bb);
    framebuffer_text(&screen,x,y,scale,"COMPANION",0xf0f5f3);
    framebuffer_text(&screen,x,y+10*scale,2,"NATIVE KERNEL",0x69e0bb);
    framebuffer_text(&screen,x,y+10*scale+28,1,"FIRMWARE HANDOFF COMPLETE",0xa8bac4);
    serial_write("KERNEL_FRAMEBUFFER_READY\n");
    if(boot->flags&COMPANION_BOOT_TEST_UD2) __asm__ volatile("ud2");
    if(boot->flags&COMPANION_BOOT_TEST_PAGEFAULT) *(volatile U8 *)__text_start=0;
    if(!timer_start()) stop("APIC_TIMER");
    serial_write("KERNEL_TIMER_READY hz=approximately100\n");
    vm_network_start(boot,&devices);
    vm_usb_start(boot,&devices);
    if(boot->flags&COMPANION_BOOT_I8042) {
        I8042Io io={0,keyboard_read,keyboard_write};
        if(i8042_start(&keyboard,&io)) {
            input_x=x;input_y=y+10*scale+90;
            framebuffer_text(&screen,input_x,input_y,1,"INPUT READY | TYPE TO TEST",0x69e0bb);
            serial_write("KERNEL_INPUT_READY i8042=translated-set1 polling=timer\n");
        } else serial_write("KERNEL_INPUT_UNAVAILABLE\n");
    }
    for(;;) {
        for(int i=0;i<16;++i) number[i]=digits[(count>>((15-i)*4))&15]; number[16]=0;
        framebuffer_rect(&screen,x,y+10*scale+52,200,20,0x101820);
        framebuffer_text(&screen,x,y+10*scale+52,2,number,0xf0f5f3);
        serial_write("KERNEL_PROGRESS "); serial_hex(count++); serial_write("\n");
        U64 target=timer_ticks()+50;
        do { __asm__ volatile("sti; hlt; cli":::"memory");poll_keyboard();vm_network_poll();vm_usb_poll(); } while(timer_ticks()<target);
    }
}

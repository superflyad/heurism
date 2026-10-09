#include "vm_usb.h"
#ifndef COMPANION_VM_USB
#define COMPANION_VM_USB 0
#endif
#if COMPANION_VM_USB
#include "../drivers/usb/xhci.h"
#include "../drivers/net/usb_ecm.h"
#include "vm_management.h"
#include "../platform/x86_64/paging.h"
#include "../platform/x86_64/io.h"
#include "pages.h"
#include "../platform/x86_64/timer.h"
static Xhci controller;
typedef struct { U64 base;volatile U16 *command; } ControllerAccess;
static ControllerAccess access;
#if COMPANION_VM_USB_NETWORK
static Network network;
static U8 network_ready,received[USB_ECM_TRANSFER_SIZE],response[1514];
static U8 discard_frame;
static U64 network_received,network_transmitted;
#endif
static U32 read_register(void *context,U32 offset) { return *(volatile U32 *)(((ControllerAccess *)context)->base+offset); }
static void write_register(void *context,U32 offset,U32 value) { *(volatile U32 *)(((ControllerAccess *)context)->base+offset)=value; }
static U64 allocate_page(void *context) { (void)context;return page_alloc(); }
static void release_page(void *context,U64 page) { (void)context;(void)page_free(page); }
static void dma_fence(void *context) { (void)context;__asm__ volatile("mfence":::"memory"); }
static void bus_master(void *context,U8 enable) {
    volatile U16 *command=((ControllerAccess *)context)->command;
    *command=enable?*command|4U:*command&~4U;
}
void vm_usb_start(const BootInfo *boot,const PciInventory *devices) {
    if(!boot || !devices || !(boot->flags&COMPANION_BOOT_VM_USB) || !(boot->flags&COMPANION_BOOT_VM_SERIAL)) return;
    for(U32 i=0;i<devices->count;++i) {
        const PciDevice *d=&devices->devices[i];
        /* QEMU's known 16-KiB aperture only. Intel's physical controller needs
         * its own validated profile, ownership and IOMMU work. */
        if(d->vendor!=0x1b36 || d->device!=0x000d || d->class_code!=0x0c || d->subclass!=3 ||
           d->programming_interface!=0x30 || (d->header_type&127)!=0) continue;
        U32 type=d->bars[0]&15;U64 base=d->bars[0]&~15U;
        if(type==4) base|=(U64)d->bars[1]<<32;
        serial_write("KERNEL_USB_BAR raw=");serial_hex(d->bars[0]);serial_write(" high=");serial_hex(d->bars[1]);serial_write("\n");
        if((type!=0 && type!=4) || !base || (base&(XHCI_WINDOW-1)) || !paging_device_range(boot,base,XHCI_WINDOW)) {
            serial_write("KERNEL_USB_MAPPING_REJECTED\n");return;
        }
        volatile U16 *pci_command=(volatile U16 *)(d->configuration+4);
        *pci_command=(*pci_command|2U|(1U<<10))&~4U;
        access=(ControllerAccess){base,pci_command};
        XhciIo io={&access,read_register,write_register,allocate_page,release_page,dma_fence,bus_master,1000000};
        /* The driver enables bus mastering only after owned rings are ready. */
        U64 available=pages_available();
        if(!xhci_start(&controller,&io)) { serial_write("KERNEL_USB_DRIVER_FAILED\n");return; }
        serial_write("KERNEL_USB_READY xhci vm_fixture_only ports=");serial_hex(controller.ports);serial_write("\n");
        int good=1;U32 count=0;
        for(U32 n=0;n<160 && good;++n) good=xhci_noop(&controller);
        for(U32 port=1;port<=controller.ports && good;++port) {
            UsbDeviceInfo info;
#if COMPANION_VM_USB_NETWORK
            int result=xhci_open_port(&controller,port,&info);
            if(result==1 && info.vendor==0x525 && info.product==0xa4a2 && (boot->flags&COMPANION_BOOT_VM_USB_NETWORK)) {
                if(!usb_ecm_start(&controller,&info,network.mac) || !vm_management_init(&network)) { good=0;break; }
                network_ready=1;
                serial_write("KERNEL_NETWORK_READY usb_ecm ip=10.0.2.15 port=47333 vm_fixture_only\n");return;
            }
            if(result==1 && !xhci_close_port(&controller)) result=-1;
#else
            int result=xhci_inspect_port(&controller,port,&info);
#endif
            if(result<0) { good=0;break; }if(!result) continue;
            ++count;serial_write("USB_DEVICE port=");serial_hex(info.port);serial_write(" speed=");serial_hex(info.speed);
            serial_write(" vendor=");serial_hex(info.vendor);serial_write(" product=");serial_hex(info.product);
            serial_write(" ep0_packet=");serial_hex(info.ep0_packet);serial_write(" configuration_bytes=");serial_hex(info.configuration_bytes);
            serial_write(" interfaces=");serial_hex(info.interfaces);serial_write("\n");
        }
        int halted=xhci_stop(&controller);*pci_command&=~4U;
        serial_write(good && halted && pages_available()==available?"KERNEL_USB_INSPECTION_OK":"KERNEL_USB_INSPECTION_FAILED");
        serial_write(" devices=");serial_hex(count);serial_write(" commands=");serial_hex(controller.commands);
        serial_write(" transfers=");serial_hex(controller.transfers);serial_write(" events=");serial_hex(controller.events);
        serial_write(" completion=");serial_hex(controller.last_completion);serial_write(" pages_restored=");serial_hex(pages_available()==available);serial_write("\n");
        return;
    }
    serial_write("KERNEL_USB_ADAPTER_ABSENT\n");
}
void vm_usb_poll(void) {
#if COMPANION_VM_USB_NETWORK
    if(!network_ready) return;
    U32 length;
    for(U32 i=0;i<16 && xhci_bulk_receive(&controller,received,sizeof(received),&length);++i) {
        if(!discard_frame && length>1514) serial_write("KERNEL_USB_OVERSIZE_FRAME_DROPPED\n");
        if(!usb_ecm_frame_boundary(&discard_frame,length)) continue;
        ++network_received;U8 action;ManagementStatus status={timer_ticks(),pages_available(),network_received,network_transmitted};
        U32 size=network_reply(&network,received,length,&status,response,sizeof(response),&action);
        if(size) while(size<60) response[size++]=0;
        if(size && xhci_bulk_send(&controller,response,size)) {
            ++network_transmitted;if(action==3) vm_management_schedule_reboot();
        }
    }
    vm_management_poll(xhci_bulk_tx_idle(&controller));
    if(controller.failed) {
        network_ready=0;serial_write("KERNEL_USB_NETWORK_FAILED\n");(void)xhci_stop(&controller);
    }
#endif
}
#else
void vm_usb_start(const BootInfo *boot,const PciInventory *devices) { (void)boot;(void)devices; }
void vm_usb_poll(void) {}
#endif

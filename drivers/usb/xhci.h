#ifndef COMPANION_XHCI_H
#define COMPANION_XHCI_H
#include "../../common/types.h"
#define XHCI_WINDOW 0x4000
#define XHCI_RING_SIZE 64
#define XHCI_MAX_PAGES 48
#define XHCI_BULK_RX_SIZE 2048U
typedef struct { U64 parameter;U32 status,control; } XhciTrb;
_Static_assert(sizeof(XhciTrb)==16,"xHCI TRB ABI");
/* All DMA pages are identity mapped, coherent, aligned and below 4 GiB.
 * The caller supplies exclusive ownership of the mapped controller window.
 * This initial implementation is enabled only for the isolated QEMU profile. */
typedef struct {
    void *context;
    U32 (*read)(void *,U32);
    void (*write)(void *,U32,U32);
    U64 (*allocate)(void *);
    void (*release)(void *,U64);
    void (*fence)(void *);
    void (*bus_master)(void *,U8);
    U32 poll_limit; /* 1..1,000,000 register/event polls per operation */
} XhciIo;
typedef struct {
    U64 ring,buffer,pointer;
    U32 next,requested,actual;
    U16 max_packet;
    U8 dci,cycle,pending,done;
} XhciBulk;
typedef struct { U8 address,burst;U16 max_packet; } UsbBulkEndpoint;
typedef struct {
    XhciIo io;
    U32 op,runtime,doorbell,slots,ports,context_size;
    U32 command_next,event_next,transfer_next;
    U8 command_cycle,event_cycle,transfer_cycle,running,failed;
    U32 allocated,commands,transfers,events,port_events,last_completion;
    U64 pages[XHCI_MAX_PAGES];
    U64 dcbaa,command_ring,event_ring,erst,input,output,transfer_ring,buffer;
    U32 active_slot,active_port,active_speed;
    XhciBulk bulk_in,bulk_out;
} Xhci;
typedef struct {
    U16 vendor,product,usb_version,device_version;
    U16 ep0_packet,configuration_bytes;
    U8 port,speed,device_class,configurations,interfaces;
} UsbDeviceInfo;
/* Start requires a zero-initialized or successfully stopped instance. */
int xhci_start(Xhci *,const XhciIo *);
int xhci_noop(Xhci *);
/* Read descriptors from one directly attached root-port device, then disable
 * its slot. This inspection API does not configure non-control endpoints. */
int xhci_inspect_port(Xhci *,U32,UsbDeviceInfo *);
/* One retained root-port device at a time; the controller owns all DMA pages. */
int xhci_open_port(Xhci *,U32,UsbDeviceInfo *);
int xhci_close_port(Xhci *);
/* Control requests currently require an exact data length, at most 1024 bytes.
 * Data direction follows bmRequestType. IN data is copied only after status. */
int xhci_control(Xhci *,U8,U8,U16,U16,U8 *,U32);
int xhci_configure_bulk(Xhci *,const UsbBulkEndpoint *,const UsbBulkEndpoint *);
/* One outstanding TD per bulk endpoint. NAK leaves IN pending; polling never
 * waits for absent traffic. Receive returns 1 for a completed ZLP with size 0,
 * and 0 while pending. Errors require controller halt before DMA reuse. */
int xhci_bulk_send(Xhci *,const U8 *,U32);
int xhci_bulk_receive(Xhci *,U8 *,U32,U32 *);
int xhci_bulk_tx_idle(Xhci *);
/* Halt before releasing DMA. On uncertain halt retain every DMA allocation. */
int xhci_stop(Xhci *);
int usb_device_descriptor(const U8 *,U32,U32,UsbDeviceInfo *);
int usb_configuration_descriptor(const U8 *,U32,U8 *);
#endif

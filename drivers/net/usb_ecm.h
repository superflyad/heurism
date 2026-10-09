#ifndef COMPANION_USB_ECM_H
#define COMPANION_USB_ECM_H
#include "../usb/xhci.h"
typedef struct {
    UsbBulkEndpoint in,out;
    U16 max_frame;
    U8 configuration,control_interface,data_interface,alternate,mac_string;
} UsbEcmDescriptor;
int usb_ecm_descriptor(const U8 *,U32,U32,UsbEcmDescriptor *);
int usb_ecm_mac(const U8 *,U32,U8 *);
#define USB_ECM_TRANSFER_SIZE XHCI_BULK_RX_SIZE
/* A full receive TD may be followed by more bytes in the same frame.
 * Drop oversized frames through the next short transfer or ZLP. */
int usb_ecm_frame_boundary(U8 *discard,U32 length);
/* CDC ECM full/high/SuperSpeed descriptor path. Physical traffic is untested;
 * the Dell adapter exposes both vendor and ECM configurations. */
int usb_ecm_start(Xhci *,const UsbDeviceInfo *,U8 *);
#endif

#include "../drivers/usb/xhci.h"
U32 xhci_structure_size(void) { return sizeof(Xhci); }
U32 xhci_io_size(void) { return sizeof(XhciIo); }
U32 usb_info_size(void) { return sizeof(UsbDeviceInfo); }

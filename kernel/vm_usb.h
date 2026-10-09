#ifndef COMPANION_VM_USB_H
#define COMPANION_VM_USB_H
#include "boot_info.h"
#include "../platform/x86_64/pci.h"
void vm_usb_start(const BootInfo *,const PciInventory *);
void vm_usb_poll(void);
#endif

#ifndef COMPANION_VM_NETWORK_H
#define COMPANION_VM_NETWORK_H
#include "boot_info.h"
#include "../platform/x86_64/pci.h"
void vm_network_start(const BootInfo *,const PciInventory *);
void vm_network_poll(void);
#endif

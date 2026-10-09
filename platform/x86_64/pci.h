#ifndef COMPANION_PCI_H
#define COMPANION_PCI_H
#include "acpi.h"
#define PCI_MAX_DEVICES 256
typedef int (*PciRead32)(void *,U64,U32 *);
typedef struct {
    U64 configuration;
    U16 segment,vendor,device;
    U8 bus,slot,function,revision,programming_interface,subclass,class_code,header_type;
    U32 bars[6];
} PciDevice;
typedef struct { U32 count,reads;PciDevice devices[PCI_MAX_DEVICES]; } PciInventory;
int pci_configuration_address(const AcpiEcam *,U32,U32,U32,U32,U64 *);
/* Bounded read-only scan. On any invalid input/read failure output is empty. */
int pci_inventory(const AcpiInventory *,PciRead32,void *,PciInventory *);
#endif

#ifndef COMPANION_E1000_H
#define COMPANION_E1000_H
#include "../../platform/x86_64/pci.h"
#include "../../kernel/boot_info.h"
#define E1000_RING 32
typedef struct { U64 address;U16 length,checksum;U8 status,errors;U16 special; } E1000Rx;
typedef struct { U64 address;U16 length;U8 checksum_offset,command,status,checksum_start;U16 special; } E1000Tx;
_Static_assert(sizeof(E1000Rx)==16 && sizeof(E1000Tx)==16,"DMA descriptor ABI");
typedef struct {
    volatile U32 *registers;
    volatile E1000Rx *rx;
    volatile E1000Tx *tx;
    U64 rx_buffers[E1000_RING],tx_buffers[E1000_RING];
    U32 rx_next,tx_next,allocated_count;
    U64 allocations[2+2*E1000_RING],received,transmitted,dropped;
    U8 mac[6],ready,discard_fragments;
} E1000;
/* Currently used only by the explicit isolated VM network profile. */
int e1000_start(E1000 *,const BootInfo *,const PciDevice *);
int e1000_receive(E1000 *,U8 *,U32,U32 *);
int e1000_send(E1000 *,const U8 *,U32);
int e1000_tx_idle(const E1000 *);
#endif

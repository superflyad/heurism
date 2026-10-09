#include "e1000.h"
#include "../../kernel/pages.h"
#include "../../platform/x86_64/paging.h"
static U32 reg_read(const E1000 *n,U32 reg) { return n->registers[reg/4]; }
static void reg_write(E1000 *n,U32 reg,U32 value) { n->registers[reg/4]=value;(void)reg_read(n,8); }
static void fence(void) { __asm__ volatile("mfence":::"memory"); }
static U64 allocate(E1000 *n) {
    U64 page=page_alloc();if(!page) return 0;n->allocations[n->allocated_count++]=page;
    U8 *p=(U8 *)page;for(U32 i=0;i<4096;++i) p[i]=0;return page;
}
int e1000_start(E1000 *n,const BootInfo *boot,const PciDevice *d) {
    if(!n) return 0;*n=(E1000){0};
    if(!boot || !d || d->vendor!=0x8086 || d->device!=0x100e || d->class_code!=2 ||
       (d->header_type&127)!=0 || !d->configuration || (d->bars[0]&15) || !d->bars[0]) return 0;
    U64 base=d->bars[0]&~15U;
    if((base&0x1ffff) || !paging_device_range(boot,base,0x20000)) return 0;
    n->registers=(volatile U32 *)base;
    /* Preserve the status register: PCI command is a 16-bit access, not a
     * dword read/modify/write that could clear write-one-to-clear status. */
    volatile U16 *command=(volatile U16 *)(d->configuration+4);
    *command=(*command|2)&~4U;
    reg_write(n,0xd8,0xffffffff);reg_write(n,0x100,0);reg_write(n,0x400,0);
    reg_write(n,0,reg_read(n,0)|(1U<<26));
    U32 wait;for(wait=0;wait<1000000;++wait) if(!(reg_read(n,0)&(1U<<26))) break;
    if(wait==1000000) return 0;
    reg_write(n,0xd8,0xffffffff);(void)reg_read(n,0xc0);
    U32 low=reg_read(n,0x5400),high=reg_read(n,0x5404);
    if(!(high&(1U<<31))) return 0;
    for(U32 i=0;i<4;++i) n->mac[i]=(U8)(low>>(i*8));n->mac[4]=(U8)high;n->mac[5]=(U8)(high>>8);
    if(n->mac[0]&1) return 0;
    n->rx=(volatile E1000Rx *)allocate(n);n->tx=(volatile E1000Tx *)allocate(n);
    if(!n->rx || !n->tx) goto failed;
    for(U32 i=0;i<E1000_RING;++i) {
        n->rx_buffers[i]=allocate(n);n->tx_buffers[i]=allocate(n);
        if(!n->rx_buffers[i] || !n->tx_buffers[i]) goto failed;
        n->rx[i].address=n->rx_buffers[i];n->tx[i].address=n->tx_buffers[i];n->tx[i].status=1;
    }
    for(U32 i=0;i<128;++i) reg_write(n,0x5200+i*4,0);
    reg_write(n,0x2800,(U32)(U64)n->rx);reg_write(n,0x2804,0);reg_write(n,0x2808,E1000_RING*16);
    reg_write(n,0x2810,0);reg_write(n,0x2818,E1000_RING-1);reg_write(n,0x2820,0);
    reg_write(n,0x3800,(U32)(U64)n->tx);reg_write(n,0x3804,0);reg_write(n,0x3808,E1000_RING*16);
    reg_write(n,0x3810,0);reg_write(n,0x3818,0);reg_write(n,0x3820,0);
    reg_write(n,0x410,10|(8U<<10)|(6U<<20));
    reg_write(n,0,reg_read(n,0)|(1U<<6));
    fence();*command|=4; /* DMA only after every owned descriptor is initialized. */
    reg_write(n,0x400,(1U<<1)|(1U<<3)|(15U<<4)|(64U<<12));
    reg_write(n,0x100,(1U<<1)|(1U<<15)|(1U<<26)); /* enable, broadcast, strip CRC, 2048-byte buffers */
    n->ready=1;return 1;
failed:
    /* Bus mastering remains disabled. No device owns these allocations yet. */
    for(U32 i=0;i<n->allocated_count;++i) (void)page_free(n->allocations[i]);
    *n=(E1000){0};return 0;
}
int e1000_receive(E1000 *n,U8 *out,U32 capacity,U32 *length) {
    if(!n || !n->ready || !out || !length) return 0;
    for(U32 poll=0;poll<E1000_RING;++poll) {
        U32 index=n->rx_next;volatile E1000Rx *d=&n->rx[index];U8 status=d->status;
        if(!(status&1)) return 0;fence();
        U32 size=d->length;int good=(status&2) && !n->discard_fragments && !d->errors && size>=14 && size<=1514 && size<=capacity;
        n->discard_fragments=!(status&2);
        if(good) { const U8 *buffer=(const U8 *)n->rx_buffers[index];for(U32 i=0;i<size;++i) out[i]=buffer[i];++n->received; }
        else ++n->dropped;
        d->status=0;d->errors=0;fence();reg_write(n,0x2818,index);n->rx_next=(index+1)%E1000_RING;
        if(good) { *length=size;return 1; }
    }
    return 0;
}
int e1000_send(E1000 *n,const U8 *packet,U32 size) {
    if(!n || !n->ready || !packet || size<14 || size>1514) return 0;
    U32 index=n->tx_next;volatile E1000Tx *d=&n->tx[index];if(!(d->status&1)) return 0;fence();
    U8 *buffer=(U8 *)n->tx_buffers[index];for(U32 i=0;i<size;++i) buffer[i]=packet[i];
    while(size<60) buffer[size++]=0;
    d->length=(U16)size;d->command=0xb;d->status=0;fence();
    n->tx_next=(index+1)%E1000_RING;reg_write(n,0x3818,n->tx_next);++n->transmitted;return 1;
}
int e1000_tx_idle(const E1000 *n) {
    if(!n || !n->ready) return 0;
    for(U32 i=0;i<E1000_RING;++i) if(!(n->tx[i].status&1)) return 0;
    return 1;
}

#include "vm_network.h"
#ifndef COMPANION_VM_NETWORK
#define COMPANION_VM_NETWORK 0
#endif
#if COMPANION_VM_NETWORK
#include "network.h"
#include "vm_management.h"
#include "pages.h"
#include "../drivers/net/e1000.h"
#include "../platform/x86_64/io.h"
#include "../platform/x86_64/timer.h"
static E1000 nic;
static Network network;
static U8 received[1514],response[1514];
void vm_network_start(const BootInfo *boot,const PciInventory *devices) {
    if(!boot || !devices || !(boot->flags&COMPANION_BOOT_VM_NETWORK) || !(boot->flags&COMPANION_BOOT_VM_SERIAL)) return;
    if(!vm_management_init(&network)) return;
    for(U32 i=0;i<devices->count;++i) if(devices->devices[i].vendor==0x8086 && devices->devices[i].device==0x100e) {
        if(!e1000_start(&nic,boot,&devices->devices[i])) { serial_write("KERNEL_NETWORK_DRIVER_FAILED\n");return; }
        for(U32 n=0;n<6;++n) network.mac[n]=nic.mac[n];
        serial_write("KERNEL_NETWORK_READY e1000 ip=10.0.2.15 port=47333 vm_fixture_only\n");return;
    }
    serial_write("KERNEL_NETWORK_ADAPTER_ABSENT\n");
}
void vm_network_poll(void) {
    U32 length;
    for(U32 i=0;i<16 && e1000_receive(&nic,received,sizeof(received),&length);++i) {
        U8 action;ManagementStatus s={timer_ticks(),pages_available(),nic.received,nic.transmitted};
        U32 size=network_reply(&network,received,length,&s,response,sizeof(response),&action);
        if(size && e1000_send(&nic,response,size) && action==3) vm_management_schedule_reboot();
    }
    vm_management_poll(e1000_tx_idle(&nic));
}
#else
void vm_network_start(const BootInfo *boot,const PciInventory *devices) { (void)boot;(void)devices; }
void vm_network_poll(void) {}
#endif

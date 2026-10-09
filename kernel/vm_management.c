#include "vm_management.h"
#if COMPANION_VM_NETWORK || COMPANION_VM_USB_NETWORK
#include "../platform/x86_64/io.h"
#include "../platform/x86_64/timer.h"
static U64 reset_at,reset_deadline;
/* Public fixture data, compiled out of production. */
static const U8 fixture_key[32]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
static int boot_nonce(U8 out[16]) {
    U32 a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    if(!(c&(1U<<30))) return 0;
    for(U32 part=0;part<2;++part) {
        U64 value=0;U8 good=0;
        for(U32 retry=0;retry<20;++retry) { __asm__ volatile("rdrand %0; setc %1":"=r"(value),"=qm"(good));if(good) break; }
        if(!good) return 0;for(U32 i=0;i<8;++i) out[part*8+i]=(U8)(value>>(i*8));
    }
    return 1;
}
int vm_management_init(Network *network) {
    U8 nonce[16];
    if(!boot_nonce(nonce) || !management_init(&network->management,fixture_key,nonce)) {
        serial_write("KERNEL_NETWORK_NONCE_UNAVAILABLE\n");return 0;
    }
    network->ip[0]=10;network->ip[1]=0;network->ip[2]=2;network->ip[3]=15;return 1;
}
void vm_management_schedule_reboot(void) {
    if(reset_at) return;
    reset_at=timer_ticks()+10;reset_deadline=reset_at+20;
    serial_write("KERNEL_AUTHENTICATED_REBOOT scheduled\n");
}
void vm_management_poll(int tx_idle) {
    if(reset_at && timer_ticks()>=reset_at) {
        if(tx_idle) {
            serial_write("KERNEL_AUTHENTICATED_REBOOT reset_vm\n");out8(0xcf9,2);out8(0xcf9,6);
            reset_at=0;serial_write("KERNEL_VM_RESET_FAILED\n");
        } else if(timer_ticks()>=reset_deadline) { reset_at=0;serial_write("KERNEL_REBOOT_ACK_TIMEOUT\n"); }
    }
}
#endif

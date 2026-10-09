/* A single unchanged-data PROGRAM request, not a general flash writer.
 * Fixed empty BIOS padding, no erase/WREN/protection changes or caller input.
 * Register definitions: Intel 631120-002 and Linux v6.18 spi-intel.c.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define DEV "/sys/bus/pci/devices/0000:00:1f.5/"
#define TARGET 0x911000U
#define STATUS 4U
#define BUSY (1U << 5)
#define GO (1U << 16)
#define CYCLES (15U << 17)
#define COUNTS (63U << 24)
static uint32_t request(uint32_t status, unsigned cycle) {
    if (cycle != 0 && cycle != 2) abort();
    return (status & ~(CYCLES | COUNTS | GO | (1U << 31))) |
           (cycle << 17) | COUNTS | GO | 7U;
}
static int unchanged_empty(const uint32_t *words) {
    for (unsigned i=0;i<16;i++) if (words[i]!=UINT32_MAX) return 0;
    return 1;
}
#ifdef GATE_HOST_TEST
int main(void) {
    uint32_t original[16];
    for(unsigned i=0;i<16;i++) original[i]=UINT32_MAX;
    if(!unchanged_empty(original))return 1;
    original[15]=0xfffffffe;
    if(unchanged_empty(original))return 2;
    uint32_t r=request(0xffffffff,0),w=request(0xffffffff,2);
    if((r&CYCLES)!=0 || (w&CYCLES)!=(2U<<17) || (r&COUNTS)!=COUNTS ||
       (w&COUNTS)!=COUNTS || !(r&GO) || !(w&GO) || (r&(1U<<31)) || (w&(1U<<31)))return 3;
    puts("PASS: fixed read/program encoding and empty-byte gate; no erase cycle available");
    return 0;
}
#else
static void fail(const char *message) {perror(message);exit(1);}
static uint64_t millis(void) {
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t))fail("clock");
    return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;
}
static uint32_t cycle(volatile uint32_t *mmio, unsigned kind) {
    uint32_t initial=mmio[STATUS/4];
    if(initial&BUSY) {fprintf(stderr,"Controller busy; no request issued\n");exit(1);}
    mmio[8/4]=TARGET;
    mmio[STATUS/4]=request(initial,kind);
    uint64_t deadline=millis()+5000;
    for(;;) {
        uint32_t s=mmio[STATUS/4];
        if(!(s&BUSY) && (s&7U)) return s;
        if(millis()>=deadline) {fprintf(stderr,"Controller did not complete; no further cycles issued\n");exit(1);}
    }
}
int main(void) {
    if(getuid()!=0) {fprintf(stderr,"Requires root\n");return 1;}
    int lock=open("/run/companion-spi-read.lock",O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(lock<0 || flock(lock,LOCK_EX|LOCK_NB))fail("exclusive flash lock");
    if(!access(DEV "driver",F_OK)) {fprintf(stderr,"Bound driver; refusing concurrent access\n");return 1;}
    int pci=open(DEV "config",O_RDONLY|O_CLOEXEC);
    uint8_t config[256];
    if(pci<0 || pread(pci,config,sizeof(config),0)!=(ssize_t)sizeof(config))fail("PCI read");
    uint32_t bcr;memcpy(&bcr,config+0xdc,4);
    if(memcmp(config,"\x86\x80\xa4\xa0",4) || !(config[4]&2) || (bcr&0xa3)!=0xa2) {
        fprintf(stderr,"Unexpected controller or protection state; no cycles issued\n");return 1;
    }
    int fd=open(DEV "resource0",O_RDWR|O_CLOEXEC);
    if(fd<0)fail("BAR open");
    volatile uint32_t *mmio=mmap(0,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(mmio==MAP_FAILED)fail("BAR map");
    uint32_t initial=mmio[STATUS/4], region=mmio[0x58/4], old_address=mmio[8/4];
    if((initial&0xc000)!=0xc000 || initial&BUSY || region!=0x17ff0800) {
        fprintf(stderr,"Unexpected locked flash descriptor/layout; no cycles issued\n");return 1;
    }
    for(unsigned i=0;i<5;i++) {
        uint32_t p=mmio[(0x84+4*i)/4];
        unsigned lo=(p&0x7fff)<<12, hi=(((p>>16)&0x7fff)<<12)|0xfff;
        if((p&0x80008000U) && lo<=TARGET+63 && hi>=TARGET) {
            fprintf(stderr,"Padding intersects protected range; no cycles issued\n");return 1;
        }
    }
    uint32_t before[16],after[16];
    uint32_t read_before=cycle(mmio,0);
    if(read_before&6U) {fprintf(stderr,"Baseline read failed; no program issued\n");return 1;}
    for(unsigned i=0;i<16;i++)before[i]=mmio[(0x10+4*i)/4];
    if(!unchanged_empty(before)) {
        mmio[8/4]=old_address;
        fprintf(stderr,"Target is not empty padding; no program issued\n");return 1;
    }
    /* Exactly the bytes read above; NOR PROGRAM cannot turn zero bits into ones. */
    for(unsigned i=0;i<16;i++)mmio[(0x10+4*i)/4]=before[i];
    uint32_t program=cycle(mmio,2);
    uint32_t read_after=cycle(mmio,0);
    if(read_after&6U) {fprintf(stderr,"Post-program read failed\n");return 1;}
    for(unsigned i=0;i<16;i++)after[i]=mmio[(0x10+4*i)/4];
    mmio[8/4]=old_address; /* READ remains selected, not PROGRAM. */
    uint32_t bcr_after;
    if(pread(pci,&bcr_after,4,0xdc)!=4)fail("Post-program PCI read");
    int identical=!memcmp(before,after,sizeof(before));
    printf("{\"scope\":\"One fixed unchanged 64-byte PROGRAM request; no erase or protection writes\","
           "\"flash_address\":\"0x%08x\",\"bytes\":64,\"program_status\":\"0x%08x\","
           "\"controller_reported_error\":%s,\"contents_unchanged\":%s,"
           "\"bios_control_before\":\"0x%08x\",\"bios_control_after\":\"0x%08x\"}\n",
           TARGET,program,(program&6U)?"true":"false",identical?"true":"false",bcr,bcr_after);
    munmap((void *)mmio,4096);close(fd);close(pci);close(lock);
    return identical?0:1;
}
#endif

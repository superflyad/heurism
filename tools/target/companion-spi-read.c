/* Read-only flash cycles for the observed Intel 8086:a0a4 controller.
 * MMIO layout and sequencing verified against Linux v6.18 spi-intel.c.
 * No program, erase, WREN, status-register write or protection change exists.
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
#define STATUS 0x04
#define ADDRESS 0x08
#define DATA 0x10
#define BUSY (1U << 5)
#define GO (1U << 16)
#define CYCLE_MASK (15U << 17)
#define COUNT_MASK (63U << 24)
#define TOTAL (24U * 1024U * 1024U)

static void fail(const char *message) {
    fprintf(stderr, "%s: %s\n", message, strerror(errno));
    exit(1);
}
static uint32_t reg(volatile uint32_t *mmio, unsigned offset) {
    return mmio[offset / 4];
}
static uint64_t millis(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) fail("clock");
    return (uint64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}
static unsigned number(const char *text) {
    char *end;
    errno = 0;
    unsigned long result = strtoul(text, &end, 0);
    if (errno || !*text || *end || result > TOTAL) {
        fprintf(stderr, "Invalid bounded numeric argument\n"); exit(1);
    }
    return (unsigned)result;
}
int main(int argc, char **argv) {
    if (argc != 4 || getuid() != 0) {
        fprintf(stderr, "Usage (root): companion-spi-read OFFSET LENGTH NEW_OUTPUT_FILE\n");
        return 1;
    }
    unsigned begin = number(argv[1]), length = number(argv[2]);
    if (!length || length > TOTAL - begin || begin % 64 || length % 64) {
        fprintf(stderr, "Range must be 64-byte aligned and within 24 MiB\n"); return 1;
    }
    int lock = open("/run/companion-spi-read.lock", O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lock < 0 || flock(lock, LOCK_EX | LOCK_NB)) fail("exclusive reader lock");
    if (!access(DEV "driver", F_OK)) {
        fprintf(stderr, "Controller has a bound driver; refusing concurrent access\n"); return 1;
    }
    int pci = open(DEV "config", O_RDONLY | O_CLOEXEC);
    uint8_t config[256];
    if (pci < 0 || pread(pci, config, sizeof config, 0) != sizeof config) fail("PCI read");
    close(pci);
    if (memcmp(config, "\x86\x80\xa4\xa0", 4) || !(config[4] & 2)) {
        fprintf(stderr, "Unexpected PCI identity or memory decoding disabled\n"); return 1;
    }
    /* This file is writable only to request READ cycles, never flash writes. */
    int fd = open(DEV "resource0", O_RDWR | O_CLOEXEC);
    if (fd < 0) fail("BAR open");
    volatile uint32_t *mmio = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mmio == MAP_FAILED) fail("BAR mapping");
    uint32_t initial = reg(mmio, STATUS);
    if (!(initial & (1U << 14)) || !(initial & (1U << 15)) || (initial & BUSY)) {
        fprintf(stderr, "Requires valid, locked, idle descriptor controller\n"); return 1;
    }
    for (unsigned i = 0; i < 5; i++) {
        uint32_t pr = reg(mmio, 0x84 + 4*i);
        unsigned low = (pr & 0x7fff) << 12;
        unsigned high = (((pr >> 16) & 0x7fff) << 12) | 0xfff;
        if ((pr & (1U << 15)) && low < begin + length && high >= begin) {
            fprintf(stderr, "Requested range intersects read protection\n"); return 1;
        }
    }
    int output = open(argv[3], O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (output < 0) fail("new output file");
    uint32_t old_address = reg(mmio, ADDRESS);
    uint64_t started = millis();
    for (unsigned address = begin; address < begin + length; address += 64) {
        uint32_t status = reg(mmio, STATUS);
        if (status & BUSY) {
            fprintf(stderr, "Controller busy before read at 0x%x; output incomplete\n", address);
            return 1;
        }
        mmio[ADDRESS/4] = address;
        /* FCYCLE is hard-coded to zero (READ). Clear stale W1C completion/errors. */
        uint32_t request = status & ~(CYCLE_MASK | COUNT_MASK | GO | (1U << 31));
        request |= (63U << 24) | 7U | GO;
        mmio[STATUS/4] = request;
        uint64_t deadline = millis() + 5000;
        do {
            status = reg(mmio, STATUS);
            if ((status & 6U) && !(status & BUSY)) {
                fprintf(stderr, "Read denied/failed at 0x%x: status 0x%08x; output incomplete\n", address, status);
                return 1;
            }
            if (millis() >= deadline) {
                fprintf(stderr, "Read timeout at 0x%x; output incomplete\n", address); return 1;
            }
        } while ((status & BUSY) || !(status & 1U));
        uint32_t buffer[16];
        for (unsigned i = 0; i < 16; i++) buffer[i] = reg(mmio, DATA + i*4);
        if (write(output, buffer, sizeof buffer) != sizeof buffer) fail("output write");
    }
    /* Restore address only; leave READ selected rather than restoring an old write cycle. */
    mmio[ADDRESS/4] = old_address;
    if (fsync(output) || close(output)) fail("output sync");
    munmap((void *)mmio, 4096);
    close(fd); close(lock);
    fprintf(stderr, "Read %u bytes from 0x%x in %llu ms; no flash writes\n",
            length, begin, (unsigned long long)(millis() - started));
    return 0;
}

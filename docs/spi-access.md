# Software flash access on the physical Dell

On 2026-09-26, after the owner identified the two XMC chips and reassembled the
laptop, root access returned on boot `e9c7e997-89ef-4ca3-b57a-aa7c7356ea72`.
Board firmware was inspected and readable regions backed up through the existing
Ethernet connection. No external programmer or USB image was needed for these reads.

## Controller inspection

Intel SPI PCI device 8086:a0a4 has BAR0 at 0x70800000, length 4096. PCI command
0x0402 already enables memory decoding. No SPI driver is bound, and the installed
Linux kernel does not include CONFIG_SPI_INTEL_PCI/PLATFORM.

The first inspection used only O_RDONLY/PROT_READ access to PCI config and the
exported BAR. BCR was 0x100008aa, with write-protect-disable clear. HSFSTS_CTL
was 0x0004e800: valid descriptor, locked configuration, no cycle in progress.
All five protected-range registers were zero. No inference that firmware is
writable follows from those zero registers: BIOS control and hardware boot
verification are separate protections.

## Restricted hardware reader

`tools/target/companion-spi-read.c` uses the Linux v6.18 hardware read sequencing
for this exact controller identity. It requires root, an unbound controller,
enabled memory decoding, a valid locked descriptor and an idle sequencer. It
holds a process lock and rejects unaligned/out-of-range requests or read-protected
ranges. Each transaction reads 64 bytes, polls completion for at most five
seconds and stops on controller errors. There are no program, erase, WREN,
status-register-write or protection-change operations in the program.

Requesting a flash read requires writes to the controller's address and cycle
control registers. These are **not writes to flash contents**. The tool never
changes BIOS_CNTL, descriptor permissions, protected ranges or lock bits. It
clears old completion/error status as required for the hardware read transaction.

GCC/musl development packages were installed on the Dell to compile the reader.
Strict compilation (`-Wall -Wextra -Werror`) passed. The host wrapper uses pinned
SSH, creates a new output file, downloads only completed reads and compares
target and host SHA-256. Firmware dumps remain under ignored `artifacts/firmware/`
locally and root-only `/var/lib/companion/firmware/` on the Dell. Treat dumps as
private machine data, not redistributable project source.

## Descriptor and verified backups

The descriptor signature is 0x0ff0a55a. FLCOMP density codes 4/5 and component
count 2 identify an 8 MiB first component and 16 MiB second component. Together
with the photo this supports U2501 as the first 8 MiB part and U2503 as the
second 16 MiB part; the controller does not independently label physical board
references.

| Region | Logical address range | Size | Host descriptor permission | Backup |
| --- | --- | --- | --- | --- |
| Descriptor | 0x000000–0x000fff | 4096 bytes | Read | Two identical copies |
| EC | 0x001000–0x100fff | 1 MiB | Neither read nor write | Read denied |
| CSME | 0x101000–0x7fffff | 7,335,936 bytes | Read | Two identical copies |
| BIOS | 0x800000–0x17fffff | 16 MiB | Read/write in descriptor only | Two identical copies |

Host FLMSTR1 is 0x00a00f00. The descriptor's BIOS write permission does not
override the BCR write protection or Boot Guard signature requirements. EC host
read permission is absent. A bounded attempted whole-device read stopped at
0x1000 with HSFSTS_CTL 0x3f00e803; the target output from that failed attempt is
only partial. No completed whole-flash image exists and no missing region was
filled with invented bytes.

Independent A/B reads were byte-identical, and both target-to-host transfers
matched their hashes:

- Descriptor: `b1fe3f65487295fea1c7e647e4cc9bb59400785d6112e00ea7eddee2498b78c9`
- BIOS: `09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c`
- CSME: `c07d8a6f14b0ea3e22d3cffa0c2eb0188fc9e00dfb3579a13d8fbae9c648b910`

The BIOS FIT pointer is 0xffc00ae0. Its 11-entry table passed checksum validation.
It locates a `__KEYM__` key manifest at 0xffc00060 (869 bytes) and an `__ACBP__`
boot policy manifest at 0xffc004a0 (1185 bytes). These were extracted from copied
files for further offline analysis. Extraction alone does not prove signatures
valid or show that an owner-generated signing key would be accepted.

Subsequent offline checks used 9elements Converged Security Suite pinned at
`667ce65d75b23ce34df470bf56da8facacfbdb20`. Its `km-verify` and `bpm-verify`
commands passed for both extracted manifests, verifying signatures against their
embedded public keys. Its file-only `read-config` decoded the BIOS backup.
The tool was compiled on the Dell after `go mod verify` passed; no signing,
provisioning, stitching or flash-writing command was invoked.

`tools/verify-boot-policy-data.py` independently verified the copied BIOS bytes
against all four IBB digests: SHA-384, SHA-1, SHA-256 and SM3. The four segments
cover physical addresses 0xffe60000–0xffffffff, totaling 1,703,936 bytes
(logical flash 0x1660000–0x17fffff). The BPM's 3072-bit RSA signing-key modulus
also matches the SHA-384 digest stored in the key manifest, following the pinned
Fiano parser's `ValidateBPMKey` encoding. Results are saved in ignored
`artifacts/firmware/boot-policy-data-verification.json`.

These checks establish internally consistent signed manifests and matching IBB
contents. They do **not** compare the key manifest's root key against hardware
fuses, demonstrate acceptance of our key, or authorize replacement of the
remaining firmware. Combined with the previously read verified-boot status and
committed-fuse status, this is evidence of a real firmware ownership barrier.
No unsigned replacement BIOS has been written or tested.

After these reads the same physical boot remained healthy: root SSH and watcher
running, healthy boot ID equal to current boot ID. A final controller inspection
confirmed BCR, region registers, protected ranges and configuration lock unchanged;
the last read's byte-count and completion fields changed as expected.

## Reproduction

```powershell
python tools/inspect-spi.py
python tools/read-spi.py 0 4096 descriptor-a
python tools/read-spi.py 0x800000 0x1000000 bios16-a
python tools/read-spi.py 0x101000 0x6ff000 csme-a
# Repeat with fresh names ending in -b, then:
python tools/analyze-spi-backups.py
python tools/analyze-boot-policy.py
python tools/verify-boot-policy-data.py
```

Existing target output files are refused. Failed reads are not valid backups.
This closes the uncertainty about software **read access** to BIOS/CSME. It does
not establish safe write access, EC access, a replacement board port, acceptance
of our firmware or independent recovery from a nonbooting BIOS.

Sources: [Linux hardware SPI sequencing](https://github.com/torvalds/linux/blob/v6.18/drivers/spi/spi-intel.c),
[Linux PCI controller identity and BCR](https://github.com/torvalds/linux/blob/v6.18/drivers/spi/spi-intel-pci.c),
[coreboot Intel descriptor definitions](https://github.com/coreboot/coreboot/blob/e9a90afe4d77b8e27ad07aec620c9e1299190944/util/ifdtool/ifdtool.h),
[Intel boot policy manifest FIT rules](https://edc.intel.com/content/www/us/en/design/products-and-solutions/software-and-services/firmware-and-bios/firmware-interface-table/1.2/boot-policy-manifest-type-0x0c-rules/).

Offline parser sources:
[9elements pinned parser](https://github.com/9elements/converged-security-suite/tree/667ce65d75b23ce34df470bf56da8facacfbdb20),
[Fiano BPM-key digest validation](https://github.com/linuxboot/fiano/blob/8f28c11fe9e8/pkg/intel/metadata/cbnt/keymanifest/manifest_cbnt.go).

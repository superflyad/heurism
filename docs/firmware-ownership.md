# Firmware ownership: target 0 findings

The required endpoint is owner-built motherboard firmware that starts Companion
management, with independent recovery. Root access, BIOS settings control and an
OS installed on the SSD do not fulfill this requirement. It is currently unmet.

Update after reassembly: [software SPI access](spi-access.md) now proves read access
to the descriptor, complete BIOS region and mapped CSME region. Two independent
reads of each match. The remaining 1 MiB EC region is denied by descriptor
permissions and an actual hardware read failure. Earlier statements below about
not having read flash describe the preceding audit, before this update.
Offline verification also passed both manifest signatures, the BPM signing-key
digest in the key manifest, and all four digests covering the 1,703,936-byte
initial boot block. Hardware-fused root-key comparison remains unverified.
Further [firmware research and emulator tests](firmware-research.md) found
later-volume digests inside the protected IBB and tested the original Dell
trust-chaining comparison on unchanged and changed DXE-volume copies.

## Physical evidence, 2026-09-26

Target: Dell Inspiron 7506 2n1, board 0VK62X, Intel Tiger Lake i7-1165G7,
BIOS 1.35.0, CSME 15.0.50.2633.

Authenticated root collected 69 OS-exported files without flash probing, MMIO
access or register writes. The snapshot includes 38 ACPI tables, ordinary PCI
configuration bytes for firmware controllers, memory/resource maps and boot
logs. Each captured file passed SHA-256 and length verification; all collected
ACPI tables passed declared-length checks and applicable checksums. Windows
licensing tables and private SSH credentials were excluded.

Snapshot: `artifacts/firmware/20260926T192105Z/`. This is board evidence, **not a
flash backup**.

All eight CPU MSR 0x13A readings were `0x000000030000007f`, reporting measured
and verified Boot Guard enabled. The CSME PCI device is 8086:a0e0. Its captured
status registers were:

| Register | PCI offset | Value |
| --- | --- | --- |
| HFSTS1 | 0x40 | 0x90000245 |
| HFSTS2 | 0x48 | 0x09f10506 |
| HFSTS3 | 0x60 | 0x00000020 |
| HFSTS4 | 0x64 | 0x00004000 |
| HFSTS5 | 0x68 | 0x00041f03 |
| HFSTS6 | 0x6c | 0x47e003c9 |

Using coreboot's CSME 15 definitions, HFSTS1 manufacturing bit 4 is clear;
HFSTS6 FPF_SOC_LOCK bit 30 is set. Coreboot's status logic therefore reports
manufacturing mode disabled and FPFs committed. HFSTS3 SKU bits identify consumer
firmware. The CPU debug-disable bit is clear, which does **not** establish usable
debug access or a method to change the Boot Guard policy.

Intel documents that the OEM key hash can be committed to hardware fuses and
cannot subsequently be changed, and that Boot Guard verifies the initial boot
block before handing control to firmware. These observations are a substantial
barrier to replacing Dell initialization with our own. They are not a complete
dump of the OEM key, protected regions or every enforcement-policy field.

The complete upstream coreboot tree at commit
`e9a90afe4d77b8e27ad07aec620c9e1299190944` contains Tiger Lake SoC support but no
path containing `7506` or `vk62x`. No board-specific port for this target was
established. This check does not cover private ports or unmerged changes.

Coreboot's documented deguard path targets Skylake/Kaby Lake and ME 11. Its
documented prerequisites do not match this Tiger Lake/CSME 15 target; it is not
an established solution for this Dell.

The kernel reports ACPI EC ports 0x934/0x930 and GPE 0x6e. These describe an
interface, not the physical EC model or its flash wiring. Debug ACPI tables also
exist; their presence does not establish an accessible physical debug connector.

## What works and what remains missing

### Physical flash identification

The owner's close-up of the two chips beside the disconnected BATT socket shows
XMC branding and the following top markings:

| Board reference | Photo position | Visible part marking | Datasheet family capacity |
| --- | --- | --- | --- |
| U2501 | Upper, next to cooling-assembly screw | QH64AHIG | XM25QH64A: 64 Mbit / 8 MiB |
| U2503 | Lower, next to board edge | QH128AHIG | XM25QH128A: 128 Mbit / 16 MiB |

Both are eight-lead SPI NOR flash parts. XMC's A-family datasheets specify
2.7–3.6 V operation. These are component ratings, not measured board rail
voltages. The photograph does not establish how the two chips map to Intel
descriptor/CSME/BIOS regions, which controller selects them or whether the EC
shares access. Subsequent controller reads established the logical layout and
backed up the accessible regions, as documented above. The prior generic MX-like
logo interpretation is superseded by the readable XMC markings.

Photo attachment: `23D728AE-D35B-4E6E-A574-3A655EE909FC/1-Photo-1.jpg`.
Datasheet references:
[XMC XM25QH128A](https://www.alldatasheet.net/html-pdf/2175042/XMC/XM25QH128A/65/1/XM25QH128A.html)
and [XMC XM25QH64A manufacturer datasheet reproduction](https://www.scribd.com/document/635592692/XMC-XM25QH64AHIG-C328461).
No claim about flash-region
contents or access permission is based on chip density alone.

Chip identification was followed by the restricted controller reader documented
in `spi-access.md`. The physical image alone did not justify bypassing the
laptop/EC guard in a generic flash utility.

A commanded physical reboot changed boot ID from
`d359b17b-f6b8-4ea0-9d7c-923b196f01fd` to
`cb9d00fc-0939-40cd-9cc5-bb96255c5933`. Root SSH returned, the root filesystem was
the installed ext4 SSD, management/watch/boot-health services started, and the
EFI boot journal became confirmed (`companion_pending=0`). No physical assistance
was requested. This proves management startup **through existing Dell firmware**.

Missing prerequisites for replacement firmware:

- Electrical/wiring confirmation, flash layout and a validated full backup
  containing this machine's individual data.
- A tested independent flash restoration method. No programmer or independent
  BIOS display/keyboard/power interface is connected.
- A verified means of booting owner-built initialization code under the actual
  hardware verification policy. Root and an external flash programmer alone do
  not demonstrate CPU acceptance of a replacement image.
- A working board-specific firmware implementation and physical cold-boot tests.

No motherboard firmware was flashed. No supported root command has been found
that removes these hardware restrictions. The requirement cannot currently be
completed through the available SSH connection. Any next firmware write requires
a technically valid image and an established recovery path, not another OS install.

## Repeatable evidence tools

```powershell
python tools/collect-firmware-evidence.py
python tools/check-firmware-support.py
python tools/analyze-firmware-evidence.py artifacts/firmware/20260926T192105Z
```

The collector runs a bounded Python program through pinned, key-only root SSH;
the upstream check accesses official GitHub source metadata; the analyzer runs
offline and rejects altered files, invalid ACPI checksums, unexpected PCI identity
or a CSME version outside its supported decoder. All three passed Python
compilation and ran successfully against the recorded physical evidence.

Sources:

- [Intel key usage and hardware boot trust](https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/resources/key-usage-in-integrated-firmware-images.html)
- [coreboot CSME 15 status fields](https://github.com/coreboot/coreboot/blob/e9a90afe4d77b8e27ad07aec620c9e1299190944/src/soc/intel/common/block/include/intelblocks/me_15.h)
- [coreboot manufacturing and committed-fuse decoder](https://github.com/coreboot/coreboot/blob/e9a90afe4d77b8e27ad07aec620c9e1299190944/src/soc/intel/common/block/cse/cse_spec.c)
- [coreboot Boot Guard MSR decoder](https://github.com/coreboot/coreboot/blob/e9a90afe4d77b8e27ad07aec620c9e1299190944/src/security/intel/cbnt/logging.c)
- [coreboot deguard platform prerequisites](https://doc.coreboot.org/soc/intel/deguard.html)

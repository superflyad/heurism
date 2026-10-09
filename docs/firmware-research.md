# Dell 7506 firmware research and offline tests

The endpoint remains owner-built board firmware that starts Companion management.
This research pass investigates paths toward it. No replacement motherboard
firmware was written, and no path to complete ownership is claimed proven.

The subsequent [update authorization and EC research](firmware-update-research.md)
adds physical BIOS Guard status, a checksum/Authenticode-verified vendor baseline,
IBB/volume comparisons and version-1 Microchip EC header tests.

## Findings from this machine

UEFIExtract was built from LongSoft source pinned at
`dac91b26733ca21cb204e41614c1b8c81cc50860`, using its non-Qt file parser. Its input
was the independently backed-up BIOS image with SHA-256
`09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c`.
The downloaded report is hash-checked and kept in ignored private artifacts.

The parser produced 5,746 report lines. Independent structural checks validated
31 mapped firmware volume headers. The parser still reported two invalid-size
file headers, six TE image-base warnings and an unreferenced FIT candidate;
therefore this is not a claim that every vendor-specific object is understood.
Its authoritative FIT address and IBB digests agree with the previous analyses.

The DXE core and `SecurityStubDxe` are inside a compressed volume contained at
BIOS offsets 0x680000–0x77ffff, outside direct IBB coverage. However, the protected
IBB contains `DellTrustChainingPei` and a `__MHTS__` hash table. This prevents us
from treating all bytes outside the IBB as freely replaceable.

`DellTrustChainingPei` GUID is `84E90BA3-CB79-4267-AE2F-437B86DAA6F4`.
Its PE section body at BIOS offset 0xfafc20 is 7,520 bytes, SHA-256
`ca630da6dc32c03468b2ba2b05279a38ebaf6af67f184a8f44bc1523cdb116c2`.
UEFIExtract independently extracted an identical copy from each of two PEI
instances. The corresponding hash tables are at 0xbb19f0 and 0xfb19f0; the latter
is within the IBB already validated against the signed BPM.

We computed complete-volume SHA-1, SHA-256, SHA-384 and SHA-512 digests and found
all four in both tables for these nine physical volumes:

| BIOS offset | Size | Interpretation |
| --- | --- | --- |
| 0x140000 | 0x40000 | Firmware volume |
| 0x180000 | 0x3a0000 | Main compressed DXE/SMM containers |
| 0x520000 | 0xe0000 | Firmware volume |
| 0x600000 | 0x80000 | Firmware volume |
| 0x680000 | 0x100000 | Contains DXE core and security driver |
| 0x810000 / 0xc10000 | 0x70000 each | Identical digest references for two copies |
| 0x9d0000 / 0xdd0000 | 0x90000 each | Identical digest references for two copies |

There are 72 digest references: nine physical volumes × four algorithms × two
tables. Finding these digests does not alone prove which checks the physical
boot executes. It does show why preserving only direct IBB bytes is insufficient
evidence that a LinuxBoot retrofit would boot.

## Original firmware code tested in an emulator

`tools/test-trust-chain.py` executes the original x86 code at 0xfffb0b73 with
Unicorn 2.1.4. It selects the module's SHA-256 fallback using synthetic runtime
globals and substitutes only the EFI allocation call with a private heap buffer.
The original Dell SHA-256 calculation and digest comparison execute unchanged.
The expected digest comes from the protected table, not from the test's hash
implementation.

| Input to the same function | Result |
| --- | --- |
| Original 1 MiB volume at 0x680000 | Comparison matched |
| Same volume with one byte changed at 0x680080 | Comparison did not match |

Both calls returned normally, within bounded instruction/time limits. All input
files remained unchanged; changed bytes existed only in emulator memory. The
test-volume change is outside IBB coverage. Emulator runtime globals are also
synthetic; they are not measurements of the physical boot's selected path.
This tests one real firmware function, **not** CPU acceptance of a modified ROM,
physical runtime enforcement, a complete BIOS boot, or a security bypass.

Private results: `artifacts/firmware/uefi-volume-analysis.json` and
`artifacts/firmware/trust-chain-emulation.json`.

## Actual boot measurements and policy branches (2026-09-26)

The current boot's TPM2 event log was retrieved through pinned root SSH:
18,285 bytes, 43 events, SHA-256
`0f274e1adbd9d68ed0687ecd3447cc601e68be481b385b068cf7a123e35f3569`.
The advertised bank is SHA-256. A read-only TPM2_PCR_Read command read PCR 0
through `/dev/tpmrm0`. Replaying the log, with the recorded startup locality 3,
produced exactly the physical PCR 0 value:
`ff9b7f47b3ad02460dd09d1c1efc378748af23937f0f24b21de3a3362d9f3e48`.
This establishes consistency of the retrieved log with the current TPM value;
it is not a signed attestation or proof of every firmware enforcement decision.

Nine firmware measurement events match whole-volume digests in the verified ROM:

| BIOS offsets matching the event | Event type |
| --- | --- |
| 0x880000 / 0xc80000 | Firmware blob |
| 0x180000 | POST code |
| 0x680000 | Firmware blob; includes the compressed DXE Core container |
| 0x600000 | Firmware blob |
| 0x520000 | Firmware blob |
| 0x810000 / 0xc10000 | Firmware blob |
| 0x140000 | Firmware blob |
| 0x110000 | POST code |
| 0x9d0000 / 0xdd0000 | Firmware blob |

Identical ROM copies cannot be distinguished by their identical digest. The
logged firmware-blob bases are RAM addresses, not direct SPI addresses.
An earlier progress update incorrectly counted ten distinct measured volumes;
the parsed result is nine measurement events, with three pairs of identical
ROM copies.

The module's GUID at 0xfffb1694 matches the standard TCG2 event HOB GUID
`d26c221e-2430-4c8a-9170-3fcb4500413f`. Its 0xfffb15c4 GUID matches the TPM1
event HOB, and 0xfffb15a4 matches the TPM error HOB. These identifications are
verified against the EDK2 SecurityPkg TcgEventHob definitions.

`tools/test-trust-chain-hob.py` executes the original mode-2 comparison code
with actual boot event bytes packaged in a synthetic GUID HOB. Only GetHobList
is replaced; GUID walking, PCR/event selection, algorithm interpretation and
digest comparison execute in the original firmware code. All nine combinations
of protected-table records and corresponding measured ROM copies matched.
Each rejected three negative controls: changed digest, PCR 1 instead of PCR 0,
and separator event type instead of the accepted firmware event. That is 36
bounded calls with expected results. Synthetic globals select mode 2; these
tests do not prove which mode the physical boot selected.

`tools/test-trust-chain-policy.py` tests the original Security2 callback at
0xfffb095c, replacing only its GetBootMode helper. It checks the returned EFI
status and DeferExecution byte:

| Synthetic boot mode | Authentication state | Result |
| --- | --- | --- |
| Normal (0x00) | Passed (0x02) | EFI_SUCCESS; execution not deferred |
| Normal (0x00) | Failed (0x0a) | EFI_SECURITY_VIOLATION; execution deferred |
| S3 resume (0x11) | Failed (0x0a) | EFI_SUCCESS; execution not deferred |
| Recovery (0x20) | Failed (0x0a) | EFI_SECURITY_VIOLATION; execution deferred |

The special 0x11 branch is **S3 resume, not recovery**. This corrects the
initial branch interpretation. The running Dell exposes only `[s2idle]` in
`/sys/power/mem_sleep`, with no `deep` option, so ordinary current Linux suspend
does not establish access to this S3 branch. Nor does an offline callback result
establish a persistent way past reset-time Boot Guard or flash write locks.
No suspend, TPM reset/clear/extend, firmware programming, or lock change was
performed.

Private artifacts are `tpm-log-analysis.json`, the boot-ID-named raw event log
and PCR response, `trust-chain-hob-emulation.json`, and
`trust-chain-policy-emulation.json`, under `artifacts/firmware`.
The parser rejects truncated records, invalid counts, inconsistent/duplicate
algorithm IDs, metadata mismatches and unexpected PCR response selections.

Root SSH and Companion watcher remained started, the healthy boot ID stayed
`e9c7e997-89ef-4ca3-b57a-aa7c7356ea72`, and the remote BIOS backup SHA-256 stayed
unchanged. Full replacement firmware acceptance, authorized flash writes and
an independently recoverable restoration path remain unresolved.

```powershell
python tools/collect-tpm-log.py
python tools/analyze-tpm-log.py
artifacts/research-venv/Scripts/python.exe tools/test-trust-chain-hob.py
artifacts/research-venv/Scripts/python.exe tools/test-trust-chain-policy.py
```

Primary format and policy references:

- [EDK2 TCG platform event structures](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/IndustryStandard/UefiTcgPlatform.h)
- [EDK2 TCG event HOB GUIDs](https://github.com/tianocore/edk2/blob/master/SecurityPkg/Include/Guid/TcgEventHob.h)
- [EDK2 PI boot mode values](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Pi/PiBootMode.h)
- [EDK2 Security2 authentication callback contract](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Ppi/Security2.h)

## Paths assessed against primary sources

| Path | Evidence and current disposition |
| --- | --- |
| coreboot board replacement | Tiger Lake SoC support exists, but no upstream 7506/0VK62X board port was established. Boot Guard acceptance and board initialization remain separate requirements. |
| deguard | The documented ME 11 Skylake/Kaby Lake technique does not establish support for this CSME 15 Tiger Lake machine. No downgrade or patch was attempted. |
| LinuxBoot DXE retrofit | A documented intermediate firmware architecture. Here, later-volume digests are present inside the signed IBB, and the original comparison rejects a changed DXE volume in the tested path. A usable handoff needs further investigation. Even success would initially retain Dell initialization. |
| CVE-2022-0004 / INTEL-SA-00613 | Intel lists 11th-generation mitigation at CSME 15.0.40; this machine reports 15.0.50.2633. This does not establish an available INIT bypass. CPU debug protection is a separate recommendation and its physical availability/authentication remains unproven. |
| CVE-2024-52541 / DSA-2025-021 | Dell lists Inspiron 7506 fixed at BIOS 1.33.1. This machine is 1.35.0. No working exploit on this revision established. |
| CVE-2024-38796 / DSA-2025-044 | Dell lists Inspiron 7506 fixed at 1.35.0, matching the installed version. This advisory does not establish a usable PE loader defect here. |
| Ordinary BIOS rollback | Dell's 1.35.0 release notes forbid rollback to 1.33.1 or earlier. This rules out assuming that the supported update workflow can simply restore vulnerable versions; actual antirollback fields are not yet fully decoded. |
| Debug/maintenance mode | Manufacturing mode is disabled and fuse-commit status set. One CPU debug-disable flag being clear does not establish privileged debug access. Intel documents separate lifecycle, authentication and unlock restrictions. |

These checks are not an exhaustive proof that no defect exists. The next narrow
research target is the trust-chaining policy: determine how hash/HOB algorithms,
volume authentication states and recovery-mode selectors are chosen, then test
those branches on copied firmware. A viable path must also solve flash write
authorization and restoration, not just make an offline comparison return true.
No speculative firmware write or lock modification is justified by this pass.

At the final physical check, the Dell retained boot ID
`e9c7e997-89ef-4ca3-b57a-aa7c7356ea72`; root SSH and Companion watcher were running,
the healthy boot ID matched, and the BIOS backup hash remained unchanged.
Research dependencies were installed on the SSD, not into board flash.

## Reproduce the file-only analysis

The pinned UEFIExtract source/build must already exist on the authenticated Dell.

```powershell
python tools/map-uefi.py
python tools/analyze-uefi-map.py
python -m venv artifacts/research-venv
artifacts/research-venv/Scripts/python.exe -m pip install unicorn==2.1.4
artifacts/research-venv/Scripts/python.exe tools/test-trust-chain.py
```

Sources:

- [Pinned UEFITool parser](https://github.com/LongSoft/UEFITool/tree/dac91b26733ca21cb204e41614c1b8c81cc50860)
- [LinuxBoot UEFI/DXE architecture](https://www.linuxboot.org/page/faq/)
- [coreboot documented deguard prerequisites](https://doc.coreboot.org/soc/intel/deguard.html)
- [Intel Boot Guard/INIT advisory](https://www.intel.com/content/www/us/en/security-center/advisory/intel-sa-00613.html)
- [Intel debug lifecycle and authentication](https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/secure-coding/intel-debug-technology.html)
- [Dell BIOS weak authentication advisory](https://www.dell.com/support/kbdoc/en-us/000258429/dsa-2025-021)
- [Dell EDK2 loader advisory](https://www.dell.com/support/kbdoc/en-td/000270384/dsa-2025-044)
- [Dell 7506 BIOS 1.35.0 release and rollback restrictions](https://www.dell.com/support/home/es-es/drivers/driversdetails?driverid=w5tw0)
- [Unicorn CPU emulator](https://github.com/unicorn-engine/unicorn)

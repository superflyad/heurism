# Dell update authorization and EC research, 2026-09-26

This pass investigates persistent firmware ownership on the connected Inspiron
7506, board 0VK62X. Full replacement firmware control remains unproven. The
current installed Companion management system continues to boot through Dell
firmware.

## Live write authorization

Read-only PCI/MMIO inspection produced BIOS_SPI_BC `0x100008aa`:

| Field | Observed | Meaning from Intel 500-series on-package PCH volume 2 |
| --- | --- | --- |
| WPD, bit 0 | 0 | BIOS writes disabled |
| LE, bit 1 | 1 | Changing WPD raises SMI; EISS locked until platform reset |
| EISS, bit 5 | 1 | BIOS write requires WPD and CPU InSMM.STS |
| BBS, bit 6 | 0 | Boot destination SPI |
| BILD, bit 7 | 1 | BIOS interface selection locked |
| ASE_BWP, bit 11 | 1 | Blocked BIOS writes can raise asynchronous SMI |

HSFSTS reports descriptor valid, configuration locked, no active descriptor
override strap and write-status operations disabled. PR0–PR4 are zero. These
observations explain why empty protected-range registers do not establish root
flash write permission. Source is document 631120-002, printed pages 320–324,
not a register definition taken from a newer PCH generation.

All eight CPU threads returned MSR 0x110 = `0x3`; MSR 0xCE =
`0x4043df0811c00`; MSR 0x13A = `0x30000007f`. The BIOS Guard enable bit is set;
the low lock bit is also set under the published research interpretation.
No MSR writes or BIOS Guard update invocation were performed. The Linux `msr`
module was loaded to expose its read interface.

The saved ROM contains `BiosGuardServices`, GUID
`6D4BAA0B-F431-4370-AF19-99D6209239F6`. Its extracted x64 PE body is 90,112 bytes,
SHA-256 `aed7c89f3093a1a90f6f9b8ab8e76661200966e39f931030eef80c9cdae7939c`.
The original code reads MSR 0x110 at RVA 0x1686 and tests bit 1 at RVA 0x168f.
An offline Unicorn 2.1.4 test executes that original branch with mocked RDMSR:
the physical value 3 takes the enabled path, while 0 and lock-only 1 do not.
Execution stops before calling any update service. This corroborates the enable
interpretation with this machine's code; it is not a write-acceptance test.

The recovery PEI and flash-update PEI modules were also extracted, hash-checked
and disassembled as files. Recovery contains explicit EFI_SECURITY_VIOLATION
returns, including physical addresses 0xfff9a389 and 0xfff9b770. Their complete
validation conditions are not yet decoded; an error constant alone does not
prove which checks protect every recovery path.

## Verified vendor baseline

Dell's published 1.35.0 `BIOS_IMG.rcv` was downloaded from its official server.
Its SHA-256 matches the published value:
`e649ae3fc5684a7bc7790bc3f2dca7f4235986d80af469ec60b926fc0e7fa9ed`.
Windows Authenticode reports Valid, signer Dell Technologies Inc., timestamp
DigiCert Timestamp 2024. This validates the downloaded PE envelope under the
host's trust policy; it does not validate every inner PFS signature or establish
that a modified image would be accepted by the board.

BIOSUtilities was pinned to `70c3a0852a6aa2643c8114ea73bc833e3b4cff0d` and
executed only as a file parser in a dedicated ignored artifact directory. It
extracted 40 files without reported parser errors. The optional BIOS Guard
script disassembler was unavailable, so script semantics remain unverified.
The package includes BIOS, CSME, EC, Cypress port-controller, Thunderbolt retimer,
BiosConnect and board-map components. There are 18 extracted signature files;
their cryptographic validation was not performed.
That was the initial extraction result; the original signed-container checks
below now verify all 18 inner signatures.

The reconstructed BIOS is 16 MiB, SHA-256
`3e19bd1cfd839d9104b3661f59b4c70b870471709fe777d96e2a4385c9733196`.
Its four IBB segments are byte-for-byte identical to the physical ROM backup;
all four saved manifest digest algorithms match. Of 31 mapped firmware volumes,
28 are identical; differing volumes start at 0x0, 0xd0000 and 0x130000.
Of 4,096 pages of 4 KiB, 4,018 match. The differences were not all classified,
so this vendor file must not replace the machine-specific backup.
BIOSUtilities reconstructs the flat BIOS from 256 PFAT blocks, stripping the
block headers/scripts. A neighboring extracted `.sig` must not be assumed to
authenticate that reconstructed flat file.

The verified baseline provides code for further analysis and a vendor recovery
candidate. It is neither a complete 24 MiB backup nor a demonstrated independent
restoration method for a machine whose startup code no longer executes.

## EC format lead

The package contains two identical main EC images labelled 1.11.0 and two
different backup images labelled 1.0.0. All four start with `PHCM`, header version
1. Each has a valid SHA-256 of its first 64 header bytes stored at offsets
64–95. A changed header byte fails that integrity comparison for every image.
Load and entry fields are 0xe0000 and 0xe0001; payload-offset field is 0xc0;
raw flags byte 6 is 0x80. The original pass left that flag undecoded; the
controlled generator tests below now establish its file-format meaning.

Chromium's Microchip packers document the same header marker and address-field
family. Its MEC152x packer uses version 2 with different security/layout rules;
its older MEC17xx packer emits version 0. Neither is a validated version-1
decoder for these Dell images. That comparison alone did not establish security
flags, exact physical chip model, executable plaintext or an EC programming
method. No EC command or firmware write was attempted.

### Version-1 generator controls

Microchip's official CPGZephyrDocs repository was pinned to
`e98e827058ac3653a2368550d27bb112112fe948`. Its MEC1501 generator is version 3.5,
dated 2018-01-15; the Windows executable SHA-256 is
`bf6a5f07d5e373173beed53215e06c32e4395f60d5cec1233835a892a8354512`.
It was run only against synthetic 4 KiB data in a dedicated local scratch
directory, producing 1 MiB files. Public vendor demonstration keys were used;
they are unrelated to this machine's credentials or Dell signing keys.

`tools/test-ec-format.py` passed all eight combinations of `FwAuthtic`,
`FwEncrypt` and `UseECDSA`. It confirms header version 1, payload offset 192,
length in 64-byte units, authentication-request bit 6 and encryption-request
bit 7. `UseECDSA` changes the signature representation independently of those
flags. Encrypted fixture bytes differ from their known plaintext; unencrypted
fixtures remain identical. Non-ECDSA headers contain the expected SHA-256;
ECDSA headers do not match that plain-hash representation.

Against the four actual Dell EC files, `tools/analyze-ec-images.py` now also
passes SHA-256 comparisons over the encrypted payload plus its following
64-byte key header. The stored digest begins 64 bytes after the payload ends.
Every key header consists of valid big-endian NIST P-256 coordinates, matching
the encrypted generator fixture's key-header convention. Changing a payload
byte or a key-header byte fails the saved digest comparison in every image.
The version-1 generator's README specifies AES-256-CBC encryption with an
ECDH-derived key/IV and a device-side private key.

Thus the Dell files request encryption and leave the authentication-request
flag clear. This does **not** establish that physical authentication is disabled:
Microchip explicitly documents that an OTP authentication requirement overrides
the header flag. The exact physical chip and OTP state remain unknown. No Dell
EC payload has been decrypted, and neither key ownership nor EC write acceptance
has been demonstrated. Unused signature bytes and the backup files' additional
16-byte trailer remain unclassified. The MEC1501 datasheet refers readers to a
separate Boot ROM document; its general features are not proof of Dell wiring.

### Recovery cryptographic primitive

`tools/test-recovery-rsa.py` runs original IA32 code from the verified physical
ROM in Unicorn 2.1.4: context initialization at `0xfff97c50`, hash update at
`0xfff97d34`, and verification at `0xfff97d59`. Static analysis and a synthetic
RSA-2048 control establish flags `0x121` as the RSA/SHA-256/PKCS#1 v1.5 path.
The fixture's private key was discarded; its public modulus, exponent, message
and signature are retained privately for repeatable testing. Independent Python
modular exponentiation checks the expected PKCS#1 encoded digest first.

The unmodified firmware code accepted the original fixture and rejected changed
message, signature and public exponent controls. No crypto helper was replaced.
An initial instruction limit was insufficient; the final bounded runs return
normally within a 300-million-instruction/10-second limit per invocation.
These are primitive-level tests, not the complete recovery flow: trusted-key
selection, stream/container handling, rollback policy, BIOS Guard authorization
and reset-time Boot Guard acceptance remain separate conditions.

### Actual Dell key and package signatures

The verified ROM's `BindingsPei` module contains an eleven-entry static key list
at BIOS offset `0xf95c20`, with eight-byte pointer pairs. The package-signing
key record is at `0xf957c0`, GUID `540b32b0-808d-47e1-a188-76aa7cf1bb93`.
Its public modulus and padded exponent occupy 512 bytes at `0xf94458`; the
record specifies length 512 and flags `0x221`. The key's SHA-256 is
`1cd3d13d3babf1b27738b1db44610e80f79651ab32c17d0c47d7e392b7690d3b`.
`BindingsPei` was extracted with the same pinned UEFI parser and has SHA-256
`80fef2eaf164eec3abef7f99bfb078384d3df4c1c7472e341b06093f6a497dbc`.
The separately extracted `PlatformInitAdvancedPreMem` module consumes the
key-registry interface GUID `acdc2cf7-5524-4d14-a42a-01992ac613be`.
The complete construction and lifetime of the recovery receiver's descriptor
have not yet been emulated or captured on physical hardware.

`tools/verify-recovery-signatures.py` parses the original compressed PFS sections
directly with bounded decompression, explicit entry revisions and container
bounds. Its layout follows the pinned BIOSUtilities parser. It verifies all 18
original inner signatures with that ROM public key: RSA-2048 PSS, SHA-256,
MGF1 SHA-256 and a 20-byte salt. All 18 one-byte payload mutations fail.
This includes every EC image, BIOS metadata, CSME, sensor-hub, retimer,
port-controller, BiosConnect, board-map and package-information payload.

The original BIOS signed data is a 17,079,024-byte container, rather than the
16,777,216-byte flat image reconstructed by the earlier extractor. Its signature
verifies against the original bytes. The previous failure to verify against the
flat image was expected and does not indicate a signature weakness. The raw
signed container and signature are preserved under ignored `artifacts/firmware/vendor`.

`tools/test-recovery-pss.py` independently runs the ROM's original GUID lookup
at `0xfff9b3d7`: all eleven actual key IDs resolve to their expected indices;
an unknown ID is rejected. It uses the actual static ROM list with a synthetic
receiving descriptor. The same unmodified context/hash/verifier functions then
accept the real EC v1.11.0 PSS signature and reject changed payload and signature
controls. No crypto helper is replaced. This confirms the primitive and key-list
behavior, not every caller's trust decision, a running EFI update or a flash cycle.

### Recovery key coverage under Boot Guard

`tools/analyze-recovery-key-coverage.py` maps all eleven 512-byte keys, eleven
28-byte records and the 88-byte pointer list into the saved signed IBB segments.
All 23 objects lie inside the `0xf70000..0xffffff` BIOS segment. Their original
SHA-1, SHA-256, SHA-384 and SM3 IBB digests match the saved manifest. Changing one
byte in each object makes every digest differ: 92 negative comparisons pass.
Only temporary memory is changed; no candidate ROM or flash write is produced.

The resulting conclusion is narrow but consequential: editing a recovery key
is also editing signed startup bytes. It requires resolving Boot Guard acceptance
as well as firmware write authorization. Likewise, recomputing an EC header or
payload integrity hash does not supply the Dell signature checked by recovery.
This pass does not establish a writable owner-key slot, OTP change, supported
custom-firmware installation procedure or authentication bypass.

Next bounded targets are the physical EC's identity/OTP policy, the recovery
registry descriptor's construction and caller policy, and the relationship between BIOS
Guard scripts, EC flash access and rollback policy. These are separate from
reset-time Boot Guard acceptance. A successful method must satisfy both boot
and write authorization and preserve a usable restoration path.

## Tools and private evidence

### BIOS Guard block signatures and script dispatch

`tools/verify-bios-guard-blocks.py` independently parses the original signed
17,079,024-byte BIOS container. It reconstructs all 256 64-KiB blocks without
overlap or gaps; the resulting SHA-256 is
`3e19bd1cfd839d9104b3661f59b4c70b870471709fe777d96e2a4385c9733196`,
identical to the pinned parser's flat BIOS output.

236 blocks have SFAM set and their own RSA-2048 PKCS1v1.5 SHA-256 signatures.
All verify over the complete BIOS Guard header, script and payload. Their common
public key matches installed registry entry 1,
`dfcfaef4-ed87-4ef3-b1b7-2b06f7ac74eb`, at BIOS offset `0xf94950` inside the
signed IBB. Matching this registry key is not a complete validation of the
CPU's BIOS Guard Platform Data Table or its runtime policy.

The other 20 blocks cover BIOS offsets `0x000000..0x13ffff` and have SFAM clear
with no inner block signature. They remain inside the signed outer PFS container.
The test verifies that outer PSS signature before processing any block. It
rejects header, script, payload and signature mutations at the applicable
signature layer: 944 inner-block controls and 80 outer-container controls,
1,024 total. Absence of a block signature is not evidence of an unauthenticated
installation route.

Every script is disassembled using
[BGScriptTool at c53fe9d94305e74f706e1224f588fbfa55b66c6a](https://github.com/platomav/BGScriptTool/blob/c53fe9d94305e74f706e1224f588fbfa55b66c6a/big_script_tool.py).
The decoder SHA-256 is
`a50942d65c8eac05dec5b4826429affb9aaeb52b0e08c95744e2e6b25f6abf1e`.
Scripts read the SPI descriptor to derive the BIOS base, compare existing pages,
and erase/write different pages in 4-KiB units, with bounded retries. Scripts
are decoded as data and never executed on the laptop or through an updater.

`tools/test-bios-guard-dispatch.py` executes copied original x64 instructions
in Unicorn 2.1.4. Three synthetic dispatch cases confirm that the BSP code
supplies `context+0x28` to MSR `0x115`, requests execution with MSR `0x116=0`,
and stores the complete returned 64-bit value at `context+0x58`. All MSR
instructions are intercepted inside the emulator; no hardware MSR is written.
Sixteen result cases verify the original mapping to EFI success, unsupported,
invalid parameter and device error. This exercises the copied argument/result
path, not CPU authentication, multiprocessor rendezvous, I/O or flash writes.

### Actual static recovery registry provider

BindingsPei calls its PEI InstallPpi service at `0xfff92ee1` with the descriptor
list at `0xfff955b8`. The list contains the recovery-key registry PPI
`acdc2cf7-5524-4d14-a42a-01992ac613be` with interface `0xfff94f50`, whose actual
contents are revision 1, eleven entries and pointer `0xfff95c20`. It also
contains policy PPI `8251115f-c0e5-49de-9936-1846f238206f` pointing to
`0xfff957dc`, starting with bytes `01 01`.

`tools/test-recovery-registry.py` supplies these actual ROM interfaces to the
original recovery receiver functions with a mocked PEI LocatePpi service. The
receiver reads authentication-required as true and obtains the actual registry
pointer. With the policy PPI missing but registry present, its initialized
authentication-required flag stays true. With both absent, registry lookup
returns EFI_NOT_FOUND; clearing the flag in that fixture does not provide a
working key registry. Twelve lookup cases using the actual registry descriptor
resolve all eleven keys and reject an unknown GUID. This replaces the earlier
synthetic registry descriptor for these tests; runtime PPI lifetime and the
complete recovery caller still have not been captured on physical hardware.

The remaining installation requirements are substantive: an implementation for
this board, startup acceptance under its configured Boot Guard policy, an
authorized write path and a restoration method independent of the candidate
firmware. None of these offline tests supplies OEM signing authority, changes
fuses or establishes an owner-key enrollment mechanism. Root management of the
installed OS is functioning; complete replacement of Dell firmware is not
achieved.

`fetch-firmware-references.py`, `verify-recovery-signature.ps1`,
`analyze-recovery-package.py`, `verify-recovery-content.py`,
`inspect-bios-guard.py`, `extract-update-modules.py`, `test-bios-guard-state.py`
`test-ec-format.py`, `test-recovery-rsa.py` and `analyze-ec-images.py` implement these checks. All Python files passed
compilation. Extraction deliberately preserves prior outputs; it will not
overwrite an existing extraction directory.
The subsequent actual-signature and coverage tools are
`verify-recovery-signatures.py`, `test-recovery-pss.py` and
`analyze-recovery-key-coverage.py`; they also pass Python compilation.

Reports, vendor files, module copies and disassembly are under
`artifacts/firmware`, excluded from Git. Root SSH, healthy boot identity and
Companion watcher were retained throughout the pass. No vendor executable,
capsule update, flash erase/program cycle or protection change was invoked.

Sources:

- [Intel 500-series on-package PCH volume 2](https://cdrdv2-public.intel.com/631120/631120-002.pdf)
- [Dell 7506 BIOS 1.35.0 package and published hashes](https://www.dell.com/support/home/es-es/drivers/driversdetails?driverid=w5tw0)
- [Pinned Dell PFS parser](https://github.com/platomav/BIOSUtilities/blob/70c3a0852a6aa2643c8114ea73bc833e3b4cff0d/biosutilities/dell_pfs_extract.py)
- [BIOS Guard MSR research; studied platform Lenovo P50, not this Dell](https://airbus-seclab.github.io/embedded_controller/BH2019-Slides-Breaking_Through_Another_Side_Bypassing_Firmware_Security_Boundaries_from_Embedded_Controller-matrosov-gazet.pdf)
- [Chromium Microchip MEC17xx header packer](https://chromium.googlesource.com/chromiumos/platform/ec/+/08f5a1e6fc2c9467230444ac9b582dcf4d9f0068/chip/mchp/util/pack_ec.py)
- [Chromium Microchip MEC152x version-2 packer](https://chromium.googlesource.com/chromiumos/platform/ec/+/08f5a1e6fc2c9467230444ac9b582dcf4d9f0068/chip/mchp/util/pack_ec_mec152x.py)
- [Pinned official Microchip version-1 generator documentation](https://github.com/MicrochipTech/CPGZephyrDocs/blob/e98e827058ac3653a2368550d27bb112112fe948/MEC1501/SPI_image_gen/README.txt)
- [Pinned official MEC1501 datasheet](https://github.com/MicrochipTech/CPGZephyrDocs/blob/e98e827058ac3653a2368550d27bb112112fe948/MEC1501/MEC1501_Datasheet.pdf)

## Unsigned-region startup investigation, September 26 continuation

The original BGPDT constructor at `0xfffd2c4b` was executed offline with
synthetic PEI services. The BIOS-region query uses the physical `FREG1`
observation: base `0x800000`, size `0x1000000`. Both EC-present and EC-absent
fixtures produce a 176-byte policy with signature-required range
`0x940000..0x17fffff`, corresponding to BIOS offsets `0x140000..0xffffff`.
The earlier twenty unsigned vendor BIOS Guard blocks cover exactly the area
before that boundary. This correspondence supports an unsigned-area lead;
it does not establish an accessible physical write service.

The original constructor executes its key conversion and SHA-256 code.
Reversing the actual recovery registry key's 256-byte modulus into little
endian and appending the four-byte little-endian exponent produces the first
two policy key hashes:
`8a4bbc34c81efcc1149f88caa8ec11a963761c46d50ec5e84c90d95e1cbaf5a5`.
A third ROM constant is
`190b33f8de3aa79b57adb245860e7f0e406280228f0492ec874481d9efed9fa3`;
no corresponding owner signing authority was established. Policy attributes
are `0x30` or `0x32` depending on the EC-presence fixture, and BIOS Guard SVN
is 1. Reading candidate policy-hash MSRs `0x111..0x114` returned EIO on all
eight CPUs. Therefore the reconstructed table has **not** been compared with
the live CPU's programmed policy hash. These reads made no register changes.

The official [Tiger Lake FSP integration guide](https://raw.githubusercontent.com/intel/FSP/98426bfd958eb7397c474ff5d440dc9a1d146215/TigerLakeFspBinPkg/Docs/TigerLake_FSP_Integration_Guide.pdf)
documents BGPDT hashes and policy attributes. The downloaded guide is pinned
to Intel/FSP revision `98426bfd958eb7397c474ff5d440dc9a1d146215`, SHA-256
`454808a8469d6a85645feb1c0ab6a1e59a13a3bbd73c64b17f68ef32d2e3238f`.
The MSR addresses were a read-only research hypothesis from the earlier
platform research, not a claim that the guide specifies those addresses.

### Empty firmware volume and actual PEI rejection

BIOS offset `0x110000` contains an empty 128 KiB FFSv2 volume named
`8b570fe1-48bc-4830-92d5-244b1b93c2e4`, inside the reconstructed unsigned
range. Executing original ReportFvPei entry `0xfffc009d` and its notification
callback `0xfffc010c` with mocked PEI services publishes the volume at
`0xff110000` during normal-startup fixtures. Recovery mode omits it. This
is actual publisher code, but publication alone is not execution permission.

`tools/test-fv-authentication.py` runs original DellTrustChainingPei
notification `0xfffb061f`, original BIOS-info searches, original digest
comparison and Security2 callback `0xfffb095c`. It supplies the actual ROM
BIOS-info table at `0xfffb9460` and digest table at `0xfffb19f0`; PEI services,
boot mode, initial authentication cache and EFI allocations remain fixtures.

| Volume fixture | Notification authentication | Security2 result |
| --- | --- | --- |
| Empty unsigned `0x110000..0x12ffff` | 0, no authenticated cache entry | EFI_SECURITY_VIOLATION, execution deferred |
| Original protected `0x680000..0x77ffff` | 2, authenticated entry | EFI_SUCCESS |
| Same protected volume, one byte changed | 10, failed entry | EFI_SECURITY_VIOLATION, execution deferred |

The unsigned volume is absent from the actual BIOS-info protected-volume
records. In this normal-boot fixture it cannot supply a PEI module accepted
by Dell's Security2 policy. A successful publication callback status would
have concealed this barrier if we had not tested the separate authentication
callback. No candidate module was written into the physical volume.

### DXE origin policy remains a separate question

Pinned UEFIExtract copied SecurityStubDxe, DxeIpl and DxeCore from the verified
backup. SecurityStubDxe SHA-256 is
`73401dd92e416dc062cccd54128bd40555c1105511e07e03323637a2eee6e728`.
Its original image-verification handler at RVA `0x4e30` calls origin
classifier `0x32e8`. A successful EFI FV2 protocol lookup/open selects origin
1 and returns success before image parsing. The offline test preserves those
original branches and mocks variable reads, protocol services and error
reporting. With a NULL image buffer:

- FV2-origin fixture, SecureBoot enabled: EFI_SUCCESS.
- No FV2-origin fixture, SecureBoot enabled: EFI_INVALID_PARAMETER.
- No FV2-origin fixture, SecureBoot disabled: EFI_SUCCESS.

This **does not prove** that the unsigned volume gets an FV2 protocol on the
physical Dell, that DXE dispatch accepts its drivers, or that a write service
will permit replacing its contents. PEI and DXE policies must not be conflated.
The [PI HOB definitions](https://uefi.org/specs/PI/1.10/V3_HOB_Code_Definitions.html)
and [reference SecurityStub implementation](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Universal/SecurityStubDxe/SecurityStub.c)
provide interface context; the module tests use Dell's copied implementation.

Reproduction tools: `test-bios-guard-policy.py`, `test-fv-publication.py`,
`test-fv-authentication.py`, `extract-startup-modules.py`, and
`test-dxe-origin-policy.py`. Reports remain ignored under `artifacts/firmware`.
Root SSH and the Companion watcher were checked and remained started on boot
`e9c7e997-89ef-4ca3-b57a-aa7c7356ea72`. No physical flash program/erase,
BIOS Guard invocation, protection change or reboot occurred in this pass.
Complete replacement of Dell firmware remains unachieved.

### Physical startup inventory — 2026-09-26

Built and host-tested `boot/firmware_probe.c`, then booted it once from the
internal ESP using BootNext. It calls read-only FV2/FVB discovery, attribute,
header and file-enumeration services. Its only file writes are the ESP report;
it does not invoke firmware-volume writes, erases or protection changes.
The deployed EFI SHA-256 is
`40d9515af35c1d87ed86f22c2ed6581b3b6e016788488da23f659079b44ee89a`.

The physical report finished with `PROBE_COMPLETE` and enumerated 15 FV2
handles, each with successful FVB header reads and complete file enumeration.
Neither physical base `0xff110000` nor volume-name GUID
`8b570fe1-48bc-4830-92d5-244b1b93c2e4` appeared. Thus the earlier offline DXE
FV-origin acceptance fixture has no matching exposed candidate on this boot.
This does not prove that every possible startup phase lacks such a path.

The exposed volume at `0xff0d0000` reports FV attributes `0x4f0df` and FVB
attributes `0x4fedf`; the microcode volume at `0xffd10000` reports `0x4f0ff`
and `0x4feff`. Attribute flags alone are not evidence of an accepted physical
flash write. No write was attempted.

The HOB header reports boot mode 1. The full HOB walk rejected its end-range
check (`HOB_BAD_RANGE`), so this pass did not inspect authentication HOBs.
Adding mode 1 to the pinned original ReportFvPei emulator still publishes
the candidate at `0xff110000`, with synthetic PEI services. Therefore boot
mode alone does not explain its absence from the physical FV2 inventory;
the exact reason remains unresolved.

After reboot, authenticated root SSH returned on boot
`5916ac8a-350c-4bae-b1e0-746666687d71`; its healthy-boot marker matches, and
sshd, companion-watch and companion-boot-health are started. BIOS remains
1.35.0. BootCurrent is 0000 and the kernel command line uses the stable
Companion fallback. The probe did not log LoadImage/StartImage status, so
direct handoff to the main GRUB loader is not proven by this result.
After verification, the temporary probe boot entry 0001 was removed and
the original normal BootOrder `0005,0000` restored. BootNext is consumed;
the probe will not run again automatically. Its binary and report are retained
on the ESP for inspection. Root SSH and the watcher remained started.

The exact physical report is saved as `artifacts/firmware/fv-probe-01.txt`,
SHA-256 `c8a5de80bd5e6937c9c4c5b20f5b7f3b3cf92fa8af1c38b026e16c48a84686f6`.
`tools/verify-live-fv-probe.py` checks report completeness, all 15 inspections,
candidate identity and the same-mode offline comparison, saving a separate
JSON verification report. Host fixtures validate malformed HOB/device-path
bounds, FV enumeration, file persistence and handoff calls; firmware write
function pointers are deliberately absent from those fixtures.

This physical check establishes continued management access and a negative
result for the proposed exposed unsigned-volume path. Complete firmware
control and a usable firmware replacement write path remain unverified.

### Alternative route: capsules and proprietary update services

The second physical probe inventories UEFI Firmware Management Protocol (FMP)
providers using only GetImageInfo. Host tests cover successful descriptors,
malformed descriptor bounds and missing FMP providers; SetImage, CheckImage,
SetPackageInfo and UpdateCapsule are never called. The deployed EFI hash is
`caa041a7e963177c489f9c7532f57bfc3f00c35f7bb2ce3771686dcb9ed8e2d7`.
The physical report ends in PROBE_COMPLETE but LocateHandleBuffer(FMP)
returns EFI_NOT_FOUND with zero handles. This is a normal-startup observation,
not a proof that the machine has no update backend.

Live OS-side ESRT nevertheless lists four update resources. The system resource
is `417e8ca2-c8c6-4a7c-be26-e3bc703c2cbb`, current and lowest supported version
74496 (`0x12300`, encoding 1.35.0). The other three resources report versions
186, 28675 and 28753, with equal minimum versions. Their identities have not
been established from an authoritative source. CapsuleFirmwareUpdate and
AllowBiosDowngrade are Enabled. These settings do not override the reported
minimum version or prove acceptance of a modified image.

Fastboot is Thorough, despite the earlier HOB header boot-mode value 1.
Do not equate that HOB value directly with Dell's Fastboot setting.
Signed Firmware Update is documented in the model's Overview as status;
it is not among the exposed firmware-attributes controls. Dell documents
Custom Mode for PK/KEK/db/dbx, but those Secure Boot databases are separate
from Intel's manufacturing-provisioned Boot Guard trust anchor. Enrolling
owner boot-loader keys would not establish permission to replace the IBB.

Sources: [Dell 7506 system setup options](https://www.dell.com/support/manuals/en-us/inspiron-15-7506-2-in-1-laptop/inspiron-7506-2n1-silver-service-manual/system-setup-options?guid=guid-cb19996e-6cf5-47b9-be58-1a039da03b99&lang=en-us),
[Intel firmware key roles and immutable Boot Guard policy](https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/resources/key-usage-in-integrated-firmware-images.html),
and [FMP interface contract](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Protocol/FirmwareManagement.h).

Extracted CapsuleRuntimeDxe, DellFlashUpdate2Dxe and RecoveryImageReadWriteV2
from the hash-verified ROM, using the pinned parser. CapsuleRuntimeDxe
SHA-256 is `4db34759e75c0502ecb4057d4988d02ba5e5d713001ed838c86f41786f11e3aa`.
Its entry assigns runtime UpdateCapsule RVA `0x1388` and QueryCapsuleCapabilities
RVA `0x1628`. The new emulator loads PE sections at their virtual addresses;
it executes only the query and forbids entering UpdateCapsule.

Seven minimal-header controls show the original query accepts a persistent
populate-system-table capsule with the system GUID, or even an unknown GUID,
reporting a 100 MiB maximum and warm reset. An unknown GUID without the populate
flag returns EFI_UNSUPPORTED. Populate without persistence is invalid. An
empty FMP capsule without persistence gets a 10 MiB limit and cold reset,
whereas FMP plus populate is invalid. There is no signature payload in these
fixtures. Acceptance proves only transport capability reporting, not update
authentication or an accepted flash write. Initial size limits and an empty
ESRT cache are fixtures, not copied live runtime state.

DellFlashUpdate2Dxe SHA-256 is
`4f40dce428d84aa1b8eb29a84d9f9698c811fd5610f338f0144fe9f0b94d2cc3`.
Copied-code inspection identifies a separate validation dispatch at RVA
`0xec40`, with protocol GUIDs `2c650f84-2fa4-453a-906b-100894ffad19` and
`57fa1a50-4b98-4547-b5a6-bedf9511b1ab`, and a policy reader at `0xed34`
looking up `d67be471-df7c-4a3a-af56-ad9ac8fb7ff8`. The validator includes
explicit EFI_SECURITY_VIOLATION branches. These proprietary interfaces are
not documented FMP methods; their complete contracts, live presence and update
authorization have not been established. An error constant alone is not a
complete rejection proof.

The subsequent physical policy probe below establishes which of these
interfaces are present and the policy byte; it does not establish their full
method contracts or authorize modified firmware.

We can implement our own management protocol. Registering it supplies a software
interface; it does not grant access through chipset flash protections or change
the hardware boot trust anchor. A useful implementation still needs a verified
backend capable of writing the intended region and booting the resulting image.
The remaining capsule question is that backend's authorization, not whether we
can create an FMP handle ourselves.

The Dell reconnected on healthy boot `ec785cc7-c345-4206-820a-f82fe3918bb1`.
Root SSH, watcher and boot-health are started. Temporary probe entry 0001 was
removed and BootOrder `0005,0000` restored. BIOS remains 1.35.0, with no capsule
submission, flash program/erase, Secure Boot key enrollment or protection change.
The physical report SHA-256 is
`c6e006fbdf92eb0eb9217df751d08f74c075ecfd4931bff474a63b5a713e568e`.

Reproduce with `tools/build-firmware-probe.ps1 -UpdateProbe`,
`tools/extract-update-modules.py --capsule`, `tools/test-capsule-query.py`
(pinned emulator), and `tools/collect-update-interfaces.py`. Physical deployment
is explicitly requested through `tools/run-firmware-probe.py --update-probe --reboot`;
it refuses an existing report, preserving the recorded run.

### Physical program gate and proprietary policy verification

On boot `ec785cc7-c345-4206-820a-f82fe3918bb1`, the new restricted SPI
test issued one hardware PROGRAM request for 64 bytes at flash address
`0x00911000` (BIOS offset `0x111000`). The page was first read and required to
be all `ff`; the requested data was exactly the data already present. There
was no erase, write-enable command, protection change, PCI write, arbitrary
address argument or retry. This is an actual program request, unlike the
earlier read-only register inspections.

The controller returned `0x3f04e803`, including FCERR. The page remained
unchanged and BIOS control remained `0x100008aa`. Thus this direct out-of-SMM
backend denied this request; no flash write permission was demonstrated.
The test does not rule out Dell's authorized SMM update backend.

An independent full BIOS read afterwards has SHA-256
`3e6c54c0edbe0cda38ea91cd8363d40c3f46526dd3bc26875072d5cd4360c745`.
Compared with the earlier backup, 4,666 changed bytes are confined to the UEFI
variable area (`0x35cb6` through `0xc5dcb`); intervening boot-entry and other
variable operations mean this is not a same-boot immediate full-image
before/after comparison. All code from BIOS offset `0xd0000` onward and the
fixed program-test page match the earlier backup. The immediate same-boot
page comparison is the direct evidence that the tested page stayed unchanged.

The third UEFI inventory was built from `boot/policy_probe.c`, SHA-256
`b998607b168bdc7704ad65427356c189d934486ea667067a1138e4543408fcf3`.
It calls only LocateProtocol and reads the exact fields used by the original
DellFlashUpdate2Dxe policy reader at RVA `0xed34`; it does not call proprietary
methods, submit capsules or change protocol fields.

| Protocol | Physical result |
| --- | --- |
| `d67be471-df7c-4a3a-af56-ad9ac8fb7ff8` | Present; byte at offset 1 equals 1 |
| `ef48ffe8-9e24-4eb8-828d-2ec11a9df8dd` | Present; version at offset 4 equals 5 |
| `2c650f84-2fa4-453a-906b-100894ffad19` | Present; primary validation dispatch branch available |
| `57fa1a50-4b98-4547-b5a6-bedf9511b1ab` | EFI_NOT_FOUND; alternate legacy dispatch absent |
| `71db7b7e-4165-48fa-ac9d-f9af4cefc534` | Present; undocumented methods not invoked |

The physical report ends in PROBE_COMPLETE and has SHA-256
`8061f920f02a6b9e52fad212a46827b0685ee7e4b6ca8cbbb3d01223a80392e3`.
Five offline controls execute the actual saved `0xed34` reader with mocked
LocateProtocol, loading PE sections at their virtual addresses. With physical
values it returns success and sets the caller's signature-required flag to 1.
A hypothetical byte 0 disables that flag in the fixture; it is not the live
value. When the policy protocol is absent, a fallback version below 5 clears
the flag, while version 5 or an absent fallback leaves it enabled. These
controls verify policy selection, not the entire signature verifier or an
accepted flash update. No live field was patched.

Separate startup controls verify all protected IBB bytes in the latest
hardware read still match the signed backup. One altered byte in each of
four IBB segments fails each of four stored hash algorithms: 16 negative
controls, with the unchanged baseline passing. A fresh valid RSA-3072 owner
public key fails the existing Key Manifest's BPM key binding; the vendor key
passes. These are offline cryptographic checks. Neither a hardware-fused
OEM-key comparison nor a physical modified-firmware boot was performed.
They do not claim that every possible later-stage customization is prevented.

Root SSH reconnected after the probe on healthy boot
`77c48e41-55b5-4a51-862e-b7771c02d93c`, without physical assistance. The watcher
is running, temporary Boot0001 was removed, and BootOrder `0005,0000` restored.
There was one denied unchanged-data PROGRAM request in this run; no erase,
replacement firmware installation, key enrollment or protection change.
Full firmware ownership remains unachieved. A management interface alone
does not solve these demonstrated authorization and startup-binding gates.

Evidence: `artifacts/firmware/spi-write-gate-live.json`,
`spi-write-gate-comparison.json`, `policy-probe-verification.json`,
`update-policy-reader-controls.json`, and `startup-acceptance-controls.json`.
Reproduce offline controls with `tools/test-update-policy-reader.py`
(Unicorn 2.1.4) and `tools/test-startup-acceptance.py` (cryptography).
Build the policy probe using `tools/build-firmware-probe.ps1 -PolicyProbe`;
the deployment tool refuses an existing report. `tools/test-spi-write-gate.py`
also refuses to repeat a recorded physical test.

The [Tiger Lake register reference](https://cdrdv2-public.intel.com/631120/631120-002.pdf)
defines FCERR for prohibited flash accesses. The
[EDK II BIOS Guard guidance](https://tianocore-docs.github.io/EDK_II_Secure_Coding_Guide/draft/secure_coding_guidelines_intel_platforms/intel_bios_guard.html)
requires all processors in SMM for BIOS Guard updates; invoking its trigger
from an ordinary kernel driver would not establish an authorized update path.

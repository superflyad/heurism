# Motherboard storage and automatic startup

On 2026-09-27, offline execution of the saved Dell image-source resolver found
no direct loading of our variable through a raw flash address or variable GUID.
Our 2 KB extension remains verified in nonvolatile motherboard storage. Its
explicit execution is proven; automatic startup still uses an SSD bootstrap.

## What was tested

`tools/analyze-nv-dispatch.py` verifies the saved flash image and payload hashes,
checks the variable name/vendor GUID/data layout, and locates image-provider
references in Dell's saved DXE core. The extension is at BIOS-image offset
`0x40494`, following its variable name and header, inside the variable-store
volume with filesystem GUID `fff12b8d-7696-4c8b-a985-2747075b4f50`.
That record is not an FFS firmware file. Its vendor GUID does not register a
firmware-volume file or executable startup hook.

`tools/test-dell-nv-dispatch.py` runs the actual saved Dell x64 resolver
instructions at RVA `0x11308` in Unicorn 2.1.4. The DXE-core SHA-256 is
`8641816dae6964f620aeac118b0303376f1abe0d3e6be6f066e17cbf52d6c246`.
Allocation/device-path helpers and image-provider interfaces are mocked.
This is a bounded function test, not a whole-firmware boot simulation.

| Fixture | Result |
| --- | --- |
| Memory-mapped flash device path, no provider | No image; zero direct flash reads |
| Same path with boot policy enabled | No image; zero direct flash reads |
| Vendor device path containing our variable GUID | No image; zero direct flash reads |
| Variable-volume GUID plus owner GUID as FV file path | No image; zero direct flash reads |
| Same FV file path with a synthetic FV provider | Exact 2 KB payload returned |

All five assertions passed. The positive fixture demonstrates that the resolver
can accept a matching provider; it does not establish that Dell exposes such a
provider for NVRAM. The candidate address `0xff040494` was populated only in
emulated memory. No physical MMIO mapping or flash read at that address was
tested. No image was started in these resolver tests.

The resolver tries firmware-volume, filesystem, LoadFile2 and LoadFile services.
It does not treat a memory-mapped device path as permission to read arbitrary
memory. The earlier physical FV inventory did not expose the variable-store
format among its listed FV block interfaces. This supports the distinction
between variable storage and executable firmware files; it is not an exhaustive
proof that every vendor-specific route is absent.

Standard Driver#### OptionalData also does not supply an executable image: the
boot manager loads the image first, then attaches OptionalData as load options.
Putting PE bytes there would not supply the missing initial loader.
See the [UEFI boot-manager specification](https://uefi.org/specs/UEFI/2.11/03_Boot_Manager.html),
the pinned [EDK II driver-loading implementation](https://github.com/tianocore/edk2/blob/80d1e5f8474565ab198847325d951560d423d50b/MdeModulePkg/Library/UefiBootManagerLib/BmLoadOption.c),
and its [image-source resolver](https://github.com/tianocore/edk2/blob/80d1e5f8474565ab198847325d951560d423d50b/MdePkg/Library/DxeServicesLib/DxeServicesLib.c).

## Management and backups

`tools/backup-boot-variables.py` captured 13 selected Boot/Driver/owner variables
through pinned root SSH, including their attributes and exact bytes. This is a
local backup, not a firmware write or an automatic restore mechanism. It excludes
unrelated variables and private SSH keys.

At capture, current boot `1ba912c9-289e-4c0d-aa8c-03b18f40b52a` matched the
management healthy-boot marker. BootCurrent was 0005, BootOrder 0005,0000,
DriverOrder 0000,0001, and BootNext absent. All three installed management,
fallback and recovery EFI loaders matched their known SHA-256, and recovery
state was `companion_pending=0`. The owner payload retained SHA-256
`b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a`.

Evidence is under ignored `artifacts/research/nv-dispatch/` and
`artifacts/firmware/boot-variable-backups/20260927T132127Z/snapshot.json`.
Reproduction uses the saved, hash-checked firmware artifacts:

```powershell
python tools/analyze-nv-dispatch.py
& artifacts/research-venv/Scripts/python.exe tools/test-dell-nv-dispatch.py
python tools/backup-boot-variables.py
```

## Remaining work

There are two separate routes to investigate. The practical route is an SSD
startup component that retrieves and verifies the NVRAM extension, executes it,
and continues normal management if retrieval or execution fails. That would
automatically execute motherboard-stored code, while still depending on the SSD
for the first loader. It does not solve SSD independence.

This practical route is now [implemented and physically verified](nv-startup.md).
The independent route below remains unproven.

The independent route requires identifying an existing Dell firmware service
that can provide our variable as an image before any SSD code runs. The resolver
test narrows that search to a real provider or vendor-specific bootstrap, rather
than merely another device path or GUID. No such service is established yet.
Installing our own provider would itself require an initial executable source.

No physical reboot, boot-entry change, flash write, protection change or native
PXE experiment was performed in this investigation. The working SSD paths remain
the recovery boundary. Root SSH cannot operate a Dell firmware error screen.

## Follow-up: what the existing load options actually select (2026-10-03)

The saved boot-variable backup was decoded as `EFI_LOAD_OPTION` records. Its
`Driver0000` (Enter Setup) has a firmware-volume node for
`a881d567-6cb0-4eee-8435-2e72d33e45b5` followed by a firmware-file node for
`d89a7d8b-d016-4d26-93e3-eab6b4d3b0a2`, with no `OptionalData`. The saved
firmware dump identifies that file as Dell's Enter Setup application, and the
earlier physical FV inventory exposes the matching firmware volume at index 4.
This proves that a boot variable can *name* an executable already in a Dell
firmware volume. The executable bytes are in the firmware file, not in the
`Driver0000` variable. The UEFI specification requires a `Driver####` image to
have a driver subsystem to be entered for initialization; this Dell application
entry alone does not prove execution during ordinary `DriverOrder` processing.

`Driver0001` instead has a GPT hard-drive node followed by the file path
`\EFI\companion\companionextx64.efi`, also with no `OptionalData`. The currently
verified Companion image therefore still needs the SSD for its first load.
The standard `Boot0005` management option likewise names the SSD GRUB file.
The [UEFI boot-manager specification](https://uefi.org/specs/UEFI/2.11/03_Boot_Manager.html)
defines `FilePathList[0]` as the image location and `OptionalData` as data passed
to the loaded image; it does not define either variable as an embedded image.

A read-only name inventory over pinned root SSH on healthy boot
`fcf8df45-34cb-4998-a56c-4f9a3f7bccc5` found `Driver0000`, `Driver0001`,
`DriverOrder` and `BootOrder`, but no `SysPrep*`, `OsRecovery*`,
`PlatformRecovery*` or `BootNext` variable. This is a current variable-name
observation, not proof that Dell implements no vendor-specific recovery route.
The same live read-only inventory found seven Dell-private `BootFFF*` variables
under vendor GUID `5990c250-676b-4ff7-8a0d-529319d0b254`. Their labels are
BIOSConnect, Fix my Dell (twice), Diagnostic Boot, Temporary Boot Menu,
Graphic Setup and Text Setup. Their decoded load-option paths all name the same
firmware volume `9375b02b-4c60-5d56-4c1c-55a699717737` and firmware file
`6b287864-759c-42c4-b435-a74ab694cd3b`, with no `OptionalData`.
The volume is exposed at index 3 in the physical FV inventory. These are
evidence of Dell-private menu/recovery selectors pointing into firmware;
their presence does not establish automatic execution, owner-image loading or
an editable NVRAM program slot. No selector was invoked or changed.
The file GUID matches the previously extracted `SpecialBootStub` PE image
(SHA-256 `adae9e0e2f39a9086d590e9641245ffa5cebc6a69bc671771b8f97ec780ff650`),
whose PE subsystem is EFI application (10). Its saved x64 instructions at RVA
`0x1064` call Runtime Services `GetVariable` for `BootCurrent` under the standard
UEFI global GUID, then compare the returned `UINT16` against `FFF6`, `FFF7`,
`FFF8`, `FFFB`, `FFFC`, `FFFD` and `FFFE`. They route matching values to distinct
internal handlers or return paths. This is a concrete Dell example of a
firmware-resident executable using a UEFI boot variable before an OS
loads. The `BootFFF*` selector records are nonvolatile; `BootCurrent` is a
transient variable populated for a selected boot. The code does not reference
the Companion owner variable by name or GUID, and no physical menu action was
run. The static branch analysis does not prove
that every selector is reachable or that any handler can load arbitrary images.
Within this extracted stub, the `BootCurrent` call is the only direct runtime
variable read visible in the saved disassembly; the selected handlers call
other firmware protocols, whose complete behavior is outside this check.
For the `FFF6`/`FFF7`/`FFF8` cases, the stub locates protocol
`7080e10a-6067-4af5-874c-c05b061d3a11` and calls its second method with
arguments 2/0/1 respectively. The saved `DellAutoOsRecoveryDxe` image contains
the same GUID at an `InstallProtocolInterface` call. This links those menu
choices to a built-in recovery provider at the code-reference level; it is not
evidence that the provider accepts a user image or that its launch succeeds on
the physical machine. The extracted recovery provider contains strings for
`BootNext`, `OsIndications` and `\EFI\Dell\SOS\bootx64.efi`, but their presence
does not establish which branch uses that SSD path. The stub itself shows no
direct `LoadImage` call. One saved recovery-provider path constructs a file-path
node from the fixed `\EFI\Dell\SOS\bootx64.efi` string at RVA `0xf87`, then
calls Boot Services `LoadImage` at RVA `0x1034` with a device-path pointer and a NULL
`SourceBuffer`, then calls `StartImage` at RVA `0x1066` on success. That path
asks firmware to resolve an image from a provider; it does not hand NVRAM
payload bytes to `LoadImage`. The path's selected device handle and physical
reachability remain unverified.
An exact-byte scan of 27 locally extracted firmware module binaries found no
Companion owner GUID or UTF-16 variable name. Generic variable enumeration or
unextracted modules could still consume the owner variable; the negative scan
does not establish their absence.

The remaining direct-NVRAM question is whether any *pre-SSD* firmware component
already retrieves an owner-supplied variable and loads its bytes as an image, or
publishes those bytes through a matching `FV2`/`LoadFile` provider. The current
evidence identifies no such component. A firmware-volume device path to the
variable-store GUID is insufficient because the owner image is a variable record,
not a PI firmware file, and the physical FV inventory did not expose the variable
store as a file-system volume. Further analysis should trace variable-service
calls in the saved firmware and retain separate evidence for any provider's
publication, image authentication and automatic pre-SSD invocation.

## Full extracted-ROM scan and Dell menu builder (2026-10-03)

`tools/scan-dell-nv-consumers.py` used pinned root SSH to read the existing
UEFIExtract dump of the hash-verified 16 MiB Dell BIOS image. It scanned all
659 `PE32 image section/body.bin` files (563 unique SHA-256 values), rather
than only the 27 previously copied modules. None contains the exact little-
endian Companion owner GUID or the exact UTF-16 owner variable name. This
is strong negative evidence for a hard-coded reference in those extracted PE
bodies, but cannot exclude computed identifiers, generic variable enumeration,
non-PE sections or code not represented in the dump. The scan reads the Dell
filesystem and writes no target file or firmware variable.

The same scan found the Dell-private menu namespace GUID in three PE images:
`FastBootHandlerDxe`, `BBSManagerDxe` and `BBSManagerSmm`. The SpecialBootStub
firmware-file GUID occurs in `BBSManagerDxe` and in the stub itself. A local
copy of `BBSManagerDxe` was verified as SHA-256
`c4ad0c1889767773c26c196d402c795999f1af4d3f4a8f18ca2c989250e0e570`
before disassembly. Its static table at RVA `0x9a00` has eight option IDs:
`FFFE` Text Setup, `FFFD` Graphic Setup, `FFFC` Temporary Boot Menu, `FFFB`
Diagnostic Boot, `FFFA` Internal Shell, `FFF8`/`FFF7` Fix my Dell, and `FFF6`
BiosConnect. `FFFA` is absent from the current live variable inventory; the
reason has not been established.

The `FFFA` builder selects a different firmware-file GUID,
`c57ad6b7-0515-40a8-9d21-551652854e37`, rather than the
`SpecialBootStub` GUID. No extracted PE section has that file GUID in its
dump path. Before the table loop, code at RVA `0x2344` locates protocol
`f2feff56-a85b-4489-9099-2d54411dfc6d`, calls its method at offset 8
with selector 11, and tests whether the returned value is 1. If the test
fails, the loop calls the variable writer with zero data size for
`FFFA` (and `FFF9` if present), which is a delete operation in `SetVariable`.
The protocol GUID appears in multiple saved Dell modules, so this does not
identify the policy's meaning or establish why the live shell option is
missing. The option builder also probes FV2 `ReadFile` against the selected
firmware-file GUID; a menu option still names firmware content, not bytes in
the boot variable. The [PI FV2 protocol](https://uefi.org/specs/PI/1.10/V3_Code_Definitions.html)
defines `ReadFile` as retrieving a GUID-named file from a firmware volume.

The saved `BBSManagerDxe` code at RVA `0x2134` constructs `EFI_LOAD_OPTION`
records using a firmware-file device-path node for `SpecialBootStub`. At RVA
`0x2a20` it formats `Boot%04x`, selects the Dell-private variable namespace
for IDs above `FFF0`, and calls Runtime Services `SetVariable` with attributes
7. These code references explain why the live `BootFFF*` records contain
descriptions and a firmware file path rather than an embedded PE image. They do
not prove every branch executes on each boot, that the missing shell option can
be enabled, or that a menu path accepts owner code. No menu or shell was launched
and no NVRAM write was made in this research pass.

## Firmware-management capsule route (2026-10-03)

The saved Dell `BdsDxe` PE body is SHA-256
`827dfbe11c71f67c1a3a37a594145e21680866e8deb8b8f5be0df3f6638fb32b`.
It contains the standard FMP capsule GUID
`6dcbd5ed-e82d-4c44-bda1-7194199ad92a`. Its code at RVA `0xde10` parses
the capsule's item offsets; for an embedded driver, RVA `0xdec4` calls a helper
at `0xdad4`. That helper calls Boot Services `LoadImage` at RVA `0xdb50` with
the capsule item as a non-null `SourceBuffer` and its item length as
`SourceSize`; on success it calls `StartImage` at `0xdb95`. This is a real
firmware pre-SSD memory-image loading route, matching the
[UEFI FMP capsule format and processing contract](https://uefi.org/specs/UEFI/2.11/23_Firmware_Update_and_Reporting.html).
It does **not** read the Companion owner variable. The image bytes are an
embedded item in a delivered capsule. UEFI also requires image-format and
security checks before an embedded driver is started.

There is a decisive distinction between Dell's capsule capability query and
its delivery entry. The saved `CapsuleRuntimeDxe` PE body is SHA-256
`4db34759e75c0502ecb4057d4988d02ba5e5d713001ed838c86f41786f11e3aa`.
An extended offline run of its original `QueryCapsuleCapabilities` code at
RVA `0x1628` returned `EFI_SUCCESS`, 100 MiB maximum and warm reset for a
minimal FMP header with `PERSIST_ACROSS_RESET`. This query uses fixture
constructor state and no driver body, so it only reports transport metadata.
The entry installed as Runtime Services `UpdateCapsule` at RVA `0x1388`
compares the incoming capsule GUID with the FMP GUID at RVA `0x13f6` and takes
an immediate `EFI_INVALID_PARAMETER` return on equality. A separate bounded
Unicorn 2.1.4 execution of that original branch reproduced the return,
without invoking any external firmware service or physical capsule update.
The function rewrote the fixture header's flag word from `0x10000` to
`0x70000` before rejecting it. This establishes rejection by this saved
runtime entry for an FMP capsule; it does not assess mass-storage delivery,
other firmware interfaces, or every physical startup state.

The same `UpdateCapsule` code's non-FMP path writes an eight-byte value to
`CapsuleUpdateData` under EFI capsule-vendor GUID
`711c703f-c285-4b10-a3b0-36ecbd3c8be2`. The code passes an eight-byte
data size to `SetVariable`; it is a pointer/descriptor handoff, not storage of
the capsule image in an EFI variable. UEFI's
[runtime capsule contract](https://uefi.org/specs/UEFI/2.11/08_Services_Runtime_Services.html)
describes persistent capsules as a scatter/gather memory delivery across a
reset, and capsule-on-disk delivery separately uses mass storage. Neither
route turns the Companion owner variable into a firmware file or directly
invokes it.

The live `SecureBoot` and `SetupMode` variable data bytes were both zero on
healthy boot `fcf8df45-34cb-4998-a56c-4f9a3f7bccc5`. This says UEFI Secure
Boot was disabled during that read-only check. It does not establish that
Dell's capsule or firmware update authorization is disabled.
`CapsuleUpdateData`, `CapsuleLongModeBuffer`, `OsIndications` and `BootNext`
were absent from the same live variable-name inventory. Earlier
[firmware-update research](firmware-update-research.md) independently found
signature-required Dell update policy and denied direct out-of-SMM SPI
programming. No capsule was submitted, flash changed, or physical reboot
performed in this investigation.

**Engineering answer:** the firmware can execute its own FV images selected
by NVRAM boot records, and it contains a separate BDS loader for drivers in
delivered FMP capsules. The current Companion NVRAM variable is neither of
those executable sources. No built-in path has been found that automatically
reads and starts its bytes before the SSD bootstrap. Adding a durable
variable-to-image loader would require changing trusted firmware code or
establishing another accepted, persistent pre-SSD provider; neither has a
verified installation and recovery path on this Dell.

## Advertised boot-manager hooks (2026-10-03)

A further read-only check of the live UEFI global variables found
`BootOptionSupport = 0x00000313` and `OsIndicationsSupported =
0x0000000000000041` on healthy boot
`fcf8df45-34cb-4998-a56c-4f9a3f7bccc5`. The first value includes
`EFI_BOOT_OPTION_SUPPORT_SYSPREP` (`0x10`), in addition to hotkey and
application-option support. The saved Dell `BdsDxe` PE image contains the
UTF-16 names `SysPrep` and `SysPrepOrder` at RVAs `0x21808` and `0x23138`.
No `SysPrep####` or `SysPrepOrder` variable is currently installed. The
capability bit is an explicit firmware advertisement, and the strings show
that the image includes the vocabulary; they do not prove that a particular
new option will execute on this physical machine.

Per the [UEFI boot-manager specification](https://uefi.org/specs/UEFI/2.11/03_Boot_Manager.html),
an active `SysPrep####` option in `SysPrepOrder` launches an EFI *application*
whose `FilePathList[0]` resolves to an image, before `Boot####` processing.
It must return to the boot manager and cannot call `ExitBootServices()`.
This is a plausible way to run an owner application before the OS while
retaining the existing `BootOrder`. It still needs an EFI image supplied by
a filesystem or `LoadFile` provider; the NVRAM option contains a device path,
not executable bytes. It also is **not proven to precede the Companion SSD
bootstrap**: the installed bootstrap is `Driver0001` in `DriverOrder`, and
UEFI processes driver options before normal boot options. A removable USB
`SysPrep` image might survive loss of the SSD, but no such option has been
installed or physically tested, and firmware response when its device is
absent remains to be checked before relying on it.

The `OsIndicationsSupported` value advertises boot to firmware UI (`0x1`)
and start platform recovery (`0x40`). It does not advertise file-capsule
delivery (`0x4`) or FMP capsule support (`0x8`); the earlier positive
`QueryCapsuleCapabilities` fixture therefore cannot be treated as an
advertised persistent FMP delivery path. Setting platform recovery would
bypass normal `BootOrder` under the UEFI contract, while the OS cannot
populate `PlatformRecovery####` options. No such options were present in the
live inventory. UEFI 2.11 also says the OS and platform recovery support
bits should appear together; this Dell advertises only platform recovery.
That discrepancy and the missing live options make its precise recovery
behavior a vendor-specific question, not a safe owner-code launch route.
The saved `BdsDxe` writes `0x41` as `OsIndicationsSupported`, which agrees
with the live value. No recovery flag was set and no reboot was performed.

### Dell control flow and isolated execution

The saved, hash-verified Dell `BdsDxe` code supplies stronger evidence than
the `BootOptionSupport` flag alone. In its normal entry path it calls the
load-option reader with type 0 (`Driver`) at RVA `0x164f`, then processes that
list at `0x1672`. Later it calls the same reader with type 1 (`SysPrep`) at
`0x1937`, then processes that list at `0x195a`. The reader's type-indexed
tables point type 1 to the actual UTF-16 `SysPrepOrder` and `SysPrep` names.
The `OsIndications` platform-recovery branch at `0x1929` skips this SysPrep
block. `tools/test-dell-sysprep-branch.py` executes the saved decision branch
in Unicorn with bounded helper stubs: normal boot loads/processes/frees a
type-1 option; platform recovery bypasses it. The test also checks the earlier
driver call sites and table pointers against the pinned PE hash. This tests
the branch and ordering, not a complete physical boot.

The Dell's `ProcessLoadOptions` routine at RVA `0xe84` continues its loop
after a per-option processing error; a failed image lookup is therefore not
itself a deliberate halt in this saved path. An application that never returns,
device-resolution bugs, or later vendor policy could still prevent Linux from
starting. This is why a physical SysPrep write remains a management risk.

The existing Hyper-V `CompanionDev` firmware reports
`BootOptionSupport = 0x303`, without the `SysPrep` bit, so it cannot serve as
the behavioral rehearsal. A separate QEMU 8.2.2 / OVMF 2024.02 instance on
PrimeLinux was used instead. The small C EFI application built by
`tools/build-sysprep-probe.ps1` writes a volatile runtime marker and exits;
an isolated setup EFI application registers `SysPrep0000` with a short-form
file path to that probe, adds `SysPrepOrder`, and resets OVMF. The first
debug-console record was `SYSPREP_REGISTERED`. On the next boot OVMF recorded
`SYSPREP_EXECUTED` followed by `BOOT_AFTER_SYSPREP_MARKER_OK`, proving that
the application ran before the ordinary boot app. With the SysPrep file
absent on a subsequent isolated boot, OVMF recorded
`BOOT_WITHOUT_SYSPREP_MARKER`, proving continuation to its ordinary boot app
in that fixture. The EFI binaries have SHA-256
`31b0873e315371d4b413e24e33c067279ea5bfccd8dbb8a01faa3b58145f1f8a`
and `3c0c7faa1604ab628c46995ddde5fa527bc928cb35cfd6a71556c230c1cc569c`.

This establishes a **real pre-OS application hook** in a comparable UEFI
implementation and identifies the corresponding Dell machine-code path. On
the Dell, `Driver0001` is processed first and still obtains the Companion
extension from the SSD. SysPrep is therefore not the missing direct NVRAM
execution mechanism or a route to run before that SSD bootstrap. No Dell
`SysPrep` variable or EFI file was installed, and no physical reboot was
performed in this follow-up.

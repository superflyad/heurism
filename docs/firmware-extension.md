# Companion extension alongside Dell UEFI

The current goal retains Dell and Intel firmware. Companion adds its own
pre-OS service rather than replacing the board's hardware initialization.
The initial bootstrap is stored on the internal SSD's EFI partition;
Driver0001 and DriverOrder are persisted in motherboard firmware variables.
The [automatic NVRAM startup driver](nv-startup.md) now retrieves and executes
the board-stored owner payload during normal startup, with an embedded SSD
fallback. It is verified on a physical reboot, but does not operate with the
SSD removed. The installation and hashes below describe the original SSD
driver, retained as the rollback copy; the linked record describes the update.

A later [board-stored payload experiment](nv-extension-research.md) placed an
exact copy of the small owner image in nonvolatile variable storage and tested
explicit loading from that storage. The subsequent startup update automates
that loading through Driver0001; SSD-independent bootstrap remains unresolved.

## Implemented and installed

`boot/extension.c` is a freestanding x64 UEFI boot-service driver (PE subsystem
11), not an EFI application or an OS service. It publishes the owner protocol
`377fa1a2-aacc-4689-b15b-8ba1e8995818`. Its bounded GetInfo method returns
the service revision, firmware-interface revisions and capability bit 1 for
read-only information. It does not write variables, disks, flash, CPU registers
or Dell protocol fields; it does not establish a network connection.

The service has boot-services lifetime. It is available to other UEFI programs
before ExitBootServices; it is not a Linux runtime service or an SMM extension.
The reported EFI FirmwareRevision is an interface field, not the DMI BIOS
version. Dell's installed BIOS remains 1.35.0.

The installed configuration is:

| Item | Value |
| --- | --- |
| Existing Dell driver | Driver0000, Enter Setup; preserved |
| Added driver | Driver0001, Companion Extension 01 |
| DriverOrder | 0000,0001 |
| Driver binary | `\EFI\companion\companionextx64.efi` on the ESP |
| Normal BootOrder | 0005,0000; restored after tests |
| Temporary observer | Boot0001; removed after verification |

Driver SHA-256:
`b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a`.
Observer SHA-256:
`02ffe02f774ab9aebbb9b6d1b695a3f01b22d48dba7e2da7101aaf0515657698`.

## Physical evidence

`boot/extension_probe.c` only locates the owner protocol and calls GetInfo.
It contains no call to load or start the extension. The extension is a
separate driver image selected through DriverOrder. Therefore the successful
observation establishes that firmware loaded the service before the observer
boot application ran, independently of Linux.

Two observed physical boots completed:

1. `5ffa086f-04d8-4ca5-9743-e38b65431236`.
2. `548096ff-7117-4516-a172-d4c231d52982`.

Both reports contain EFI_SUCCESS for LocateProtocol and GetInfo, a 32-byte
result with magic `0x314458454d504f43`, revision 1 and capability 1, and
PROBE_COMPLETE. Root SSH and the watcher returned and passed the boot-health
check on each boot. Before the second boot, the first ESP report was moved
to a separate preserved file so the observer created a fresh report.
Both reports have SHA-256
`08345ce476140274a550da735868f4f5761b398336b03cd4195dcda485c291f8`;
identical contents are expected because the interface values are unchanged.

Evidence is saved in ignored `artifacts/firmware/extension-deployment.json`,
`extension-observation-1.txt` and `extension-observation-2.txt`. Normal boot
order is restored; the persistent owner driver remains registered.

## Build, verification and removal

Run `tools/build-extension.ps1` with explicit Clang and lld-link paths.
It builds the driver, its host tests and the separate observer. Tests check
driver PE type, relocations, lack of OS imports, protocol ABI, insufficient
buffers, invalid inputs and installation-error propagation. Observer tests
check its read-only protocol query, ESP report and existing recovery handoff.

`tools/run-extension.py deploy` stages matching binaries, preserves the
existing Dell driver entry, activates the new DriverOrder entry and schedules
a one-time observer boot. `verify`, `retest` and `finalize` require the expected
target and healthy authenticated root access. Existing metadata prevents a
duplicate installation. This particular RST/VMD target rejected efibootmgr's
full-device-path construction; the accepted driver registration uses the
GPT HD()+File() path already used for our successful boot entries.

To remove the installed extension, use `tools/run-extension.py uninstall`.
It validates the saved owner entry, restores DriverOrder to 0000 and removes
only the owner registration. Executable files may remain on the ESP, but
firmware will no longer load them through that registration. New unrelated
DriverOrder changes cause the tool to stop for review rather than overwrite
them.

The extension-loading route is proven. Independent storage in motherboard
flash, preboot remote networking, arbitrary BIOS-setting operations from the
driver, and recovery before Dell can load the driver are not established.

Source: [UEFI driver-loading order and required image types](https://uefi.org/specs/UEFI/2.10/03_Boot_Manager.html).

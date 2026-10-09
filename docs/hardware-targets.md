# Hardware targets

## Target 0: Dell Inspiron 7506 2-in-1

Status: inventoried over authenticated root SSH from the physical Dell on
2026-09-26. There are separate Black and Silver manuals. Do not assume
the display, memory configuration or graphics devices from one match the other.

| Fact | Current evidence |
| --- | --- |
| Model family / board | DMI: Inspiron 7506 2n1 / Dell 0VK62X; case variant not independently verified |
| CPU / RAM | Intel Core i7-1165G7; MemTotal 16,102,776 KiB (approximately 16 GB installed) |
| Storage | Intel H10: 512,110,190,592-byte NVMe SSD plus 29,260,513,280-byte Optane device; both show Intel RAID metadata |
| SSD contents | Original BitLocker Windows and recovery partitions replaced with 512 MiB EFI plus ext4 management root after explicit owner authorization |
| Firmware | Dell BIOS 1.35.0, dated 2025-02-13; UEFI boot confirmed; Boot Guard unknown |
| GPU / Wi-Fi | Intel Tiger Lake i915 loaded; Intel AX201 wireless interface detected; Wi-Fi association untested |
| Ethernet / SSH | Realtek r8152 USB Ethernet; trusted key-authenticated root SSH at 10.8.22.238 during this session |
| Audio | SOF driver reports missing firmware/topology; audio not yet functional/verified |
| Provisioning physical boot | USB Linux startup and local root shell confirmed; duplicate tty0/tty1 gettys corrected in rebuilt media |
| Companion physical boot | Confirmed by owner photo on 2026-09-26; framebuffer screen visible; Escape return pending |

Run this **on the Dell**, from Windows, to collect a starting inventory:

```powershell
.\tools\inventory.ps1
```

Output is `artifacts/hardware-inventory.json`, ignored by Git. It omits service
tags, serial numbers, MAC addresses and user names. Review device IDs before
sharing the file. Running it on a different development computer does not
inventory target 0. Record Secure Boot state, boot mode and recovery arrangements
separately; the script does not change any system settings.

Follow up with a live Linux USB as a reference experiment: record PCI and USB IDs,
display modes, touch/trackpad, Wi-Fi, Bluetooth, audio, battery, suspend and resume.
Store facts and failed observations, not just a list of features in the brochure.

The full physical audit is saved locally at
`artifacts/hardware/dell-7506-20260926.txt` (ignored by Git). It includes device
identifiers and boot logs; review before sharing. Root access has been verified
after physical boot from the installed SSD without USB; see [deployment](dell-deployment.md).
The native Companion kernel runs in VM with ACPI/PCI inventory and keyboard
input; physical native management remains pending.

Sources: [Dell Silver setup/specifications](https://www.dell.com/support/manuals/en-us/inspiron-15-7506-2-in-1-laptop/inspiron-7506-2n1-silver-setup-and-specifications/set-up-your-inspiron-7506-2-in-1-silver),
[Dell Black setup/specifications](https://dl.dell.com/topicspdf/inspiron-15-7506-2-in-1-laptop_users-guide2_en-us.pdf).

## Target 1: ARM64 development board

Not selected. Choose only after identifying what target 0 teaches us and which
device features the portable design actually needs. `platform/arm64/` is a
reserved boundary, not a claim of ARM64 support.

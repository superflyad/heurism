# Boot milestone 0 on USB

1. Run `.\tools\build.ps1 -Test`. Keep the printed SHA256 hash.
2. Use a spare USB drive with an existing FAT32 partition. Copy the **contents**
   of `build/esp/` onto its root. The final path must be
   `X:\EFI\BOOT\BOOTX64.EFI`. This repository provides no disk-formatting script.
3. The image is unsigned. Firmware must permit unsigned UEFI applications, or
   the image must be signed with a key the firmware trusts. Before changing
   Secure Boot settings, have the machine's BitLocker recovery key available
   if drive encryption is enabled. Do not clear TPM keys or change storage mode.
4. On the Dell, open its one-time boot menu (normally F12 at the Dell logo) and
   select the USB's UEFI entry. Keep a record of any settings you changed.
5. Observe the dark Companion screen. The text should say framebuffer and
   firmware services are active. Press Escape; the application returns to the
   firmware boot manager. Remove the USB to use the normal boot path again.
6. Record model/variant, firmware version, settings, image hash, photo and result
   in a copy of `docs/experiment-template.md`. Hardware boot remains unproven
   until these observations exist.

The app neither installs to internal storage nor selects Windows' boot entry.
Firmware decides what to launch after it returns. If no usable framebuffer is
found, it stays in text mode and displays status information until Escape. Fatal
watchdog/input errors stay visible for 15 seconds before returning. The renderer
supports current-mode RGB/BGR 32-bit layouts; BLT-only or bitmask layouts are
reported as unsupported. Very small modes can clip text.

## If Dell lists the USB but boot fails

The 0.2 build first prints `COMPANION BOOT 0.2 - EFI ENTRY REACHED` for about two
seconds before drawing the UI. Seeing it proves that firmware launched our app.
A text-mode fallback is also a successful app launch; photograph any status
codes. An immediate failure with no banner can happen before our code executes,
including when firmware rejects an unsigned image. A Security Violation or
signature message is a Secure Boot issue, not a framebuffer failure.

The prepared USB also contains `\Companion.efi` and
`\EFI\Companion\Companion.efi`. These are identical copies for a firmware
“boot from file” or “add boot option” workflow. Naming the volume `COMPANION`
helps identify it but does not add a trusted signature or guarantee a specific
F12 menu label. Dell documents manually selecting `\EFI\BOOT\BOOTX64.EFI`
when automatic boot discovery fails; exact firmware screens vary.

The owner's Dell photo confirms a signature rejection. For the development
experiment, have the Windows BitLocker recovery key available if encryption is
enabled, restart with F2, open Boot Configuration → Secure Boot and switch
Enable Secure Boot off. Apply/save and retry the USB from F12. Keep UEFI mode
and existing TPM keys/storage settings. Restore Secure Boot when returning to
the normal secured configuration. A future signed build will also need its
signing key trusted by firmware; self-signing alone is insufficient.

Menu reference: [Dell Boot Configuration options](https://www.dell.com/support/manuals/en-us/inspiron-15-7506-2-in-1-laptop/inspiron-7506-2n1-black-service-manual/system-setup-options?guid=guid-cb19996e-6cf5-47b9-be58-1a039da03b99&lang=en-us).

The firmware provides the initial display setup and keyboard input. Seeing our
screen proves our code executed in UEFI and wrote to the framebuffer. It does
not prove native GPU/input drivers or successful ExitBootServices.

References: [UEFI removable-media boot rules](https://uefi.org/specs/UEFI/2.11/03_Boot_Manager.html),
[Dell F12 boot menu](https://www.dell.com/support/manuals/en-us/inspiron-15-7506-2-in-1-laptop/inspiron-7506-2n1-black-service-manual/boot-sequence?guid=guid-c50bed7f-226b-4768-ab61-9725153df132&lang=en-us),
[Microsoft BitLocker recovery triggers](https://learn.microsoft.com/en-us/windows/security/operating-system-security/data-protection/bitlocker/faq).

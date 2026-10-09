# Companion startup presentation

## Current state: hidden menu

Iteration 2 hides the GRUB menu during normal startup, with a one-second
key window. Press Escape during that window to reveal the management/recovery
menu. The stable/rescue journal and fallback behavior remain unchanged. This
is still the Linux management path; the separate native kernel is tested in VM
and has not replaced the Dell default. The Dell logo remains firmware-owned.

The exact new configuration passed both isolated recovery checks: trusted root
SSH in RAM rescue, then automatic rescue after deliberately missing stable
kernel files. A second run injected Escape, observed the GRUB menu and booted
rescue with Enter; fallback also passed afterward. See
`build/recovery/test/proof.txt` and `menu-key-proof.txt`. The key path is verified
in VM, not yet observed on the physical Dell.

Deployment used the same preflight, hash/syntax checks and backups described
below. Rollback files are at
`/var/lib/companion/presentation-20260927T161814Z`. To undo only the hidden menu,
restore its `grub.cfg.before` to `/boot/efi/companion/recovery/grub.cfg`.

Normal physical reboot changed boot ID from
`2683f150-bc71-41bf-b755-3290bdddde29` to
`7c465eeb-59ca-46c5-8f6a-31954e668b8c`. Trusted root SSH, watcher and welcome
returned, the healthy marker matched this boot and the recovery journal cleared
to `companion_pending=0`. The installed config has hidden timeout 1 and all six
protected EFI/kernel/initramfs hashes passed unchanged. BootCurrent is `0005`;
BootOrder was restored to `0005,0000` after firmware appended NIC entries;
DriverOrder remains `0000,0001`, BootNext absent. No NIC boot was selected.
Evidence is `artifacts/startup/hidden-menu-reboot.txt` and
`presentation-deployment.json`. SSH verifies healthy boot/configuration, not
every frame of the physical display transition.

## Iteration 1 record

The installed startup path now uses Companion menu colors and an explicit
`COMPANION | Starting management` label. Routine kernel console messages are
suppressed with `quiet loglevel=4`; error-level and more severe messages remain
eligible for console output, and kernel logs remain available through dmesg.
See [Linux parameter definitions](https://docs.kernel.org/admin-guide/kernel-parameters.html).
OpenRC service output can still appear during startup.

Once management startup and its health check complete, a presentation service
generates a concise Companion login screen. It shows the IPv4 address and
`Management ready` only when this boot's healthy marker matches and sshd and
the network watcher report started. Otherwise it says `Starting management`.
This is a startup snapshot; `companion-status` remains the current diagnostic
command. The local getty clears earlier output before displaying the new screen.
Other virtual terminals, local login and authorized-key root SSH remain usable.

This is a text presentation iteration, not a continuous graphical splash.
The owner-observed Dell logo precedes our current handoff point and is not
replaced by these changes. Native kernel work is specified in
[the kernel plan](kernel-plan.md).

## Installation and recovery

`tools/update-startup-presentation.py` checks the current root/EFI devices,
matching healthy boot ID, running management services, known EFI loader hashes,
default orders and a clear recovery journal. It transfers the three source
files, checks their hashes and shell/GRUB syntax, backs up the current state and
installs the presentation files. It neither reboots nor changes boot entries.

The installed external configuration is
`/boot/efi/companion/recovery/grub.cfg`. The active standalone EFI loaders use
this configuration; `/boot/grub/grub.cfg` is not the active recovery-aware menu.
Stable/rescue selection, `companion_pending`, the three-second menu timeout and
the rescue kernel command line retain their previous behavior. EFI loader,
extension, stable kernel and stable initramfs hashes are unchanged.

The new OpenRC service `/etc/init.d/companion-welcome` runs after Companion
management, watcher and boot-health services. It prepares `/etc/issue` and
`/etc/motd` through `/etc/companion/bin/companion-welcome`. It does not configure
networking, restart management services or write the recovery journal. Future
provisioning builds include the service and the installer carries it forward.

Original pre-presentation backups are on the Dell at
`/var/lib/companion/presentation-20260927T153602Z`. The final deployment, after
correcting the console log threshold to retain error messages, is backed up at
`/var/lib/companion/presentation-20260927T153627Z`. The deployment record is
`artifacts/startup/presentation-deployment.json`.

To restore the original presentation while connected as root, copy the first
backup's `grub.cfg.before` to `/boot/efi/companion/recovery/grub.cfg`, restore
`issue.before` and `motd.before` to `/etc/issue` and `/etc/motd`, and remove
`companion-welcome` from the default runlevel with `rc-update del
companion-welcome default`. Keeping the unused welcome executable is harmless.
No EFI loader or firmware variable rollback is needed. This rollback requires
working management access; it is not an independent recovery channel.

## Verification

The final configuration passed `grub-script-check`; both new shell files passed
`sh -n`. The installed protected files passed SHA256 checks before reboot.

The exact installed GRUB configuration was copied into the existing isolated
rescue VM payload and exercised through the existing standalone loader. Both
checks passed: trusted root SSH in offline RAM rescue, and automatic rescue with
trusted SSH after a stable boot attempt with intentionally missing management
kernel files. Evidence is in `build/recovery/test/proof.txt` and `serial.log`.
These tests establish VM fallback, not independent recovery from a Dell firmware
halt.

The physical normal reboot changed the boot ID from
`73ece2ba-139c-4c0c-8369-0c795cd39c40` to
`c4e357e0-3ffd-4a83-9284-4e18ea0f5053`. Trusted root SSH returned, the watcher
and welcome service started, and the boot-health marker committed this new boot
with `companion_pending=0`. The running kernel command line includes the new
console parameters. A read of the physical virtual-console text buffer shows
the Companion title, `Management ready`, the Ethernet address and login prompt.
This confirms console content; no photograph or continuous visual timing
measurement was collected.

All six protected file hashes remained unchanged. BootCurrent is `0005`;
DriverOrder remains `0000,0001`, and BootNext is absent. Dell again appended
automatic NIC entries during reboot; their observed order was `0005,0000,0001,0002`.
It was restored to `0005,0000` without selecting or deleting any NIC entry.
Physical evidence is `artifacts/startup/presentation-reboot.txt`.

## Dell headless banner policy

The owner requested removal of Dell's `Headless Operation Mode Active` POST
banner. The live BIOS exposes no independent banner-suppression setting.
[Dell's documented method](https://www.dell.com/support/kbdoc/en-us/000139731/what-the-headless-operation-mode-active-post-message-means-and-how-to-stop-it-appearing-during-start-up)
is to select `Prompt on Warnings and Errors`; both continue policies display
the banner. The setting was changed from `WarningsAndErr=ContWrn` to
`WarningsAndErr=PromptWrnErr` through the supported firmware-attributes interface.
`PowerWarn=Disabled` and `DockWarningsEnMsg=Disabled` were verified and retained,
so those known warnings remain suppressed.

This changes future warning handling: an unsuppressed firmware warning can
require local input before management starts. SSD recovery cannot bypass that
pause, and Ethernet cannot dismiss it. The absence of an independent reset or
BIOS console remains a limitation. Reverting with `companion-bios set
WarningsAndErr ContWrn` restores continuation on warnings but also restores
Dell's banner. The setting change is recorded in
`artifacts/startup/headless-banner-setting.txt`; post-reboot verification is
recorded separately. No firmware code or EFI loader is patched for this change.

Normal reboot returned healthy root SSH on boot
`7dc09e62-a246-408d-a0af-7aba72e2001c`. The setting persisted, both known warning
toggles remained disabled, the welcome/watcher services started and the recovery
journal committed `companion_pending=0`. All six protected file hashes passed.
BootCurrent `0005`, BootOrder restored `0005,0000`, DriverOrder `0000,0001`,
BootNext absent. Evidence: `artifacts/startup/headless-banner-reboot.txt`.
The owner confirmed the headless banner was not visible on this reboot.

On 2026-10-09 a later checked reboot stalled at the Dell logo and required a
physical power cycle. The precise preboot cause remains unknown. Because Dell
documents that `PromptWrnErr` can stop POST on warnings, the owner-prioritized
remote-access recovery led to restoring the earlier `ContWrn` setting. Readback
passed with `pending_reboot=1`. A later monitored checked restart returned fresh
boot `9668135f-ca95-4be3-a0d9-78a6fc9a78a7` with management and desktop
healthy; the owner saw no warning or unusual behavior. One pass does not
establish the stall's cause or eliminate the preboot recovery gap. See
[the incident record](dell-terminal-20261009.md).

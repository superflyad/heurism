# Current product direction

On 2026-10-09 the owner requested a Heurism look and performance unlike the
stock Alpine/Xfce desktop. A sealed VM visual release
`/opt/heurism/native/releases/heurism-os-20261009T235647Z-205314` now includes
the H mark, vector wallpaper, branded application menu, navy panel, dark dock
and clean workspace. Checked C reboot returned fresh VM boot
`124b1b46-a472-49d7-b7ff-9a4696254888` with painted UI and healthy
SSH/watch/control/release. Before this work, repeated Xvfb sidecar tests had
left user D-Bus/GVFS/ssh-agent helpers alive. Unique runtime-directory cleanup
and real-session token cleanup are now in source. On the fresh VM boot, the
user D-Bus count was two before and after a desktop restart; idle used memory
was 524 MiB at first measurement, 532 MiB after restart. The old long-running
VM had 34 user D-Bus daemons and 615 MiB used; this is not a controlled memory
benchmark. Temporary VM compiler packages were removed. Prechange VM
checkpoint `Heurism-look-baseline-20261009` UUID
`496118fc-a694-4f32-9650-d68b4cb49f69`. The same look and C session cleanup
were then installed on the Dell through the guarded installer as sealed release
`/opt/heurism/native/releases/heurism-os-dell-20261010T002244Z-12355`.
Dell sidecar tests, C release verification, live 1920x1080 screen capture,
SSH/watch/control/desktop status, checked power service and protected SSD/NVRAM
verification passed on boot `9668135f-ca95-4be3-a0d9-78a6fc9a78a7`.
No Dell OS reboot occurred, so Dell restart persistence for this release is
unverified. The remote-power guard remains active. See `docs/heurism-look.md`.

On 2026-10-09 the owner physically found the Dell stalled at its logo after
the checked C restart, with the laptop throttling. Powering it off and on
restored boot `30913da8-5441-41d6-967f-020680316816`, pinned SSH, Xfce,
control/watch and protected hashes. Persistent syslog shows orderly OpenRC
shutdown at 15:57 UTC and no Linux startup until 23:20 UTC after the owner's
power cycle. The precise preboot cause is unknown; do not attribute it to the
C terminal or claim it fixed. The earlier banner-suppression change left Dell
BIOS `WarningsAndErr=PromptWrnErr`, which Dell documents can halt POST on a
warning. This was reverted through the supported BIOS interface to the prior
`ContWrn` on the live boot. With the owner physically at the Dell, one checked
C restart returned normally on fresh boot
`9668135f-ca95-4be3-a0d9-78a6fc9a78a7`; the owner saw no warning or unusual
behavior. Pinned SSH/watch/control, painted Xfce, the active sealed release,
SSD bootstrap, protected hashes and C power check passed. Dell appended its
known USB NIC entries; after checking them, BootOrder was restored to
`0005,0000`. BootCurrent is `0005`, DriverOrder `0000,0001`, BootNext absent.
One normal boot does not establish the earlier stall's cause or unattended
reliability. Do not perform an unattended Dell reboot or poweroff. The host
remote-power guard remains active; a restart requires a person who can inspect
and power-cycle the Dell until an independently powered reset and observation
path is physically proven. Keep SSD management and recovery intact. See
`docs/dell-terminal-20261009.md` and `docs/independent-control.md`.

The owner clarified that Heurism is not ready for a public OS release and asked
to focus on making the OS excellent. On 2026-10-09 a local C terminal iteration
added 512-line bounded scrollback with Shift+PageUp/PageDown and wheel
navigation. The build staging path now normalizes Windows CRLF source files
before sending Unix scripts to PrimeLinux. Latest isolated image SHA256 is
`1c0cef7e9a02d369d19b306a7ee25ddf1575c9b6678b7504489c50f94809569c`;
boot `5fbcdc7a-b640-4eda-b4db-d8d00ce9626e` ran sealed release
`/opt/heurism/native/releases/heurism-os-image-20261009T152521Z`. Real X11
terminal screenshots on the preceding candidate showed live lines 60–80,
39–60 after Shift+PageUp and 36–57 after one wheel step. Returning to the
prompt wrote a UID-1000 file; 700 lines left the process healthy. A
narrow-to-wide resize exposed black blank-cell repainting, fixed by setting
unit width and default colors on backfilled cells. The final sealed image
passed that same framebuffer resize check, protected hashes, package inventory
and C power. Checked poweroff reached host-observed Off. Original
`CompanionDev` restarted on healthy boot
`0b1b0863-01f4-4406-a6bd-a9a2c37e4cc7`; candidate is Off. The terminal
change has not been activated on the existing VM or Dell. Do not publish an OS
image or characterize this iteration as a public release. See
`docs/heurism-system-base.md` and `docs/native-runtime.md`.

On 2026-10-09 a fresh C-first Heurism VM image was built from the checked
Alpine 3.24.2 base and 45 direct package selections. Its VHDX SHA256 is
`5bce37d38af77435b65e99f999284a9560c70216fa08e38ebfd4bfc9882f9e8c`.
An isolated `HeurismCandidate` Hyper-V VM booted sealed C release
`/opt/heurism/native/releases/heurism-os-image-20261009T145632Z` on boot
`6edd8116-4960-4bc0-9d64-392e6f304c19`. Actual first-boot framebuffer
shows the Heurism wallpaper. SSH/watch/control, UID-1000 Xfce, C power,
protected hashes and exact installed-package inventory passed. The C workspace
fallback painted correctly with Heurism branding; the C shell and graphical
terminal passed a real UID-1000 file/window test. Checked reboot returned
healthy boot `9ba17212-8231-4d66-8863-1a5cfb3c4935` with that file intact.
Checked shutdown reached host-observed Off. The original `CompanionDev` VM was
restarted on healthy boot `9b727da7-1024-40dd-97b8-5fc3186c7a42` and the
candidate left Off. Pretest checkpoint UUID is
`a90e8c9e-73d7-4967-bb17-fb3eb5e66e1b`. This candidate has not been installed
on the Dell. The old Python/Tk desktop is absent from the image; Onboard still
pulls in upstream Python 3. The VHDX contains generated SSH host private and
root authorized keys and must not be published as a public downloadable image.
Package versions are recorded, not locked to a repository snapshot. Provide
per-install key provisioning and a repeatable package source before public
image distribution. See `docs/heurism-system-base.md`.

On 2026-10-09 the owner clarified that Heurism is its own Linux distribution,
built from upstream Linux and Alpine components, not merely an Alpine desktop
theme. The live VM and Dell now report `ID=heurism`, `ID_LIKE=alpine` in
`/etc/os-release`, while the packaged Alpine 3.24.2 `/usr/lib/os-release` and
`apk` provenance remain intact. Both Heurism identity files are protected by
the VM/Dell manifests. The checked VM fresh boot is
`82e62200-48b6-44f5-9562-78f8c55ad056`; the checked Dell fresh boot is
`4dc58594-e232-44f6-baa2-6998c17115cb`. Dell NVRAM path 3, C release,
Xfce, SSH/watch/control, sound and protected checks remained healthy. Default
BootOrder `0005,0000`, DriverOrder `0000,0001`, no BootNext. The VM image
builder consumes Heurism's checked direct-package profile and now seals C
before first boot in the isolated candidate. See
`docs/heurism-system-base.md`.

On 2026-10-09 the dual-name EFI bootstrap was activated on the physical Dell
after 19 host and six isolated OVMF cases passed. The owner confirmed local
power-button access. The new `HeurismExtensionImage01` variable has attributes
7 and matches the 2,048-byte legacy payload exactly; the old
`CompanionExtensionImage01` variable remains intact. A checked reboot under
the old driver returned healthy boot `acb8eaf4-8627-4751-b12c-ef838ec56122`.
The guarded activation backed up the old SSD driver and six-file manifest,
then installed dual-name driver SHA256
`a215f4143742e4263577942e1886b5546375bf08777f11903196e0f99c78ff31`.
Fresh checked boot `1ae2b1d9-a8ce-4467-97be-e952e7198888` reports volatile
marker path 3 with zero read/load/start/service errors: it executed the image
read from the Heurism-named NVRAM variable. Root SSH/watch/control, Xfce
release/UI, speaker sink and protected manifest are healthy. BootCurrent
`0005`, BootOrder restored
to `0005,0000` after verifying Dell-appended NIC entries, DriverOrder
`0000,0001`, BootNext absent. The original SSD driver is backed up at
`/boot/efi/EFI/companion/companionextx64.before-heurism.efi` and under
`/var/lib/companion/firmware/heurism-nv-candidate/`. The EFI bootstrap still
depends on the SSD; the payload protocol ABI and volatile marker retain their
legacy names. Do not delete the legacy variable without a separate recovery
migration. See `docs/heurism-nv-migration.md`.

The owner renamed the OS product to **Heurism** and asked for a deep C runtime
migration. Active VM and Dell releases use `/opt/heurism/native/current`,
`heurism-*` C executables and Xfce launchers. Current releases are VM
`/opt/heurism/native/releases/heurism-os-20261009T015910Z-3115` and Dell
`/opt/heurism/native/releases/heurism-os-dell-20261009T020254Z-7718`.
The hostnames are `heurism-vm` and `heurism-dell`; the active C services are
`heurism-control` and `heurism-desktop`. The old desktop/control service files
remain disabled for rollback. Dell fresh boot
`1ae2b1d9-a8ce-4467-97be-e952e7198888` passed release/UI health, C power,
root SSH/watch/control, six protected hashes and default SSD boot state after
the verified auto-created USB NIC entries were removed from BootOrder.
BootCurrent is `0005`, BootOrder `0005,0000`, DriverOrder `0000,0001`, BootNext
absent. The native install staging directory was found world-writable and is
now mode `0700` root-owned on VM and Dell; both installers reject writable
stage inputs. Temporary build headers were removed. A checked attempt to
rename Boot0005 with `efibootmgr -b 0005 -L` had no effect; raw entry bytes
match the backup. The protected EFI files and labels, `companion-ui` account,
independent `companion-watch`, `/etc/companion` and healthy-boot records,
pinned SSH identities and sealed Python/Tk recovery remain compatibility and
recovery interfaces. Do not change these without a complete recovery gate.
See `docs/heurism-migration.md`.
The VM final release also passed a checked fresh boot
`44ae8e38-07e5-4acc-bacd-4efa95d0a528` with the new services.
The final Dell reboot confirmed the live Ethernet DHCP process advertises
`heurism-dell` while the pinned SSH link remains healthy.
The local Git repository has commits but no remote; the current branch is
`task/heurism-rename`.

The owner asked for a usable revamp with an established desktop. The current
default VM and Dell user session is Alpine Xfce 4.20 over Xorg, with Thunar and
Mousepad. Companion-owned shell, terminal, settings, control and release
verifier remain C. The original C workspace/dock is selectable with `native`
in `/etc/companion/native-session-mode`; the legacy Python/Tk UI remains sealed
recovery. The active VM release is
`/opt/companion/native/releases/c-os-20261004T004048Z-7138` and the active Dell
release is `/opt/companion/native/releases/c-os-dell-20261004T004134Z-8068`.
Xfce's built-in Log Out dialog had disabled Restart and Shut Down because it
could not use Companion's guarded power service. The current release replaces
the top-right Xfce actions button with a Companion C Power launcher and
overrides the disabled Applications > Log Out entry with Applications > System
> Companion Power. Its Restart and Shut down controls require a second click
within ten seconds and call the existing checked C control socket. A live Dell
UI confirmation click on the preceding power release restarted to fresh boot
`2fe46192-7c97-4629-97f9-757032debc23` with SSH/watch/control/Xfce UI,
painted framebuffer, sound sink, saved input settings, release verification and
six protected hashes healthy. `BootCurrent` is 0005, `BootOrder` was restored
to 0005,0000 after Dell-added NIC entries, `DriverOrder` is 0000,0001 and
BootNext is absent. The final menu override release was activated and its menu
route tested live without another physical reboot. Build packages were removed.
See `docs/xfce-bridge.md`.
The shared X11 UID-1000 and unencrypted disk security limits still apply.
The previous VM Xfce checkpoint is `Companion-Xfce-bridge-final-20261003`, UUID
8cf15a9d-b3ca-4569-a098-bf0681ff2b4c. The current VM power release passed
checked UI restart and UI shutdown with host-observed Off state, followed by
host start and fresh boot `d383719e-9de0-4f3b-9886-9e29e522e0f9`.
The current VM checkpoint is `Companion-Xfce-power-menu-ready-20261003`, UUID
a3d74a06-adf2-4523-bb5c-184d857dba03.
On 2026-10-07 the physical Dell was verified with its lid closed, AC online and
Ethernet active. A checked C reboot returned fresh boot
`fe284286-173f-4aa0-8b97-ddca43b80693` with SSH/watch/control/Xfce healthy,
protected hashes unchanged, `BootCurrent` 0005, `BootOrder` restored to
0005,0000, `DriverOrder` 0000,0001 and no BootNext. The r8152 USB Ethernet
adapter reports `Wake-on: g`; its USB device and XHCI controller wake settings
are enabled. Two `s2idle` suspend cycles with 120-second RTC alarms resumed
after local magic packets on the same boot. In the second, port 22 remained
unreachable after 25 seconds without a packet, then returned about eight
seconds after the packet, well before the RTC alarm. Suspend stats: two
successes, zero failures. RTC alarm was cleared. This proves local-LAN wake
from Linux `s2idle` with closed lid, not wake from poweroff, firmware halt,
or an arbitrary network. See `docs/closed-lid-control.md`.
The earlier C workspace details below are historical baseline evidence.

The owner has clarified the implementation rule: keep Alpine Linux as the
kernel and Unix foundation, but write new Companion-owned runtime components in
C. Do not extend Python/Tk as the product architecture. Build a sound C shell and
graphical terminal before further desktop visuals; keep code compact, consistent
and correct. Python in `tools/prime-vm.py` is host-only VM orchestration. The
Python desktop/control service is sealed legacy recovery; the active Dell
desktop/control release is C at
`/opt/companion/native/releases/c-os-dell-20261003T182623Z-27847`.
The VM C release is
`/opt/companion/native/releases/c-os-20261003T182145Z-17012`.
It contains the C shell, X11/Xft/libvterm terminal, workspace and dock,
Files/Editor, root Unix socket control, VM session configuration and release
verifier. The administrator entry directs users to authenticated root SSH;
the control socket rejects `admin-console` to remove the shared-X11 root
terminal path.
POSIX shell scripts are startup glue; Firefox and Onboard remain standard Unix
applications. The legacy Python/Tk runtime is now a sealed fallback, not the
active desktop/control path. A checked C power reboot returned fresh boot
ac14c8d9-a625-49e6-bb2a-21c09069b0bc with healthy SSH/watch/control/UI,
painted framebuffer and the dock at 120,716. The UID-1000 live PTY, resize and
Ctrl+C test passed after reboot; no Python/Tk runtime process was active.
Temporary build packages were removed. One failed candidate activation returned
to the previous C release; Xorg lifetime, first paint and dock placement were
fixed before the final release. Keep C releases sealed and use the VM-only
installer's rollback gate. The current C baseline checkpoint is
Companion-C-runtime-ready, UUID 7e64ad35-6f70-4fac-826f-1b6e1b69c6b2.
The C control service supports the verified hyperv-dev VM profile and the
exact Dell Inspiron 7506 2n1 DMI. The owner chose Ethernet and deferred live
Wi-Fi association. Dell activation and C checked reboot returned fresh boot
`fcf8df45-34cb-4998-a56c-4f9a3f7bccc5` with healthy root SSH/watch/control/UI,
painted 1920x1080 workspace, working C terminal, and all protected hashes
unchanged. `BootCurrent` is 0005, `BootOrder` restored to 0005,0000,
`DriverOrder` 0000,0001, and no BootNext. The first Dell C release had a
Sound-page health table crash; the sealed release fixes it, and the
live Sound page retained the same desktop PID. The current release adds C shell
cursor editing, session history and basic unquoted globbing. A real PTY test
passed after reboot. The Dell installer closes its flock descriptor in
sidecar/OpenRC children. Build packages were removed. Keep
C releases sealed and use the Dell-only installer's rollback gate. See
`userspace/native/README.md`, `docs/native-runtime.md` and `docs/security.md`
for boundaries. The shared X11 UID-1000 session and unencrypted root disk
remain material security limits; do not claim isolation of untrusted apps.
On 2026-10-03, the owner confirmed that touch input and speakers work on the
physical Dell. Specific gestures and audio quality were not described; do not
infer those from this report. Live Wi-Fi association remains deferred.

The owner clarified that Linux stays: Companion is an OS experience and hardware
control platform on Linux, not a replacement Linux kernel. Prioritize Companion
startup, userspace services, local UI and supported hardware/BIOS interfaces.
Keep authenticated management independent of the UI. Retain custom kernel and
native USB/network work as optional isolated research; do not treat their missing
drivers as product blockers or select that kernel on the Dell.

The owner initially moved OS iteration to PrimeServer emulation, then explicitly
authorized installing the C runtime on the Dell. Continue ordinary development
in the VM and use the guarded Dell installer for physical releases. Use
`tools/prime-vm.py` for the separate
Hyper-V Generation 2 `CompanionDev` VM, UUID
43d94d09-f1b1-4980-a66c-4736526639e2, private address 172.28.50.3 through the existing
pinned `primeserver` SSH alias. Its client/host pin is separate under
artifacts/prime-vm. Root SSH/watch/control remain independent of the user desktop;
Hyper-V supplies host reset, console readback and checkpoint restore without
guest SSH. The existing PrimeLinux VM builds images and relays guest SSH;
direct Windows-to-guest forwarding intermittently failed on cold startup. See
docs/prime-vm.md. An absent platform config retains the Dell guards; the explicit
hyperv-dev profile requires real Microsoft Virtual Machine DMI. Never fabricate
Dell BIOS/touchpad/Wi-Fi/speaker support in the VM. VM audio currently uses the
PulseAudio null sink. Tk flushes deferred painting before UI health is saved and
the VM initializes its virtual pointer once in the empty header. Cold startup
otherwise retained a black frame until pointer motion despite healthy polling;
painting flush alone was insufficient after hard reset. This is software cursor
initialization, not physical human input evidence. Candidate image builds must
not overwrite the running VM's SSH host pin.

PrimeServer's cold-start verified release was
/opt/companion/releases/20260928T005954Z-7763, healthy after a host hard reset on
df974b4d-fb78-4c39-a34c-7c421d696743. The actual cold-start framebuffer passes
`capture --require-ui`; normal reboot retained preferences and a UID-1000
document. Desktop application tests, failed-clone rollback and host checkpoint
disk/state restoration passed. The current baseline is Companion-ready-v2,
snapshot UUID 684e5772-8fdd-4963-8beb-bf59c25b8c6d. Restoring a checkpoint loses
later guest changes and restores its boot ID/memory state; a hard reset must
return a fresh boot ID. Use `wait --after-boot UUID` to verify that distinction.
The preceding legacy VM UI release is /opt/companion/releases/20260930T000247Z-9693,
desktop 0.4. It replaced the full-screen launcher with a workspace background,
desktop shortcuts, a persistent centered dock/menu and live window task buttons.
Openbox reserves 84 pixels for the dock; Windows+D minimizes apps but leaves the
dock visible. Files windows and Onboard fit above it. VM capability labels remain
honest: Ethernet, virtual audio without speakers, no Wi-Fi form or Dell BIOS
controls, and no invented battery. Live application tests, Onboard-generated text,
task restoration and workspace tests passed. Normal API reboot returned fresh
boot 0005192e-cbcc-4740-8f0c-292167230274 with the same release, healthy
SSH/watch/control/UI, preferences and a UID-1000 document. `capture --require-ui`
confirmed the painted desktop. No host hard reset has been run for desktop 0.4;
the ready-v2 checkpoint still contains the preceding pre-dock release.

# Physical Dell boot experiments

Desktop 0.3 is installed at /opt/companion/releases/20260927T195311Z-7003 and
verified on normal boot a51e7782-1f35-4e31-814c-0d4b4e7b14ef. Files/Trash/restore,
editor/drafts, Firefox, ordinary/root terminals, Onboard, managed windows,
Wi-Fi scan/configuration, speaker controls, BIOS attribute UI and checked power
actions are implemented. Six control cases, eight wireless/power cases and five
release cases pass. Real X launches and Onboard key-generated text pass; physical
human finger/trackpad motion, audible quality and real-password Wi-Fi association
remain unobserved. Shutdown/suspend were not physically tested.

Versioned UI startup has a 15-second health gate and a stable fallback helper at
/usr/local/sbin/companion-release. A sealed, deliberately broken UI clone restored
the working session without owner input; root management/watch/control remained
healthy. Never corrupt a working release for a test. Home uses home.py to minimize
apps and focus the desktop; do not reintroduce Openbox's ShowDesktop input overlay.
User .local/state parent directories must remain owned by companion-ui, mode 0700.

SOF audio firmware and ALSA UCM are installed. Cold-boot mdev nodes lacked udev
SOUND_INITIALIZED classification: session-config.py uses scoped sound-card
udevadm test --action=change before PulseAudio. RUN rules are not executed; do not
trigger all devices or detach the sole Ethernet driver. The corrected normal
reboot returned the real speaker sink; silent PCM playback ran it successfully.
BootCurrent 0005, BootOrder 0005,0000, DriverOrder 0000,0001, no BootNext, all six
protected hashes unchanged, tty7 and four management/desktop services healthy.
Saved preferences and a UID-1000 document survived reboot. Power operations verify
protected files and default drivers, reject BootNext/unexpected orders, and may
restore only the proven SSD prefix after verifying Dell-appended auto-created
MAC/IP NIC entries. They never select a NIC entry. See docs/desktop.md and
docs/workspace.md. Startup still depends on SSD/Dell firmware; no BIOS-halt control
or independent reset is established.

Shell 0.2 now uses libinput for the Dell keyboard/touchpad/touchscreen, with saved
tap, natural-scroll and pointer-speed settings, keyboard page navigation and a
touch/pointer drawing and scrolling workspace. Five control host tests, actual Tk
interaction tests and UI crash recovery pass. Normal boot
02e13d27-8e55-4f10-8da0-c41d50ccdce3 reapplied nondefault input settings and returned
healthy root/UI; defaults restored afterwards. BootCurrent 0005, BootOrder restored
0005,0000, DriverOrder 0000,0001, no BootNext; protected hashes unchanged. Installing
eudev added a dev-settle provider that starts udev in normal boot; mdev sysinit is
retained. SSH can return before OpenRC desktop/watch readiness, so verify actual
services and tty7. Human gestures remain unobserved; X tests are software evidence.
See docs/desktop.md. Preserve management and boot/recovery paths during iteration.

Linux Companion shell 0.1 is installed at /opt/companion/desktop. OpenRC
companion-control and companion-desktop start by default; the UI runs as
companion-ui on Xorg tty7, independent of root SSH/watch. Real brightness controls,
disk-persisted appearance, UI crash respawn and normal reboot startup passed.
Healthy boot 05383219-e31e-4bd9-9dae-8f2e48d6debc returned root and UI, with saved
appearance. BootCurrent 0005, BootOrder restored to 0005,0000 after firmware
appended NIC entries, DriverOrder 0000,0001, BootNext absent; protected hashes
unchanged. See docs/desktop.md. No boot loader/kernel/network replacement occurred.
Input devices opened successfully; injected X events are not physical human touch
acceptance. Preserve management, consoles and SSD recovery when iterating UI.

Preserving management access takes priority over firmware/network experiments.
The Inspiron 7506 failed native PXE DHCP and stopped at Dell SupportAssist
"No bootable devices found" on 2026-09-27. Its SSD BootOrder did not provide
automatic recovery from that attempt. Do not repeat native PXE BootNext/BootOrder
tests or bypass the tool guard through direct SSH. Do not disable the working
SSD management/recovery paths to prove independence.

Use the proven SSD Companion loader for future preboot network experiments,
with bounded network operations and explicit transfer to the normal management
loader on failure. Keep the known working default boot entries and driver order.
This still depends on the SSD; report that limitation clearly. A VM PXE pass
does not establish physical firmware fallback or an independent reset channel.

Access returned after the owner cleared the halt. BootCurrent 0005, BootOrder
0005,0000 and DriverOrder 0000,0001 are restored; BootNext is absent and boot
1ba912c9-289e-4c0d-aa8c-03b18f40b52a is confirmed healthy. Firmware added a
duplicate NIC entry 0001, so do not rely on fixed NIC entry numbers alone.
Verify installed boot/recovery files before any reboot. Do not claim control
of a firmware error screen through Ethernet.

Normal startup now uses the SSD NV bootstrap at the existing Driver0001 path.
Its SHA256 is 2ebfd763305cddb886fae0793f60e08ae073f919b72ca1d6b405e61eb7e08df4;
the original driver is companionextx64.ssd-backup.efi. The owner NVRAM payload
is unchanged. Boot f8fef63e-2976-4c0c-a384-28fe70247786 verified automatic NV
read/load/start/service success and healthy root reconnection. Dell appended
NIC entries to BootOrder during that normal reboot; they were recorded and
BootOrder 0005,0000 restored. DriverOrder remains 0000,0001 and BootNext absent.
See docs/nv-startup.md. Do not mistake the volatile startup diagnostic variable
for persistent payload storage or claim SSD-independent startup.

The HTTP interface inspection completed on healthy boot
b8c1de82-8a89-42de-945f-17d82af25c04, with BootCurrent 0000 (verified SSD
fallback). BootOrder 0005,0000 and DriverOrder 0000,0001 are restored; BootNext
and temporary inspection entry 0004 are absent. HTTP/TLS service bindings are
present but no standard HttpBootDxe binding/HII configuration or URI LoadFile
provider was observed. See docs/http-interface-inspection.md. Future work must
preserve the working SSD management paths and report this dependency.

The read-only HTTP prerequisite probe completed on healthy boot
dbb8c399-d20b-4ee7-8218-d0512098a1bd. BootCurrent was 0000; BootOrder 0005,0000
and DriverOrder 0000,0001 are restored, BootNext and temporary entry 0004 absent.
Live FV2 sections match pinned HttpBootDxe PE/DEPEX and DellBoardPolicyDxe PE;
same-NIC HTTP/DHCP bindings and policy image ownership are verified. The HTTP
driver was not loaded/started and its policy methods were not invoked. See
docs/http-prerequisite-inspection.md. This remains SSD-dependent.

Targeted HttpBootDxe initialization succeeded on healthy boot
149f6b3c-138f-47d0-bf0c-0816f077a8cb. Its two IPv4/IPv6 binding interfaces and
four component-name interfaces were removed successfully before SSD handoff.
BootCurrent 0000; BootOrder 0005,0000 and DriverOrder 0000,0001 restored;
BootNext and temporary entry 0004 absent. The test did not explicitly attach
a controller or request HTTP transfer. Avoid the vendor unload routine: it
enumerates/disconnects controllers and calls an unverified packed cleanup
service. See docs/http-driver-physical-initialization.md. Independent startup
and independent reset remain unproven; preserve SSD management/recovery.

Physical Ethernet Supported checks passed for IPv4 and IPv6 on healthy boot
baacdd87-1731-4568-8a13-b7732b226f96. BootCurrent 0000; BootOrder 0005,0000
and DriverOrder 0000,0001 restored; BootNext and temporary entry 0004 absent.
All owned binding/name interfaces were removed. This did not attach a
controller or transfer HTTP data. Saved-code Stop fixtures show that success
can leave controller state installed if the supplied handle has no resolving
MNP/DHCP open records. Resolve full Start/HII cleanup and inspect live open
relationships before physical attachment; do not assume Stop success proves
cleanup or bypass driver calling restrictions. See docs/http-ethernet-support.md.

The read-only cleanup relationship probe returned healthy on boot
73ece2ba-139c-4c0c-8369-0c795cd39c40. BootCurrent 0000; default orders restored,
temporary Boot0004 removed, BootNext absent. Physical NIC MNP/DHCP4 protocols
are absent; an existing DHCP4 child has a BY_DRIVER record pointing to the NIC.
Do not disconnect that existing firmware child. Full saved-code Start/Stop
fixtures show ignored cleanup failures can leave interfaces pointing into freed
memory even when Stop succeeds. Do not infer safety from its status or perform
physical attachment before reviewing targeted disconnect and failure recovery.
See docs/http-cleanup-investigation.md. No driver was loaded or attached in
the physical cleanup inspection; SSD management/recovery remains required.

Saved Dell DXE targeted DisconnectController verification passes eight offline
cases. The new owned DHCP child is the nominal target; the original NIC and
existing firmware DHCP child do not invoke the new driver's Stop in the replay.
Four injected cleanup failures still return success with leftover state, and
offline postconditions reject handoff. These gates are not deployed; autonomous
failure reset remains unproven. Do not attach physically based on nominal replay
success. Root management remains healthy on boot 73ece2ba-139c-4c0c-8369-0c795cd39c40.
See docs/http-targeted-disconnect-verification.md for fixture limits and evidence.

Startup presentation iteration 1 is installed and verified on healthy normal
boot c4e357e0-3ffd-4a83-9284-4e18ea0f5053. BootCurrent 0005; BootOrder restored
to 0005,0000 after Dell appended NIC entries, DriverOrder 0000,0001, no BootNext.
The external /boot/efi/companion/recovery/grub.cfg remains the active menu;
do not mistake /boot/grub/grub.cfg for that configuration. The new welcome
service only prepares local presentation. Stable/rescue journal behavior and
all EFI loader/stable kernel hashes remain intact. See docs/startup-presentation.md
for backups and evidence. A native kernel is implemented and tested in VM,
but is not installed or selected on the Dell. Linux root SSH will not coexist with an early
native kernel; retain management and validate recovery before unattended native runs.

At the owner's request for a clean startup, WarningsAndErr was set to PromptWrnErr
to remove Dell's headless banner. That choice was later reverted to ContWrn
after the logo stall described at the top of this file. PowerWarn and
DockWarningsEnMsg remain Disabled.
Normal reboot returned healthy root on boot 7dc09e62-a246-408d-a0af-7aba72e2001c,
and the owner confirmed the banner absent. Default boot/driver orders are restored,
BootNext absent; protected EFI/kernel hashes unchanged. Unsuppressed future
warnings can now require local input, which SSD rescue/SSH cannot bypass. Do not
claim this policy retains continuation on every warning. See docs/startup-presentation.md.

Hidden GRUB presentation is installed with timeout_style=hidden, timeout=1;
Escape reveals the recovery menu in VM tests. Normal physical reboot returned
healthy root on 7c465eeb-59ca-46c5-8f6a-31954e668b8c. Protected file hashes remain
unchanged; BootCurrent 0005, BootOrder 0005,0000 restored, DriverOrder 0000,0001,
BootNext absent. Native EFI/ELF builds pass host fixtures and six isolated VM
cases, including own paging/timer, exceptions, page faults and ACPI inventory.
These are VM-only builds; native physical management and independent reset
remain unimplemented. Do not change the Dell default to native on this evidence.
See docs/native-kernel-verification.md and docs/kernel-plan.md.

Native PCI ECAM inventory now matches QMP in isolated VM tests. Device memory
mapping rejects RAM/reservation overlaps; no PCI config writes or BAR sizing.
Native i8042 typing passes mock controller fixtures and VM key/pixel checks.
Diagnostic builds default to q35 i8042; production builds require explicit
--keyboard i8042. Dell Linux confirms isa0060/serio0 keyboard but FADT boot flags
0x0001 omit the i8042 bit, so do not use that bit alone as an absence test.
Seven VM cases and deterministic builds pass. None of these native drivers has
run on the physical Dell; preserve Linux management/recovery and default orders.

The isolated --vm-network profile now proves native e1000 ARP/ICMP/UDP,
authenticated status and kernel-triggered q35 reboot/reconnect with fresh nonce;
old-boot commands are rejected. The key bytes 0..31 are PUBLIC FIXTURE DATA,
not production credentials. The profile requires VM diagnostics; production
compiles out its endpoint and port-0xcf9 reset path. Do not deploy this build
to the Dell or a LAN, or infer physical reset/control from the VM result.
Dell Realtek USB Ethernet, IOMMU ownership, production keys and autonomous
physical recovery remain pending. Preserve SSD management/recovery defaults.
See docs/native-network-verification.md.

The isolated --vm-usb profile now owns QEMU 1b36:000d xHCI command/event rings
and reads root-port device/configuration descriptors at full/high/SuperSpeed.
It halts before releasing DMA; failed halt retains allocations. Four USB VM
cases and host failure fixtures pass. Production excludes this startup path.
This is descriptor inspection, not bulk Ethernet or physical Intel xHCI support.
No native USB driver has run on the Dell. Do not infer physical management or
reset from enumeration; preserve Linux management/recovery and default orders.
See docs/native-usb-verification.md.

The isolated --vm-usb-network profile now configures owned xHCI bulk IN/OUT
and CDC ECM 0525:a4a2 Ethernet. Thirteen VM cases pass, including 140 USB packet
round trips, exact packet-size/ZLP boundaries, authenticated kernel reboot and
fresh-nonce reconnect. Host C tests cover bulk failures, copy bounds, zero/full
receives and oversized-frame draining; QEMU caps Ethernet input at 2048 bytes,
so multi-TD oversized draining is host evidence only. Public fixture credentials
and q35 reset remain excluded from production. No native driver was installed
or selected on the Dell. Intel 8086:a0ed ownership/IOMMU, physical 2357:0601
Realtek protocol, production credentials and Dell recovery/reset remain pending.
Final read-only root check confirms healthy boot 7c465eeb-59ca-46c5-8f6a-31954e668b8c,
unchanged protected hashes and default orders, no BootNext. Preserve the SSD
management/recovery paths. See docs/native-usb-network-verification.md.

Read-only sysfs inspection found a CDC ECM configuration 2 on the actual
2357:0601 adapter, alongside the active r8152 vendor configuration 1. Captured
SuperSpeed descriptors and actual C setup pass host models (68 controller/setup
cases, 2295 malformed descriptors); the 13 VM cases still pass. This is not
physical ECM traffic proof. Do not switch the live USB configuration or detach
its Linux driver while it is the only management link. The Intel controller's
Linux BAR is 0x601f260000..0x601f26ffff, 64 KiB above the native 64-GiB mapping
limit. Obtain preboot assignments and ownership/IOMMU evidence before native
DMA; an absent Linux IOMMU group does not prove translation is off at handoff.
Root remains healthy on 7c465eeb-59ca-46c5-8f6a-31954e668b8c, default orders and
protected hashes unchanged, BootNext absent. No physical reboot/native boot
occurred. See docs/dell-usb-ecm-investigation.md for the fixture and next gates.

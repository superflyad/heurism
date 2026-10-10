# Current product direction

On 2026-10-10 the C Editor gained numbered rows, visible pointer-placed
caret, horizontal scrolling, UTF-8 column movement, bounded undo/redo and
per-process locked drafts with inactive-draft recovery and legacy migration.
The isolated VM gate tests two simultaneous Editors, recovery and migration.
The VM release is
`/opt/heurism/native/releases/heurism-os-20261010T172320Z-20197`, verified
after checked reboot to `e507e4db-871e-4de0-ab29-8637d3721833`; checkpoint
`Heurism-Editor-migration-20261010` has UUID
`8e186c4c-ee4f-4eda-86d3-98a76f0cac5d`. The VM is intentionally locked on
boot; pointer wake painted its password prompt.
The Dell release is
`/opt/heurism/native/releases/heurism-os-dell-20261010T172614Z-14907`,
activated with the guarded installer on the existing boot
`edb102d9-25c6-409c-aa17-181fea3eaf41`. Root SSH/watch/control/UI,
protected hashes and SSD boot orders remain healthy. No Dell reboot or
physical display acceptance occurred. Build packages were removed. The next
desktop work is a coherent C design system, Editor selection/clipboard/find,
better window arrangement, and Settings/lock/notification consistency. See
`docs/heurism-modern-workspace.md`.

On 2026-10-10 the owner rejected the dashboard-like C desktop as dated and
asked for a modern, window-first Heurism workspace. The new C desktop 0.7
removes the large app and system cards. It has a persistent top status panel,
compact centered dock with drawn app symbols and live window switching,
searchable installed applications through GIO, borderless launcher and quick
controls, and a quieter Xfwm theme. Its C settings still expose Dell hardware.
The active sealed VM release is
`/opt/heurism/native/releases/heurism-os-20261010T160753Z-5683`; normal VM reboot
returned boot `faecd695-a4c4-4dc3-b8b5-6645e5b7ecea` with root SSH/watch,
control, UI and protected hashes healthy. The ready checkpoint is
`Heurism-modern-workspace-20261010`, UUID
`daa80285-d64f-415d-ad99-d5c4e904369e`. The active Dell release is
`/opt/heurism/native/releases/heurism-os-dell-20261010T161019Z-5092`, activated
with the Dell-only rollback gate on the existing boot
`edb102d9-25c6-409c-aa17-181fea3eaf41`. Isolated Dell-size workspace and
hardware-settings tests passed; the live UID-1000 UI, panel, dock, four
management services, the live PAM lock query on the session D-Bus, and eight protected hashes passed. Temporary build
packages were removed. The Dell was not rebooted, and physical viewing,
touch acceptance and persistence across a Dell reboot remain unverified.
See `docs/heurism-modern-workspace.md`. The remaining C Files/Editor and
Settings pages still need a broader visual and usability redesign; do not
call the full distinct-desktop goal complete. Keep Dell firmware/SSD/SSH
guards and the locked Xfce recovery session intact.

On 2026-10-10 the physical Dell now selects `heurism` in
`/etc/companion/native-session-mode`. The guarded Dell installer sealed and
activated `/opt/heurism/native/releases/heurism-os-dell-20261010T153748Z-15862`
with the C workspace 0.6, Heurism Xfwm theme, launcher and quick controls.
The Dell assembler passed Xfce recovery, shell/PTY, Dell hardware control,
settings/device/network/sound UI, workspace/quick/terminal, Files/Editor and
sealed-release rejection tests. On live Xorg :0 the C workspace and Xfwm4
passed `heurism-release health`; Xfce panel/desktop are absent, and the Xfce
PAM screensaver reported active. Killing the C workspace selected `xfce` and
returned a healthy locked Xfce session within eleven seconds; the C session
was then restored. Root SSH, watch, control, Ethernet, saved brightness and
audio, eight protected hashes and boot `edb102d9-25c6-409c-aa17-181fea3eaf41`
stayed healthy. BootCurrent 0005 and BootOrder 0005,0000 were unchanged. No
reboot or boot-path change occurred. Physical display/touch interaction and
post-reboot persistence remain unverified. See
`docs/heurism-dell-live-session.md`. The shared UID-1000 X11 session and
unencrypted root disk still limit security; do not claim untrusted-app
isolation. Keep authenticated SSH/watch independent and never reboot the Dell
remotely without local recovery readiness.

On 2026-10-10, Heurism desktop 0.6 from the sealed VM release was tested on
the physical Dell inside a private 1920x1080 Xvfb :93 session with Xfwm4 and
an X11 authorization cookie. The C workspace, quick controls with the Dell's
real Ethernet address, launcher, terminal and Super+Space passed. Its VM and
Dell binary SHA256 was
ec404f0bbf1c0cff3639128507e9e5546865afca0b14cdecc97bc19ebe8e5e0e.
The Dell boot ID, active release and production Xfce :0 session were unchanged;
SSH, watch, C control and eight protected hashes remained healthy. This is an
isolated display-size gate, not physical screen/touch or full-session proof.
The installed release at that test predates the Heurism Xfwm theme and new
session scripts. See docs/heurism-dell-workspace-gate.md for that earlier gate.

On 2026-10-10 the VM C dock's connection indicator gained Heurism quick
controls: actual Ethernet address, honest virtual-audio status, checked C
appearance, Settings, Network, Lock and Power routes. The active sealed release
is `/opt/heurism/native/releases/heurism-os-20261010T152041Z-51720`.
The tracked Xvfb interaction test passed dock opening, panel reuse, appearance
change/restoration, Settings, Network, Power and Escape. A second tracked
isolated test clicked Quick Lock and verified Xfce's PAM screensaver active.
Both passed again on the installed release after checked C reboot to fresh boot
`e73fbbb1-9e62-4b4a-a528-04518fc8a8cf`. The C desktop, theme, release,
SSH/watch/control were healthy; the original Night preference was restored and
build packages removed. The prechange checkpoint is
`Heurism-before-quick-controls-20261010`, UUID
`b3803c66-d1d6-4f8c-8a39-0ec365da9b2b`; the ready checkpoint is
`Heurism-quick-controls-ready-20261010`, UUID
`749dc961-e3fa-47bf-87d3-ae5e9b5c50c7`. See
`docs/heurism-quick-controls.md` and
`build/prime-vm/heurism-quick-controls.png`. The Dell remains on Xfce and
was not changed. The full distinct desktop goal remains open.

On 2026-10-10 the VM C launcher gained live open-window search and switching.
The tracked isolated Xvfb test opened C Terminal and Files, searched for the
running Terminal from Files, and verified that Enter made Terminal active.
The 1280x800 preview is `build/prime-vm/heurism-window-switch.png`. The
guarded installer activated sealed release
`/opt/heurism/native/releases/heurism-os-20261010T150745Z-16471`.
Checked C reboot returned fresh boot `b7736202-94de-413f-9f08-de130ece7fce`
with verified C desktop, theme and root SSH/watch/control healthy; the same
launcher test passed again against the installed release. Build packages were
removed. The prechange checkpoint is `Heurism-before-live-window-search-20261010`
(UUID `24f16f20-a3d2-4a40-9c93-91e969b6eb06`); the ready checkpoint is
`Heurism-live-window-search-ready-20261010` (UUID
`ba785c18-865d-43c1-92e3-050192feaa8f`). The Dell remains on Xfce and
was not changed. See `docs/heurism-launcher.md`. The full distinct desktop
goal remains open.

On 2026-10-10 the VM C workspace gained a process-scoped Super+Space launcher
shortcut. It opens the existing searchable C launcher from a focused Terminal;
Xfce recovery keyboard settings remain unchanged. The tracked isolated Xvfb
test caught and fixed a launcher focus timing failure before activation. The
active sealed release is
`/opt/heurism/native/releases/heurism-os-20261010T150201Z-40393`.
Checked C reboot returned fresh boot `b822a784-1798-4585-8445-bcf25556e18b`
with verified C workspace, theme, SSH/watch/control and no shortcut grab
warning. Build packages were removed. The prechange checkpoint is
`Heurism-before-keyboard-launcher-20261010`, UUID
`14e5e9c1-f440-4d5c-97ae-f02cab75033c`; the ready checkpoint is
`Heurism-keyboard-launcher-ready-20261010`, UUID
`bb5c3d12-32ab-47aa-9a13-c30241b6f3d1`. The Dell was not changed.
See `docs/heurism-launcher.md` and the 1280x800 preview at
`build/prime-vm/heurism-workspace-shortcut.png`. The full distinct desktop
goal remains open.

On 2026-10-10 the VM C workspace gained a searchable C/X11 launcher window
opened by the Heurism dock button. Search, arrow/Enter selection, Settings,
Power, Escape and repeated-click reuse passed an isolated Xvfb UI test; see
`build/prime-vm/heurism-launcher-preview.png`. The active release is
`/opt/heurism/native/releases/heurism-os-20261010T145206Z-34622`. Checked C
reboot returned fresh boot `66ee461d-3216-4c21-87ee-baadc68b3292` with
verified C workspace, Heurism window theme and root SSH/watch/control healthy.
The prechange checkpoint is `Heurism-before-launcher-20261010`, UUID
`246de076-ca3f-401d-b398-01975e64d975`. Build packages were removed.
The ready checkpoint is `Heurism-launcher-ready-20261010`, UUID
`080485cc-747e-4355-9f76-192703559050`.
The Dell remains on Xfce; the launcher was not physically deployed. Xfwm,
shared UID-1000 X11 and unencrypted disk remain. See
`docs/heurism-launcher.md`.

On 2026-10-10 the VM `heurism` C workspace gained original Heurism Xfwm4
window decorations, generated by host-only Python into a 4.3 KiB sealed
asset. Files and C Terminal showed active/inactive frames in an isolated Xvfb
capture at `build/prime-vm/heurism-theme-preview.png`. The VM release is
`/opt/heurism/native/releases/heurism-os-20261010T144037Z-34721`. A checked C
reboot returned boot `aa8b6855-6782-4c32-b70e-03389719ca77` with verified
release, C workspace, selected theme and root SSH/watch/control healthy. The
prechange checkpoint is `Heurism-before-window-theme-20261010`, UUID
`585347f5-5703-44cf-8fb6-0606ff20e16c`. Temporary build packages were
removed. The Dell was not changed; its established Xfce desktop remains active.
See `docs/heurism-look.md`. Xfwm/Xorg and shared UID-1000 remain limitations;
do not call this a fully independent desktop or migrate to the Dell solely on
the basis of the visual pass.

On 2026-10-10, `CompanionDev` moved from Xfce's visible panels/desktop to a
Heurism C workspace selected by `heurism` in
`/etc/companion/native-session-mode`. Its sealed release is
`/opt/heurism/native/releases/heurism-os-20261010T142713Z-21679`.
Xfwm4 remains the window manager and Xfce's PAM screensaver is required
before C workspace health; root SSH/watch/control stay independent. The C
workspace has a new centered app/system layout and running-task dock. Files
and Terminal opened in isolated Xvfb UI clicks. A live locker-failure test
automatically selected the locked Xfce recovery session and retained root
management; reselecting Heurism and a checked reboot returned healthy boot
`0e0739b9-f3b1-4692-b562-2120b64026ca`. Protected hashes passed and
temporary build packages were removed. The prechange VM checkpoint is
`Heurism-C-workspace-experiment-20261010`, UUID
`f0db5c40-16dc-4800-9ca5-35aca8b363fe`. The Dell was untouched and
still uses Xfce. This does not close signed inactive-root staging,
physical firmware recovery, shared-X11 security or Dell UI acceptance.
See `docs/heurism-shell-vm.md`.

On 2026-10-09 local / 2026-10-10 UTC, VM-only C `heurism-slot` was added to
fresh images for inactive-root verification, one-time B queue, health-gated
B renewal and fallback. Its exact-DMI/layout guards reject the current
single-root VM and Dell. The final isolated candidate VHDX SHA256 is
`bde276fe736d31f6142f7f0b500f25fb3b41a6824bd843063049f8d19aadc023`.
It rejected altered B init and a missing B SSH service link, booted healthy
B `7ea3012c-4ff8-4e41-ab60-4e46a1ad9310`, renewed after 50 seconds and
cleared the renewal for A. A separate injected B startup hang in an earlier
candidate was reset by the explicit Hyper-V host gate and returned healthy A;
the final manifest now protects `/sbin/init`. The candidate is off, and
`CompanionDev` is healthy on boot `ab3f3709-0a9b-486f-978b-c289430add29`
with its earlier single-root disk and pinned identity. The Dell was untouched.
Signed inactive-root staging, automatic renewal, a persistent host watcher
and an independent physical recovery channel remain. See `docs/ab-updates.md`.

On 2026-10-09 local / 2026-10-10 UTC, the fresh VM image builder gained a
four-partition A/B layout with shared `/var/lib/companion` data. The isolated
`HeurismCandidate` VM booted A, one-time B, then A, preserving the same data
marker and passing release/management health. A deliberate fatal B init in
that disposable VM returned to healthy A without a second host reset;
B's original init was restored. Candidate VHDX SHA256 is
`1e7f1235bbe9b0374cf5f60e126367642e1be6ed5a4602759a73ddd4117dda5b`.
`CompanionDev` resumed its original pinned SSH identity on fresh boot
`c8cfc619-4ca4-4889-b285-f8590445490c`; its current installed disk is
still the earlier single-root layout. The Dell was untouched and remains
single-root. This is a VM boot prototype, not a full updater: A anchors GRUB,
and staging, health promotion, hang watchdog, shared-data migrations and
independent physical recovery remain. See `docs/ab-updates.md`. Do not
repartition or change the Dell boot path unattended.

On 2026-10-09 the owner asked to close session-security and whole-system
update/rollback gaps. The Dell root console's empty password was replaced
with a random local password; `companion-ui` received a separate random PAM
unlock password. Owner-only Windows ACL credential files are under ignored
`artifacts/ssh/dell-root-console-recovery.txt` and
`artifacts/ssh/dell-desktop-unlock.txt`. Pinned root SSH remains key-only.
VM-tested `xfce4-screensaver` startup locking, five-minute idle locking and
locker-crash session restart are now active on the Dell in sealed release
`/opt/heurism/native/releases/heurism-os-dell-20261010T013332Z-20380`.
The Dell's boot ID is still `edb102d9-25c6-409c-aa17-181fea3eaf41`:
**no physical reboot of this release has been run**. SSH/watch/control/Xfce,
locked-session query, protected hashes and default NVRAM orders passed after
graphical restarts. VM release
`/opt/heurism/native/releases/heurism-os-20261010T013054Z-3396` passed
hard-reset startup lock, password unlock and forced-locker-crash recovery.
VM host-checkpoint `upgrade-system` updated 15 packages including Linux LTS
to 6.18.55-r0 and returned fresh healthy boot; injected failures restored
the previous full disk and fresh boot. The fresh image builder now gives root
and desktop unique nonempty passwords and includes the lock by default.
Image SHA256 `50aa6535210a33adcf6c01a2b5f310b3020a3b3f4042d104efae6450380cd4e5`
booted isolated `HeurismCandidate` with lock/health/protected files verified.
This does not create a Dell whole-OS rollback: Dell remains single-root ext4
with no independent firmware reset. Do not update its kernel/bootloader or
repartition unattended. X11 shared UID 1000 and unencrypted disk remain
material security gaps. See `docs/session-security-updates.md`.

On 2026-10-09 the owner directed work toward substantive OS usability beyond
wallpaper. Source revision `0572384` added bounded Tab completion to the C
shell and real PTY regression cases. VM release
`/opt/heurism/native/releases/heurism-os-20261010T005501Z-24116` passed shell,
interactive/job-control, desktop and control sidecars, a live UID-1000 terminal
completion/file operation, and checked reboot to fresh boot
`111a682d-c551-4eb6-879d-b1791b161726`. Prechange checkpoint is
`Heurism-shell-completion-pre-20261009` UUID
`1674cf62-28f7-40b6-a09d-826417e730e1`. Temporary VM build packages were
removed. Guarded Dell installer assembled and activated
`/opt/heurism/native/releases/heurism-os-dell-20261010T010017Z-7933` on
existing boot `edb102d9-25c6-409c-aa17-181fea3eaf41` without reboot.
The Dell now has the same C shell and VM-tested clipboard terminal binaries;
shell/job-control PTY sidecars, control/desktop/apps, release/corrupt-clone,
live UID-1000 terminal completion and cross-app clipboard paste passed.
SSH/watch/control/UI, C power and protected SSD/NVRAM checks remain healthy.
This new release has not had a Dell OS reboot. Keep the remote-power guard;
the earlier firmware-logo stall has no independent recovery. See
`docs/shell-completion.md` and `docs/terminal-clipboard.md`.

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
The on-disk `current` symlink points to the sealed release and
OpenRC's default runlevel links enable `heurism-control` and `heurism-desktop`.
The desktop init script launches `/opt/heurism/native/current/session.sh`;
Xfce's user profile stores the wallpaper and panel settings. A Dell desktop
service restart returned healthy UID-1000 Xfce with the same release and
painted look. With the owner available for local recovery, a checked C reboot
returned fresh boot `edb102d9-25c6-409c-aa17-181fea3eaf41` and the same
sealed release, SSH/watch/control/UI health, saved settings and actual painted
1920x1080 desktop. Dell appended known USB NIC entries to BootOrder; after
checking their paths and MAC, `BootOrder` was restored to `0005,0000`.
`BootCurrent` is `0005`, `DriverOrder` is `0000,0001`, BootNext absent, and the
protected SSD/NVRAM verifier passes. This proves persistence across one normal
Dell reboot, not unattended recovery from a future firmware stall. Keep the
remote-power guard active. See `docs/heurism-look.md`.

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

The owner asked for a usable revamp with an established desktop. At the
2026-10-03 bridge milestone, the default VM and Dell user sessions ran Alpine
Xfce 4.20 over Xorg with Thunar and Mousepad. Companion-owned shell, terminal, settings, control and release
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

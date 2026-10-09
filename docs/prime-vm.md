# Companion on PrimeServer

Companion development now targets a separate Hyper-V Generation 2 VM,
`CompanionDev`, on the owner's authenticated `primeserver` host. The Dell is
left untouched. This runs the Linux Companion desktop and management services;
the optional native kernel fixtures are a separate research target.

The VM has four virtual CPUs, 4 GiB fixed RAM, and a 24 GiB dynamic VHDX in
`D:\HyperV\CompanionDev`. Its private address is `172.28.50.3/24` on the existing
`PrimeLinuxInternal` switch, with NAT through `172.28.50.1`. The existing
`PrimeLinux` VM supplies the image builder; its applications are not replaced.
No new LAN listener, port forwarding rule, or physical USB/disk write is required.

## Platform and access

The guest uses Alpine 3.24.2, Linux LTS, OpenRC, Xorg/libinput, Openbox,
the [Companion C runtime](native-runtime.md), Firefox and Onboard. Tk remains
only in the sealed legacy recovery release. Hyper-V supplies
the virtual display, keyboard, pointer, storage and Ethernet interfaces.
The explicit `hyperv-dev` profile selects fbdev instead of the Dell Intel
display. It requires protected root-owned configuration and actual Microsoft
Virtual Machine DMI identification. An absent configuration retains the existing
Dell policy. The Dell boot guard has not been removed or relaxed.

Guest SSH traverses `primeserver` and the existing `prime-linux` jump host to
the private address. Direct Windows-to-guest forwarding intermittently failed
during cold startup; the Linux relay is the proven route. Its availability is
required for this guest SSH path. Hyper-V control still uses PrimeServer directly.
It uses a separate key pair under `artifacts/prime-vm/`, with a pinned
host public key obtained from the authenticated builder. Private client keys
stay on this computer. SSH password authentication is disabled. Management is
independent of the desktop and runs automatically at boot; its watch service
checks the actual SSH handshake and records health for the current boot.

Hyper-V provides a second management path outside the guest OS. PrimeServer SSH
can inspect the VM, read the virtual console, reset the VM, or restore a
checkpoint while guest SSH is unavailable. This independence applies to this VM;
it does not establish independent physical reset or BIOS halt control on the Dell.
The host and its own power/network availability are still prerequisites.

## Iteration

Run commands from the repository on this computer:

```powershell
python tools/prime-vm.py status
python tools/prime-vm.py guest --command 'rc-status'
python tools/prime-vm.py capture --name companion-vm --require-ui
python tools/prime-vm.py console
python tools/prime-vm.py wait
python tools/prime-vm.py checkpoint --snapshot My-experiment-baseline
python tools/prime-vm.py reset
python tools/prime-vm.py restore --snapshot Companion-C-runtime-ready
```

`publish` belongs to the old Python/Tk release path and now refuses to run
while the C runtime is active. Build and activate C releases with
`userspace/native/install-native-vm.sh` on CompanionDev as documented in
[native-runtime.md](native-runtime.md). Its health gate restores the prior
release on failure and retains root SSH/watch. Checkpoints and reset/restore are bound
to the stored Hyper-V VM UUID, rather than trusting its name alone. Restoring a
checkpoint also restores its disk contents, so subsequent guest changes are lost.
`wait` polls authenticated boot/UI health for up to 90 seconds. With
`--after-boot UUID`, it also requires a new guest boot. Checkpoint names must be
unique; the default restore target is the verified `Companion-C-runtime-ready`
baseline. `Companion-ready-v2` remains available as the earlier legacy baseline.

For direct interaction, open Hyper-V Manager on PrimeServer and connect to
`CompanionDev`, or use `vmconnect.exe primeserver CompanionDev` from a Windows
machine with Hyper-V management tools and the existing access permissions.

## Building another image

`python tools/prime-vm.py build` creates a new image on PrimeLinux from the official
Alpine minirootfs, checking its published SHA256 and signed APK packages. The
builder partitions only its newly created disk file, never a host disk. It
installs GRUB's removable UEFI path without writing host NVRAM. The kernel line
explicitly specifies `rootfstype=ext4` so the initramfs loads the root driver.
The guest has its own protected boot/management manifest. VM power requests
verify it, management health, UEFI boot and absence of a pending BootNext.

`create` refuses an existing VM or directory. Secure Boot is disabled only on
this development VM for Alpine's unsigned loader. Automatic start is enabled
after a 20-second delay; standard checkpoints preserve guest disk and memory.
The image builder scripts do not copy Dell firmware, NVRAM or private identities.

## What the VM can prove

Desktop startup, applications, document persistence, keyboard/pointer interactions,
API validation, UI recovery, release rollback, guest reboot/reconnection and
host reset/recovery can be tested here. Display and control data come from the
actual virtual guest; unavailable interfaces are reported as such.

Hyper-V does not provide the Dell touchpad/touchscreen, wireless adapter,
backlight, battery, SOF speaker or Dell BIOS attributes. The basic console has
no physical audio device; PulseAudio may expose a null sink. Tests of its volume
controls do not establish audible output. Wi-Fi association, finger gestures,
physical audio quality and Dell firmware/embedded-controller behavior still
need later hardware acceptance. Do not treat VM success as those hardware proofs.

## Verification

Deployment and acceptance evidence is kept under `build/prime-vm/` locally and
`/var/lib/companion/desktop-stage/evidence/` in the guest. Boot/recovery results
are recorded after execution, separately from this intended test scope.

The installed release `/opt/companion/releases/20260928T005954Z-7763` passes six
control, eight network/power, eight platform and five release fixtures. Actual
Tk/application tests prove editor private saves and draft recovery, Files
create/rename/Trash/restore, terminal UID 1000, root terminal UID 0, Firefox,
window minimize/restore and Onboard pointer-generated `q` input. BIOS and wireless
tests explicitly verify unavailable interfaces; audio controls operate a null sink.

A sealed failing clone restored the working UI automatically while management
remained healthy. Normal reboot from `b7d28b05-6118-491f-9707-2a249c307508` to
`c39021d4-983a-4f0f-8ad9-36ec06e07a29` retained the release, preferences and a
UID-1000 document. The UI initially stayed black without pointer motion even
though health polling succeeded. Flushing Tk painting alone was insufficient
after a hard reset. The VM now initializes its virtual pointer once in the empty
header and flushes painting before reporting health. This is explicit software
cursor placement, not human movement or a claim of physical gesture acceptance.
Fresh boot screenshots require no owner interaction; `capture --require-ui`
rejects the black-frame regression using actual X framebuffer pixels.

The original `Companion-ready` host checkpoint restored disk contents and the running
guest state: a file deliberately created after the checkpoint was absent after
restore, and authenticated management/UI health returned. A host hard reset is
also verified independently of the guest reboot API. This tests VM recovery;
PrimeServer itself was not rebooted. The current ready checkpoint is
`Companion-ready-v2`, containing the cursor initialization fix. The final hard
reset returned healthy root/UI on `df974b4d-fb78-4c39-a34c-7c421d696743`.
Its 1280×800 framebuffer contains 1,371 distinct colors and is fully painted.

## Desktop capability presentation

Release `/opt/companion/releases/20260929T232121Z-108070` was hot-published on
that boot. Home now shows the VM's actual Ethernet address, labels the PulseAudio
null sink as virtual audio without speakers, and disables the unavailable Dell
BIOS tile. The Network window shows Ethernet without Wi-Fi credentials or a
Connect action. Overview reports an unavailable power source when the VM has
neither battery nor external-power sensors. Power and Administrator prompts say
"computer" on both platforms.

The versioned release passed install control/network fixtures and the live Tk
workspace test on an isolated X display, including the capability text, missing
Wi-Fi form, sound label, Files, Editor, terminal, Firefox and sound controls.
`capture --require-ui` accepted the actual VM framebuffer after publish, and
release verification plus SSH, watch, control and desktop service checks passed.
Normal API reboot returned fresh boot `01aed4f4-5502-40b1-9289-eec353cf79e4`
with the same release, healthy SSH/watch/control/UI, saved preferences and a
UID-1000 document. The boot manifest and UEFI selection passed the VM guard.
`capture --require-ui` accepted the painted framebuffer after reboot.
Evidence: `build/prime-vm/desktop-capabilities.png`,
`build/prime-vm/desktop-capabilities-reboot.png` and the guest's
`/var/lib/companion/desktop-stage/evidence/workspace.json` and
`workspace-reboot.json`. A host hard reset has not been run for this release.
The `Companion-ready-v2` checkpoint still contains the previous release.

## Desktop 0.4 workspace

Release `/opt/companion/releases/20260930T000247Z-9693` replaces the launcher
grid with a desktop workspace, file and application shortcuts, and a centered
dock that stays visible while applications run. The dock offers quick launch,
a menu for settings and power actions, live window buttons, network state and
the clock. Openbox recognizes its dock window and reserves 84 pixels so maximized
apps stay above it. The Home helper minimizes application windows but leaves the
dock in place. Files windows and Onboard are positioned above the reserved area.
Companion's control service and root management remain separate from the user
desktop process.

The live VM display proved Files open above the dock, task button restoration of
an iconified Files window, and Windows+D minimizing Files while keeping the dock
visible. The application test again passed Files, Editor, Firefox, normal/root
terminals and Onboard-generated `q` input. The isolated Tk workspace test passed.
After a normal checked API reboot, boot `0005192e-cbcc-4740-8f0c-292167230274`
returned the same release with tty7, SSH/watch/control/UI, preferences and a
UID-1000 document intact. `capture --require-ui` accepted the actual desktop
frame. Evidence is in `build/prime-vm/desktop-04-reboot.png` and the guest's
`vm-apps.json` and `workspace-reboot.json`. This VM release has not had a host
hard reset, and `Companion-ready-v2` still contains the pre-dock release.

Driver references: Alpine recommends [fbdev for Hyper-V guests](https://wiki.alpinelinux.org/wiki/Xf86_Video).
The independent console uses Microsoft's [Hyper-V thumbnail API](https://learn.microsoft.com/en-us/windows/win32/hyperv_v2/getvirtualsystemthumbnailimage-msvm-virtualsystemmanagementservice).

## Historical C shell and terminal foundation

The owner set C as the language for new Companion-owned guest runtime. Alpine
remains the kernel and Unix base. The first versioned native release is
`/opt/companion/native/releases/c-base-0.4` on CompanionDev, selected by
`/opt/companion/native/current`. `companion-sh` and `companion-terminal` are
available through `/usr/local/bin`. The current shell is 32,704 bytes and the
terminal 29,544 bytes; libvterm adds 100 KiB installed. These are
dynamic binaries that also use existing X11/Xft libraries. Temporary compiler
and header packages were removed after the build.

`userspace/native/Makefile` builds both C sources. `tests/native_shell.sh` covers
quoting, expansion, pipes, redirection, status and errors. The live
`tests/native_terminal_vm.sh` opened the terminal as UID 1000 and checked a
command through the PTY, file ownership, pipeline output, terminal resize and
Ctrl+C interruption in a running command and at the idle prompt.
`capture --require-ui` accepted the actual framebuffer;
evidence is `build/prime-vm/native-c-terminal-04.png`. The C terminal opens in the
desktop user's home and the shell shows a clean prompt after Ctrl+C. The control
and desktop services remained started after test and after the temporary build
toolchain was removed. A pre-change host checkpoint is
`Companion-c-base-before-terminal`, UUID
`36b680fa-4210-4060-b314-cf900511aad5`.

A normal VM reboot changed boot ID from
`0005192e-cbcc-4740-8f0c-292167230274` to
`3436af74-ab3f-4239-8cbe-2c1e02be3f80`. SSH, watch, control, tty7 UI and
the native executable links returned. The graphical PTY test passed again after
reboot; `build/prime-vm/native-c-terminal-reboot.png` shows that frame.

This section records the earlier shell/terminal milestone. The active C runtime
now includes the dock, applications and control service; see
[native-runtime.md](native-runtime.md). The shell has bounded syntax and the
terminal still needs scrollback and clipboard. No C runtime was installed on
the Dell.

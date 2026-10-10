# Heurism C desktop on the physical Dell

On 2026-10-10 the guarded Dell installer assembled sealed release
`/opt/heurism/native/releases/heurism-os-dell-20261010T153748Z-15862` from
the VM-verified C binaries and current Heurism theme/session assets. The first
assembly exposed an obsolete Dell UI test that still clicked the old menu.
`tests/native_desktop_dell.sh` was updated to navigate the new workspace and
its Appearance, BIOS information, Network and Sound pages. The captured Sound
page showed the actual 100% left, 40% right, muted speaker state. The next
assembly passed Xfce recovery, C shell and PTY, physical Dell C control,
desktop/settings, 1920×1080 C workspace/quick controls/launcher/terminal,
Files/Editor and sealed-release corruption rejection. The candidate verified
against its hash manifest. The Dell installer then activated it with its
20-second health gate and automatic restoration on failure.

The desktop mode file now contains `heurism`. A desktop-service restart, with
no OS reboot, started the Heurism C workspace on Xorg `:0` under Xfwm4. The
release verifier returned a live UID-1000 C workspace PID on boot
`edb102d9-25c6-409c-aa17-181fea3eaf41`; the Xfce panel and desktop were
absent. The Xfce PAM screensaver's own query reported an active lock. Root
SSH, `heurism-control`, `companion-watch` and the desktop service remained
started.

An intentional C workspace termination exercised the Dell recovery route.
The root session client selected `xfce`; within eleven seconds the release
verifier reported a healthy Xfce session with its panel and desktop. The PAM
screensaver again reported active. The mode was then set back to `heurism`
and a desktop-service restart returned the healthy C session.

Before and after assembly and activation, brightness stayed at `96000`,
FnLock stayed `Enabled`, speaker volume stayed left `65536`/100% and right
`26090`/40%, and the sink stayed muted. The boot ID did not change. The eight
protected hashes passed, `BootCurrent` remained `0005`, and `BootOrder`
remained `0005,0000`. Ethernet and the pinned root SSH link stayed available.

This proves a live C desktop process, lock and recovery path on the Dell.
The lid was closed, so direct viewing of the physical display and human
touch/keyboard interaction were not observed. Xrandr reports eDP-1 connected
and the live Xorg screen at 1920×1080; that is display configuration evidence,
not physical viewing. A boot into this exact release
and mode has not yet been tested; a remote reboot is intentionally deferred
because an earlier Dell firmware-logo stall required a physical power cycle.
The shared UID-1000 X11 session and unencrypted root disk remain material
security limits. Heurism is visually distinct in the captured Dell-size X11
frames, while Linux/Xorg, Xfwm4 and the PAM locker remain upstream components.

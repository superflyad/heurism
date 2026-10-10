# Heurism workspace visual pass

Heurism keeps Xfce 4.20 for proven window, file and application behavior while
giving the workspace its own visual identity. The first pass is versioned with
the C release, so a failed release can roll back the scripts and assets together.

The workspace now has a Heurism H mark and name in the application menu, a
navy top panel, a larger dark dock, a vector wallpaper with a copper and teal
orbit motif, and an uncluttered desktop. Files and applications remain in the
dock and menu. The look script applies panel and desktop-icon defaults once;
later user panel choices remain. It updates the wallpaper on Heurism release
changes but leaves an unrelated user wallpaper alone. The artwork is SVG and
adds no image service or runtime dependency.

`userspace/native/heurism-look.sh`, `heurism-mark.svg` and
`heurism-wallpaper.svg` are included in the sealed release manifest. All three
installers and the fresh image builder carry them. The C verifier requires
their hashes. The image source has the same wallpaper bytes; its staged digest
is pinned in `tools/build-hyperv-guest.sh`.

## VM evidence, 2026-10-09

The `CompanionDev` checkpoint before this work is
`Heurism-look-baseline-20261009` (UUID
`496118fc-a694-4f32-9650-d68b4cb49f69`). The guarded installer assembled
and activated release
`/opt/heurism/native/releases/heurism-os-20261009T235647Z-205314` after
isolated Xfce windows, C Settings/Power, shell, interactive PTY/job control,
control socket and release checks. A first screenshot exposed clipped artwork
at 1280×800; the text position was corrected and the final capture showed the
full label and dock. Checked C reboot returned fresh boot
`124b1b46-a472-49d7-b7ff-9a4696254888` with the same release, root
SSH/watch/control, UID-1000 Xfce and painted wallpaper. A subsequent desktop
service restart retained healthy release status.

Repeated Xvfb sidecar tests had left UID-1000 D-Bus, GVFS and ssh-agent
processes alive. The sidecar now stops only processes carrying its unique
runtime directory. The real X session now tags its descendants with a random
session token and stops them on session exit. On the fresh VM boot, two user
D-Bus daemons were present before and after a desktop service restart; the
prior long-running VM had 34 before these cleanup changes. `free -m` reported
524 MiB used immediately after the fresh boot and 532 MiB after the desktop
restart, versus 615 MiB on the old long-running boot. These are different
uptimes and cache states, so they show cleanup behavior, not a controlled
speed or memory benchmark. Temporary compiler packages were removed.

## Dell deployment, 2026-10-09

The VM-tested look and C session cleanup were assembled and activated through
the guarded Dell installer as sealed release
`/opt/heurism/native/releases/heurism-os-dell-20261010T002244Z-12355`.
The Dell's existing shell and terminal binaries were hash-matched and retained;
only the tested look, session, menu and release-verifier sources were staged.
Host and Dell hashes of all nine staged inputs matched. The installer passed
isolated Xfce windows, applications, C Settings and Power, shell and interactive
shell, native control, Files, Editor, release verification and a deliberately
corrupted clone rejection before activation. It kept the previous working
release available for rollback.

The live 1920×1080 Dell capture at
`build/desktop/dell-heurism-look-20261009.png` shows the H mark, wallpaper,
menu, panel and dock. The release verifier and C health endpoint passed with
UID 1000 Xfce running. SSH, watch, control and desktop services stayed up;
Ethernet, AC power, sound and input preferences remained present. The checked
power endpoint and protected SSD/NVRAM verification passed. The boot ID stayed
`9668135f-ca95-4be3-a0d9-78a6fc9a78a7`: this was a desktop-service
activation, not an OS reboot. Dell boot persistence has not been tested for
this release. The remote-power guard remains active because a prior Dell
restart stalled at its firmware logo without an independent recovery path.

The on-disk `/opt/heurism/native/current` symlink resolves to this release.
OpenRC's default runlevel enables `heurism-control` and `heurism-desktop`, whose
installed init script starts `current/session.sh`. Xfce saved its wallpaper
and panel XML in the UID-1000 home directory. A later `heurism-desktop` service
restart returned a new healthy Xfce session on the same release; the wallpaper,
menu, panel and dock were painted again in
`build/desktop/dell-heurism-persisted-20261009.png`. This verifies persistence
across a user-session restart. A full Dell boot remains untested for this
release because the firmware-logo stall has no remote recovery path.

This is the first workspace pass. Application windows and much of the icon
set still use Xfce themes. The next visual work is a consistent Heurism
window, notification, launcher and settings language, tested against file,
browser, terminal, touch and accessibility tasks. Measure fresh boot-to-ready,
idle memory, app launch and responsiveness on identical VM configurations
before claiming a performance gain.

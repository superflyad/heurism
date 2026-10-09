# Heurism C userspace

Heurism uses Alpine Linux for the kernel, drivers, init and standard Unix
programs. Heurism owned guest runtime programs are C executables with narrow
interfaces. POSIX shell scripts connect boot and release steps. The Python
`tools/prime-vm.py` program runs on the development host, outside the active
VM desktop and control services.

| Component | Boundary |
| --- | --- |
| `heurism-sh` | User command shell; launches ordinary Unix programs. |
| `heurism-terminal` | X11/Xft/libvterm terminal; owns a PTY and runs `heurism-sh` as its caller. |
| `heurism-desktop` | Original X11 workspace and dock, now a selectable native session; `--settings` opens a regular Xfce window for platform controls. It runs as `companion-ui` and calls the local control socket. |
| `heurism-app` | Shared Files and Editor executable, installed with two hardlinks. It reads and writes as `companion-ui`. |
| `heurism-control` | Root Unix socket service. Uses peer credentials; supports status, appearance, Dell hardware controls, platform-specific boot checks and checked power. It denies the former administrator-console action. |
| `heurismctl` | Small C client for the local control socket. |
| `heurism-session-config` | Root Xorg configuration from actual input devices on the verified VM or Dell profile. |
| `heurism-release` | Verifies sealed release hashes and live UID-1000 UI health. |

The default desktop session is Alpine Xfce 4.20. Its panel, window manager,
desktop, Thunar file manager and Mousepad editor provide the established daily
desktop. Heurism Settings and the C terminal have Xfce application launchers.
The top-right red power icon and Applications > System > Heurism Power open
the C Power window for checked Restart and Shut down, each requiring a second
click within ten seconds. The disabled Xfce Log Out menu entry is replaced in
the user session.
The optional original C workspace can be selected by placing `native` in
`/etc/companion/native-session-mode` and restarting the currently named
`companion-desktop` OpenRC service; removing
that file selects Xfce again. The C control socket and authenticated root SSH
stay separate from either user session. Xfce power manager autostart is hidden
so its controls do not bypass Heurism's checked power path. See
[xfce-bridge.md](../../docs/xfce-bridge.md).

The active VM release is selected by `/opt/heurism/native/current` and is
`/opt/heurism/native/releases/heurism-os-20261009T011559Z-4876`.
`install-native-vm.sh assemble` builds a versioned, hashed candidate;
`activate` replaces the VM's OpenRC desktop and control scripts, waits for C UI
health, and restores the previous scripts and release on failure. The native
session can fall back to the sealed legacy UI if its own X startup fails. Root
SSH and `companion-watch` run independently of the desktop. Do not deploy this
VM installer on the Dell. The Dell has its own exact-DMI installer,
`install-native-dell.sh`, and the active sealed release
`/opt/heurism/native/releases/heurism-os-dell-20261009T011703Z-5716`. The Dell
installer's `assemble` action does not switch services. `activate` changes
only the desktop/control OpenRC scripts and native release symlink, with a
20-second health gate and automatic restoration of the previous scripts and
release if that gate fails. The legacy runtime remains sealed for recovery;
SSH and `companion-watch` remain independent. Ethernet is the active Dell
management link. Wi-Fi association is deferred at the owner's direction.
The verified boot loader, OpenRC service names, `companion-ui` account,
management hostnames, protected manifests and sealed Python recovery retain
their installed identifiers during this migration. See
[heurism-migration.md](../../docs/heurism-migration.md).

Build in the VM with `make` and Alpine's `build-base`, `libxft-dev`,
`libvterm-dev`, `json-c-dev`, and `openssl-dev`. The C control service uses
OpenSSL PBKDF2 to derive a WPA key without putting the password in a command
argument or configuration file. Temporary build headers can be removed
after sealing. See [native-runtime.md](../../docs/native-runtime.md) for test
evidence and remaining work.

The shell has cursor editing, session-only command history and unquoted whole-word
pathname globbing. It is smaller than a POSIX script shell: it lacks job control,
command substitution, functions and completion. BusyBox ash remains
for startup and recovery scripts. The terminal still lacks scrollback,
clipboard, mouse reporting and accessibility support. Firefox and Onboard
remain external Unix applications. Administrator access uses authenticated root
SSH; it no longer opens a root terminal in the shared X11 session. See
[security.md](../../docs/security.md) for the current security boundary.

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
| `heurism-desktop` | C workspace, launcher, task dock and platform settings; it runs as `companion-ui` and calls the local control socket. |
| `heurism-app` | Shared Files and Editor executable, installed with two hardlinks. It reads and writes as `companion-ui`. |
| `heurism-control` | Root Unix socket service. Uses peer credentials; supports status, appearance, Dell hardware controls, platform-specific boot checks and checked power. It denies the former administrator-console action. |
| `heurismctl` | Small C client for the local control socket. |
| `heurism-session-config` | Root Xorg configuration from actual input devices on the verified VM or Dell profile. |
| `heurism-release` | Verifies sealed release hashes and live UID-1000 UI health. |
| `heurism-slot` | Fresh Hyper-V image only: verifies the inactive root, queues a one-time B boot, and renews B only after boot health. See [A/B updates](../../docs/ab-updates.md). |

The development VM now selects `heurism` in `/etc/companion/native-session-mode`:
the visible workspace, searchable launcher, task dock, Files, Editor, terminal and shell
are Heurism C programs. Xfwm4 still manages windows, Xfce's PAM screensaver
locks the session, and Alpine Linux supplies the kernel and Unix services.
The dock button and Super+Space open the same launcher from the VM workspace.
It searches built-in actions, visible installed applications and current open
windows; selecting a window returns focus to it. The 0.8 workspace has a
persistent status panel, compact task dock and search palette instead of the
old application and system dashboard. See
[the modern workspace record](../../docs/heurism-modern-workspace.md).
The dock's connection button opens C quick controls for real network status,
appearance, Settings, Network, Lock and checked Power. See
[the quick-controls evidence](../../docs/heurism-quick-controls.md).
The Dell now defaults to the Heurism C workspace with Xfwm4 window management
and an active Xfce PAM screen lock. Alpine Xfce 4.20 with Thunar and Mousepad
remains the automatic recovery session. In recovery, Heurism Settings and the C
terminal have Xfce application launchers. See
[the Dell live-session evidence](../../docs/heurism-dell-live-session.md) and
[the isolated workspace gate](../../docs/heurism-dell-workspace-gate.md).
The top-right red power icon and Applications > System > Heurism Power open
the C Power window for checked Restart and Shut down, each requiring a second
click within ten seconds. The disabled Xfce Log Out menu entry is replaced in
the user session.
The explicit `heurism` mode requires a working password lock before it publishes
health. A failed C workspace or locker makes the root session client select
`xfce` for the next supervised start. The old unlocked `native` mode remains
blocked when `/etc/heurism/desktop-lock` is present. Removing the mode file
also selects Xfce. The C control socket and authenticated root SSH stay
separate from either user session. See [VM shell evidence](../../docs/heurism-shell-vm.md)
and [Xfce recovery](../../docs/xfce-bridge.md).

The active development VM release is selected by `/opt/heurism/native/current`
and is `/opt/heurism/native/releases/heurism-os-20261010T184600Z-87843`.
See [the launcher evidence](../../docs/heurism-launcher.md).
The separate fresh-image candidate uses `install-image-vm.sh` during image
construction. It seals the compiled C binaries before first boot, installs
the active C services and provides the Xfce-to-C-workspace startup fallback.
Its latest verified release is
`/opt/heurism/native/releases/heurism-os-image-20261010T035847Z` on the
isolated `HeurismCandidate` VM; it has not replaced the existing development
VM or Dell release. See [system base](../../docs/heurism-system-base.md).
`install-native-vm.sh assemble` builds a versioned, hashed candidate;
`activate` replaces the VM's OpenRC desktop and control scripts, waits for C UI
health, and restores the previous scripts and release on failure. The native
session can fall back to the sealed legacy UI if its own X startup fails. Root
SSH and `companion-watch` run independently of the desktop. Do not deploy this
VM installer on the Dell. The Dell has its own exact-DMI installer,
`install-native-dell.sh`, and the active sealed release
`/opt/heurism/native/releases/heurism-os-dell-20261010T184528Z-30288`. It
adds the window-first C workspace, launcher, quick controls and refined Heurism
window theme and Editor clipboard to the earlier shell and terminal. This release was activated
without an OS reboot. See [the Dell live-session record](../../docs/heurism-dell-live-session.md),
[the Editor clipboard evidence](../../docs/heurism-modern-workspace.md),
[the shell completion record](../../docs/shell-completion.md) and
[the earlier Dell restart record](../../docs/dell-terminal-20261009.md). The Dell
installer's `assemble` action does not switch services. `activate` changes
only the desktop/control OpenRC scripts and native release symlink, with a
20-second health gate and automatic restoration of the previous scripts and
release if that gate fails. The legacy runtime remains sealed for recovery;
SSH and `companion-watch` remain independent. Ethernet is the active Dell
management link. Wi-Fi association is deferred at the owner's direction.
The verified boot loader, `companion-watch`, `companion-ui` account,
protected manifests and sealed Python recovery retain their installed
identifiers. The active desktop/control OpenRC services and hostnames now use
Heurism. The root staging directory is mode `0700`, and installers reject
writable staged inputs. See
[heurism-migration.md](../../docs/heurism-migration.md).

Build in the VM with `make` and Alpine's `build-base`, `libxft-dev`,
`libvterm-dev`, `json-c-dev`, `glib-dev`, and `openssl-dev`. The C control service uses
OpenSSL PBKDF2 to derive a WPA key without putting the password in a command
argument or configuration file. Temporary build headers can be removed
after sealing. See [native-runtime.md](../../docs/native-runtime.md) for test
evidence and remaining work.

The shell has cursor editing, session-only command history, unquoted whole-word
pathname globbing, [bounded Tab completion](../../docs/shell-completion.md) and
[interactive job control](../../docs/shell-job-control.md). It is smaller than
a POSIX script shell: it lacks command substitution, functions and quoted
completion. BusyBox ash remains for startup and recovery scripts. The terminal retains up to
512 scrolled lines and supports Shift+PageUp/PageDown and mouse-wheel history
navigation. The current VM release also shows the scrollback position in the
window title and has [X11 text selection and clipboard](../../docs/terminal-clipboard.md).
The Dell now runs the same sealed terminal binary. Live PTY completion and
cross-app clipboard paste passed there; selection/copy remains a physical
acceptance item.
Editor 0.5 supports keyboard and pointer selection, cut/copy/paste through X11
CLIPBOARD, PRIMARY selection and middle-click paste. The isolated VM gate
verified transfers between two Editors and an external X11 clipboard owner,
including 320 KiB text. The Dell isolated app gate passed; live physical touch
and clipboard acceptance on the Dell remain unobserved.
The VM includes a WenQuanYi fallback for tested CJK glyphs. Broader Unicode
coverage, wide-character alignment, application mouse reporting and
accessibility remain open.
Firefox and Onboard remain external Unix applications. Administrator access uses authenticated root
SSH; it no longer opens a root terminal in the shared X11 session. See
[security.md](../../docs/security.md) for the current security boundary.

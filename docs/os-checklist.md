# Heurism OS checklist

This is the working product checklist for the Dell Inspiron 7506 and its
separate Hyper-V proving ground. Heurism uses Linux and selected Alpine
components as its foundation; Heurism-owned shell, terminal, control,
settings and release verification are C. Keep the established Xfce desktop
until a replacement meets the same daily-use tests. The target is a dependable
daily computer on the Dell.

**Completion rule:** a box is checked only when the named behavior has passed
on the stated target. A VM pass does not stand in for Dell hardware evidence.
For each change, record the exact source revision, installed release, boot ID,
test result and rollback path. See the [architecture](architecture.md),
[system base](heurism-system-base.md), [security status](security.md) and
[product roadmap](roadmap.md) for the current evidence.

## What already works

- [x] Linux, OpenRC, root SSH/watch and SSD rescue remain independent of the
  desktop on the Dell; verified checked reboots preserved management.
- [x] Xfce provides a panel, workspace, file manager, editor and browser;
  Heurism C Settings and checked Restart/Shut down have live Dell evidence.
- [x] The C shell, PTY terminal, control socket and sealed release verifier
  exist. Bounded scrollback passed a live Dell session before a checked reboot,
  but [post-reboot Dell health is unverified](dell-terminal-20261009.md).
  The later scrollback title cue is VM only.
  [Selection, clipboard and clean close](terminal-clipboard.md) are also VM only.
- [x] A fresh C-first VM image booted, painted the desktop, passed fallback and
  checked power tests. The Dell's last known release used its guarded installer
  and existing SSD boot path; its present boot state remains unverified.
- [x] The owner reports that Dell touch and speakers work. Detailed gestures,
  microphone behavior and audio quality have not been accepted by hand.

## P0 — Make the Dell dependable for daily work

Use the [Dell daily-use pass](dell-daily-use-pass.md) to record physical results.

- [ ] **Run an end-to-end human session.** From a cold boot, use only the Dell
  display and input to open Files, create/edit/save/reopen documents, move a
  file to Trash and restore it, browse and download, use Settings and Power,
  and work in the terminal. Record every confusing or failed step.
- [ ] **Finish the C terminal.** Confirm the Dell returns healthy after the
  [scrollback release reboot](dell-terminal-20261009.md). Promote the VM-tested
  selection and clipboard safely, then complete large-paste behavior, Unicode
  display and wide-character checks, alternate-screen behavior and a visible
  scrollback cue. Verify real
  PTY input, resize, Ctrl+C and window close after a reboot.
- [ ] **Define the C shell contract.** Decide explicitly what is interactive
  Heurism shell behavior and what stays with BusyBox ash for scripts. Add
  foreground/background jobs, terminal process groups and signals, completion,
  quoting/globbing consistency, environment handling and useful error status.
  Test with real PTYs, pipelines and interrupted jobs.
- [ ] **Make ordinary desktop tasks coherent.** Applications, file types,
  default opener, browser downloads, drag-and-drop, notifications, search,
  keyboard shortcuts and window switching should work predictably together.
  Keep Xfce where it performs well; replace a component only after its
  replacement passes the same tasks.
- [ ] **Provide a real local session boundary.** Establish login, lock/unlock,
  idle screen behavior and user switching or an explicit single-user policy.
  Recovery and root SSH must remain available when the display session fails.
- [ ] **Make settings trustworthy.** Every visible Sound, Input, Network,
  Display, Power and supported BIOS control must show actual state, explain
  unavailable operations and preserve successful changes across restart.
  Reject unsupported hardware instead of displaying a simulated control.
- [ ] **Complete physical input acceptance.** Test keyboard, touchpad click/tap,
  scrolling, pointer speed, touchscreen gestures, on-screen keyboard, tablet
  posture and external mouse with a person at the Dell; verify saved settings
  after a cold boot.
- [ ] **Complete physical audio and display acceptance.** Check speaker volume,
  mute, headphones, microphone capture, browser playback, brightness, display
  scaling, rotation and an external display where supported. Measure behavior
  on both AC and battery.
- [ ] **Finish power behavior.** Test Restart, Shut down, suspend/resume,
  lid-closed behavior, low-battery action and recovery from an interrupted
  sleep transition on the Dell. Preserve the proven SSD boot entries and
  independent SSH/watch paths. The proven local-LAN `s2idle` wake does not
  count as wake from full poweroff.
- [ ] **Provide routine app maintenance.** A user can see installed software,
  add/remove a supported application, update it, and understand when a restart
  is required. Root operations are deliberate and failures are recoverable.

## P0 — Prevent data loss and failed boots

- [ ] **Specify one internal system update transaction.** Pin the input package
  set and C release, verify all inputs, stage changes away from the running
  system, then activate them together. Record exactly what is installed.
- [ ] **Prove whole-system rollback.** Failed package, kernel, service and UI
  updates must return to the previous bootable system in the VM. On the Dell,
  promote only after a guarded rehearsal that preserves SSD rescue and root
  management. C release rollback alone is insufficient.
- [ ] **Separate system state from user data.** A rollback restores OS code and
  configuration without discarding documents or silently reverting user work.
  Define migrations for settings and a way to undo failed migrations.
- [ ] **Provide backup and restore.** Back up user files, preferences and
  recovery material to another device; restore them on a clean installation
  and verify hashes and permissions. A VM checkpoint is not a Dell backup.
- [ ] **Exercise failure paths.** In the VM, test power interruption during an
  update, full disk, missing package repository, failed desktop startup,
  broken network and corrupted candidate manifest. Confirm a bounded recovery
  route and no loss of the previous bootable system.
- [ ] **Make boot and service health visible.** Show boot ID, active system/C
  release, failed services, disk health, network state and recent errors in a
  simple local diagnostic view, with equivalent authenticated SSH commands.

## P1 — Make security a real system property

- [ ] **Write a threat model for this Dell.** Cover physical theft, malicious
  downloads, untrusted desktop apps, compromised UID 1000, remote SSH,
  firmware changes and update inputs. State what Heurism currently protects.
- [ ] **Reduce desktop privilege.** Split sensitive hardware/network/BIOS
  operations from the shared desktop account. Require explicit administrator
  authentication for high-impact actions and verify that a compromised user
  application cannot invoke them silently.
- [ ] **Establish application isolation.** The current shared X11/UID-1000
  session cannot isolate untrusted apps. Prototype a desktop/session model in
  the VM and demonstrate that one test app cannot read another window, inject
  its input or read unrelated user files before claiming such isolation.
- [ ] **Plan and prove disk encryption.** Design encryption and key recovery
  around the existing SSD boot/rescue and closed-lid management constraints.
  Restore a real backup before any Dell conversion; measure cold boot,
  unlock, recovery and lost-key behavior in an isolated target first.
- [ ] **Protect the boot and update chain.** Define which firmware, EFI,
  kernel, package and C artifacts are authenticated, where trust starts, how
  keys rotate, and how rescue works if verification fails. File hashes alone
  are point-in-time checks, not a complete trust chain.
- [ ] **Maintain a minimal network surface.** Audit listeners and outbound
  services after every change, retain key-only SSH, set a host firewall policy
  and exercise credential rotation/revocation. Add Wi-Fi association only when
  it can be tested without sacrificing the sole Ethernet management link.
- [ ] **Create a security maintenance routine.** Track upstream kernel,
  Alpine, Xfce, browser and C dependencies; apply fixes through the tested
  update path. Keep compiler hardening, static checks and focused fuzzing for
  parsers, shell input, terminal escapes and the control protocol.
- [ ] **Make privacy explicit.** Document stored logs, diagnostics and network
  contacts. Give the user a way to inspect and clear personal history without
  erasing needed recovery records.

## P1 — Support the actual Dell hardware

- [ ] **Keep a physical capability matrix.** For Ethernet, Wi-Fi, Bluetooth,
  camera, microphone, speakers, touch, touchpad, battery, AC, USB, external
  display and supported BIOS attributes, record detected device, driver,
  tested action, observed result and unsupported state. Do not infer a Dell
  result from VM emulation.
- [ ] **Measure battery and thermals.** Record idle and active power use,
  suspend drain, temperature, fan behavior and throttling before setting
  optimization targets. Verify that power controls do not strand management.
- [ ] **Make peripherals predictable.** Test USB storage insertion/removal,
  file permissions, external keyboard/mouse, headset and display hotplug.
  Unplugging the sole Ethernet adapter must not be part of a remote test.
- [ ] **Define firmware-operation boundaries.** Keep current SSD boot and
  recovery order, use only verified Dell BIOS attributes, and require a
  separately proven recovery route before any NVRAM or boot-chain change.

## P2 — Make Heurism feel like one OS

- [ ] **Define a compact interface language.** Consistent naming, typography,
  icons, spacing, colors and error wording across shell, terminal, Settings,
  Power, startup and recovery. Avoid visual changes that hide real state.
- [ ] **Give the desktop a coherent workflow.** Design the dock/workspace,
  launcher, search, task switching, quick settings and notifications around
  common tasks; compare each change against the current Xfce workflow with a
  human user before replacing it.
- [ ] **Meet accessibility basics.** Keyboard-only operation, visible focus,
  readable scaling/contrast, screen-reader compatibility, touch targets and
  an on-screen keyboard must work through login, desktop, settings and power.
- [ ] **Make files and settings interoperable.** Follow desktop-entry, MIME
  association and XDG config/state conventions so upstream apps and Heurism C
  tools agree on launchers, file openers and persistent data locations.
- [ ] **Set measured quality budgets.** On the Dell, establish repeatable
  baselines for boot-to-usable-desktop, idle RAM/CPU, app launch, disk use,
  battery life and crash recovery. Set targets from those measurements, then
  reject regressions that users can feel.
- [ ] **Make errors actionable.** A failed network join, update, power action
  or app launch should say what failed, what is still safe, and the next
  recovery action. Keep diagnostic output available after a reboot.

## Working order

1. Perform the human Dell daily-work session and record the first real blockers.
2. Finish shell/terminal interaction and promote those C improvements through
   the guarded VM-to-Dell path.
3. Build and failure-test whole-system updates, rollback and user-data restore.
4. Reduce the shared desktop privilege boundary and prove an encryption plan.
5. Complete hardware/power acceptance, then refine the Heurism interface.

Use the [XDG base-directory](https://specifications.freedesktop.org/basedir/0.8/),
[desktop-entry](https://specifications.freedesktop.org/desktop-entry/latest/)
and [MIME association](https://specifications.freedesktop.org/mime-apps/latest-single/)
specifications for desktop compatibility. Use Alpine's
[upgrade guidance](https://wiki.alpinelinux.org/wiki/Upgrading_Alpine) for
package-transition planning and the Linux kernel's
[dm-crypt documentation](https://www.kernel.org/doc/html/latest/admin-guide/device-mapper/dm-crypt.html)
when designing encrypted storage. These references guide implementation; the
completion boxes still require Heurism VM and Dell evidence.

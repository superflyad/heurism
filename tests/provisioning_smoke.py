"""Boot the provisioning USB in UEFI and verify authenticated root access.

Optional --install tests the installer ONLY on a newly created virtual disk,
then boots that disk without the USB to verify persistent access.
"""
import json
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import time

repo = Path(__file__).resolve().parents[1]
runtime = repo / 'build' / 'tools' / 'qemu'
output = repo / 'build' / 'provisioning' / 'smoke'
output.mkdir(parents=True, exist_ok=True)
payload = repo / 'build' / 'provisioning' / 'payload'
usb_image = repo / 'build' / 'provisioning' / 'companion-usb.img'
if not usb_image.exists():
    subprocess.run([sys.executable, str(repo / 'tools' / 'make-fat-image.py'), str(payload), str(usb_image)], check=True)
keys = repo / 'artifacts' / 'ssh'
install = '--install' in sys.argv
shutil.copyfile(runtime / 'share' / 'edk2-i386-vars.fd', output / 'vars.fd')
ports = []
for _ in range(2):
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0)); ports.append(reservation.getsockname()[1])
qmp_port, ssh_port = ports
blank = output / ('install-' + str(int(time.time())) + '.qcow2')
subprocess.run([str(runtime / 'qemu-img.exe'), 'create', '-f', 'qcow2', str(blank), '12G'], check=True,
               capture_output=True, creationflags=0x08000000)
base = [str(runtime / 'qemu-system-x86_64.exe'), '-machine', 'q35', '-accel', 'tcg',
        '-m', '3072M', '-smp', '2', '-display', 'none',
        '-drive', f'if=pflash,format=raw,readonly=on,file={runtime / "share" / "edk2-x86_64-code.fd"}',
        '-drive', f'if=pflash,format=raw,file={output / "vars.fd"}',
        '-drive', f'if=none,id=system,format=qcow2,file={blank}',
        '-device', 'virtio-blk-pci,drive=system,addr=0x4,bootindex=2',
        '-device', 'qemu-xhci,addr=0x6',
        '-netdev', f'user,id=net,hostfwd=tcp:127.0.0.1:{ssh_port}-:22', '-device', 'e1000,netdev=net',
        '-qmp', f'tcp:127.0.0.1:{qmp_port},server=on,wait=off',
        '-serial', f'file:{output / "serial.log"}']
usb = ['-drive', f'if=none,id=stick,format=raw,file={usb_image}',
       '-device', 'usb-storage,drive=stick,bootindex=1']
ssh_base = ['ssh', '-i', str(keys / 'companion_client_ed25519'), '-p', str(ssh_port),
            '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=3', '-o', 'StrictHostKeyChecking=yes',
            '-o', f'UserKnownHostsFile={keys / "known_hosts"}', '-o', 'HostKeyAlias=companion-dell',
            'root@127.0.0.1']

def remote(command, timeout=20):
    return subprocess.run(ssh_base + [command], capture_output=True, text=True, timeout=timeout,
                          creationflags=0x08000000)

def wait_ssh(process, seconds=180):
    deadline = time.monotonic() + seconds
    last = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError((output / 'qemu.log').read_text())
        last = remote('id -u; cat /etc/hostname', timeout=8)
        if last.returncode == 0 and '0\ncompanion-dell' in last.stdout:
            return
        time.sleep(2)
    raise RuntimeError('No trusted root SSH: ' + (last.stderr if last else '')
                       + '\nInspect ' + str(output / 'serial.log'))

def launch(args):
    log = (output / 'qemu.log').open('w')
    process = subprocess.Popen(args, stdout=log, stderr=log, creationflags=0x08000000)
    return process, log

def check_console_login():
    # tty0 aliases the active virtual terminal. A second getty there races tty1
    # for keyboard input even though key-authenticated SSH still works.
    result = remote("! grep -q '^tty0::respawn:' /etc/inittab && "
                    "test $(awk -F: '$1 == \"root\" && $2 == \"\" {print 1}' /etc/shadow) = 1")
    assert result.returncode == 0, 'Console configuration or empty root password is incorrect'
    # Test actual emulated keyboard input through the standard tty1 getty.
    deadline = time.monotonic() + 30
    while remote('rc-service companion status').returncode != 0:
        assert time.monotonic() < deadline, 'Companion startup did not finish before console login'
        time.sleep(1)
    time.sleep(2)
    with socket.create_connection(('127.0.0.1', qmp_port), timeout=10) as connection:
        stream = connection.makefile('rwb')
        stream.readline()
        def qmp(command, arguments=None):
            request = {'execute': command}
            if arguments is not None:
                request['arguments'] = arguments
            stream.write(json.dumps(request).encode() + b'\n'); stream.flush()
            while True:
                reply = json.loads(stream.readline())
                if 'error' in reply:
                    raise RuntimeError(reply)
                if 'return' in reply:
                    return reply['return']
        qmp('qmp_capabilities')
        qmp('human-monitor-command', {'command-line': 'sendkey ctrl-alt-f1'})
        time.sleep(1)
        def type_keys(value):
            for character in value:
                key = {' ': 'spc', '/': 'slash', '\n': 'ret'}.get(character, character)
                qmp('human-monitor-command', {'command-line': 'sendkey ' + key})
                time.sleep(0.12)
        type_keys('root\n')
        time.sleep(2)
        type_keys('touch /run/consoleproof\n')
        time.sleep(1)
        result = remote('test -f /run/consoleproof && test $(stat -c %u /run/consoleproof) = 0')
        assert result.returncode == 0, 'Keyboard login as root did not reach a working local shell'
        type_keys('exit\n')
    print('PASS: actual keyboard login on tty1 reaches a root shell without a password', flush=True)

def check_access_recovery(process):
    assert remote('rc-service companion-watch status').returncode == 0
    # A dead DHCP client must be recreated, not merely have a stale pid file.
    previous = remote('cat /run/companion/dhcp-eth0.pid').stdout.strip()
    assert previous.isdigit(), 'No DHCP client pid'
    remote('kill ' + previous)
    time.sleep(12)
    wait_ssh(process, seconds=45)
    result = remote('pid=$(cat /run/companion/dhcp-eth0.pid); kill -0 "$pid"; echo "$pid"')
    assert result.returncode == 0 and result.stdout.strip() != previous
    # Stopping the listener keeps this session alive; the guard must start a
    # listener for a new authenticated connection without manual intervention.
    remote('rc-service sshd stop')
    time.sleep(12)
    wait_ssh(process, seconds=45)
    assert remote('rc-service sshd status; rc-service companion-watch status').returncode == 0
    print('PASS: DHCP-client failure and stopped SSH listener recover automatically', flush=True)

process, log = launch(base + usb)
try:
    wait_ssh(process)
    print('PASS: provisioning booted via UEFI virtual USB; trusted SSH gives root access', flush=True)
    check_console_login()
    check_access_recovery(process)
    result = remote('companion-status; companion-audit; for f in /etc/companion/bin/*; do sh -n "$f" || exit 1; done; companion-install /dev/vda; companion-install /dev/sda --erase /dev/sda')
    (output / 'checks.txt').write_text(result.stdout + result.stderr)
    assert result.returncode != 0 and ('Refusing to install to a USB disk' in result.stderr
                                      or 'mounted filesystems' in result.stderr), result.stdout + result.stderr
    assert 'Preview only. Nothing changed.' in result.stdout
    print('PASS: hardware audit, script syntax, disk preview and refusal to erase boot USB', flush=True)
    if install:
        result = remote('companion-install /dev/vda --erase /dev/vda', timeout=240)
        (output / 'install.txt').write_text(result.stdout + result.stderr)
        assert result.returncode == 0, 'Virtual installation failed; inspect install.txt'
        print('PASS: installed persistent management system onto the isolated blank virtual disk', flush=True)
finally:
    process.terminate(); process.wait(timeout=15); log.close()

if install:
    process, log = launch(base)
    try:
        wait_ssh(process)
        check_console_login()
        check_access_recovery(process)
        result = remote('findmnt /; command -v companion-status; test -f /etc/companion/bin/companion-install; cat /etc/os-release')
        (output / 'installed-boot.txt').write_text(result.stdout + result.stderr)
        assert result.returncode == 0 and 'ext4' in result.stdout, result.stdout + result.stderr
        print('PASS: boots from installed disk without USB and retains trusted root SSH and management tools', flush=True)
    finally:
        process.terminate(); process.wait(timeout=15); log.close()

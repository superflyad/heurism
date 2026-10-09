"""Exercise the exact staged EFI rescue loader without a main root disk."""
from pathlib import Path
import argparse
import json
import shutil
import socket
import subprocess
import time

repo = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--menu-key',action='store_true',help='Reveal the hidden menu through QMP Escape')
args=parser.parse_args()
runtime = repo / 'build/tools/qemu'
out = repo / 'build/recovery/test'
out.mkdir(parents=True, exist_ok=True)
shutil.copyfile(runtime / 'share/edk2-i386-vars.fd', out / 'vars.fd')
with socket.socket() as sock:
    sock.bind(('127.0.0.1', 0)); port = sock.getsockname()[1]
with socket.socket() as sock:
    sock.bind(('127.0.0.1', 0)); qmp_port = sock.getsockname()[1]
keys = repo / 'artifacts/ssh'
ssh = ['ssh', '-i', str(keys / 'companion_client_ed25519'), '-p', str(port),
       '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=3', '-o', 'StrictHostKeyChecking=yes',
       '-o', 'HostKeyAlias=companion-dell', '-o', 'UserKnownHostsFile=' + str(keys / 'known_hosts'),
       'root@127.0.0.1']
def remote(command, timeout=10):
    return subprocess.run(ssh + [command], capture_output=True, text=True, timeout=timeout,
                          creationflags=0x08000000)
def wait_rescue(previous=None):
    deadline = time.monotonic() + 180
    while time.monotonic() < deadline:
        assert process.poll() is None, (out / 'qemu.log').read_text()
        reply = remote('test -f /etc/companion/rescue && test "$(id -u)" = 0 && cat /proc/sys/kernel/random/boot_id')
        if reply.returncode == 0 and reply.stdout.strip() != previous:
            return reply.stdout.strip()
        time.sleep(2)
    raise RuntimeError('Rescue SSH unavailable; inspect ' + str(out / 'serial.log'))
log = (out / 'qemu.log').open('w')
process = subprocess.Popen([str(runtime / 'qemu-system-x86_64.exe'),
    '-machine', 'q35', '-accel', 'tcg', '-m', '3072M', '-smp', '2', '-display', 'none',
    '-drive', f'if=pflash,format=raw,readonly=on,file={runtime / "share/edk2-x86_64-code.fd"}',
    '-drive', f'if=pflash,format=raw,file={out / "vars.fd"}',
    '-drive', f'if=none,id=efi,format=raw,file={repo / "build/recovery/rescue-test.img"}',
    '-device', 'virtio-blk-pci,drive=efi,bootindex=1',
    '-netdev', f'user,id=net,hostfwd=tcp:127.0.0.1:{port}-:22', '-device', 'e1000,netdev=net',
    '-qmp',f'tcp:127.0.0.1:{qmp_port},server=on,wait=off',
    '-serial', f'file:{out / "serial.log"}'], stdout=log, stderr=log, creationflags=0x08000000)
connection=None
try:
    if args.menu_key:
        deadline=time.monotonic()+30
        while time.monotonic()<deadline:
            try:connection=socket.create_connection(('127.0.0.1',qmp_port),timeout=2);break
            except OSError:time.sleep(.1)
        assert connection,'QMP unavailable'
        stream=connection.makefile('rwb');assert 'QMP' in json.loads(stream.readline())
        def command(name,arguments=None):
            request={'execute':name}
            if arguments:request['arguments']=arguments
            stream.write(json.dumps(request).encode()+b'\n');stream.flush()
            while True:
                reply=json.loads(stream.readline())
                if 'error' in reply:raise RuntimeError(reply)
                if 'return' in reply:return reply['return']
        command('qmp_capabilities')
        # Wait for firmware to start the EFI loader, then send Escape within
        # the hidden timeout window. Repeated attempts are bounded and VM-only.
        deadline=time.monotonic()+40
        while time.monotonic()<deadline:
            serial=(out/'serial.log').read_text(errors='replace') if (out/'serial.log').exists() else ''
            if 'BdsDxe: starting' in serial:break
            time.sleep(.02)
        else:raise RuntimeError('EFI loader start not observed')
        deadline=time.monotonic()+5
        while time.monotonic()<deadline:
            command('human-monitor-command',{'command-line':'sendkey esc'})
            time.sleep(.08)
            if 'GNU GRUB' in (out/'serial.log').read_text(errors='replace'):break
        else:raise RuntimeError('Escape did not reveal GRUB menu')
        deadline=time.monotonic()+2
        while 'Companion internal rescue' not in (out/'serial.log').read_text(errors='replace'):
            assert time.monotonic()<deadline,'Menu header appeared but entries did not render'
            time.sleep(.02)
        command('screendump',{'filename':str(out/'revealed-menu.ppm')})
        command('human-monitor-command',{'command-line':'sendkey ret'})
    boot = wait_rescue()
    reply = remote('findmnt /; cat /etc/apk/world; rc-service companion-watch status; sshd -t')
    # SSH can precede the service that starts it finishing.
    deadline = time.monotonic() + 30
    while remote('rc-service companion-watch status').returncode:
        assert time.monotonic() < deadline
        time.sleep(1)
    assert remote('test "$(findmnt -n -o FSTYPE /)" = tmpfs; sshd -t; apk info -e alpine-base openssh grub-efi').returncode == 0
    print('PASS: exact staged EFI loader boots offline RAM rescue with trusted root SSH', flush=True)
    first_serial=(out/'serial.log').read_text(errors='replace')
    if args.menu_key:
        assert 'GNU GRUB' in first_serial
        print('PASS: Escape reveals the hidden recovery menu',flush=True)
    else:
        assert 'GNU GRUB' not in first_serial
        print('PASS: default startup hides the GNU GRUB menu',flush=True)
    # The test disk intentionally lacks /companion/stable: main cannot load.
    reply = remote('envfile=$(find /media -name grubenv | head -1); test -n "$envfile" && mount -o remount,rw "$(findmnt -n -o TARGET -T "$envfile")" && grub-editenv "$envfile" set companion_pending=0 && sync')
    assert reply.returncode == 0, reply.stdout + reply.stderr
    remote('nohup sh -c "sleep 2; reboot" >/tmp/reboot.log 2>&1 </dev/null &')
    second = wait_rescue(boot)
    assert second != boot
    serial = (out / 'serial.log').read_text(errors='replace')
    assert 'COMPANION | Starting management' in serial and 'not found' in serial, serial[-4000:]
    print('PASS: missing main boot files automatically fall back to authenticated rescue', flush=True)
    (out / ('menu-key-proof.txt' if args.menu_key else 'proof.txt')).write_text(reply.stdout + '\nHidden menu key test: '+str(args.menu_key)+'\nRescue boots: ' + boot + ' -> ' + second)
finally:
    if connection:connection.close()
    process.terminate()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill(); process.wait()
    log.close()

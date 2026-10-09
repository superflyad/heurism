"""Run the reviewed firmware inventory once, preserving normal boot order.

Only changes its ESP files and an owner-controlled UEFI BootNext/boot entry.
Does not issue firmware-volume writes, erase/program operations or SMM calls.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import shlex
from boot_safety import validate_command

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reboot', action='store_true', help='Boot the staged probe once now')
parser.add_argument('--update-probe', action='store_true', help='Inventory FMP update providers as well as volumes')
parser.add_argument('--policy-probe', action='store_true', help='Read Dell update policy protocol fields; no method calls')
parser.add_argument('--nv-probe', action='store_true', help='Create/read/load one pinned 2048-byte owner NVRAM payload; no BIOS-volume writes')
parser.add_argument('--network-probe', action='store_true', help='Read existing firmware network interfaces without starting transactions')
parser.add_argument('--http-probe', action='store_true', help='Bounded HTTP transfer of a pinned owner driver from the local controller')
parser.add_argument('--udp-probe', action='store_true', help='Bounded UDP transfer of a pinned owner driver from the local controller')
parser.add_argument('--http-inspect-probe', action='store_true', help='Inventory HTTP boot driver bindings and HII metadata; no network/configuration transactions')
parser.add_argument('--http-prereq-probe',action='store_true',help='Read firmware sections, NIC prerequisites and policy ownership without execution')
parser.add_argument('--http-init-probe',action='store_true',help='Initialize a pinned vendor HTTP boot driver, remove owned bindings and return to SSD management')
parser.add_argument('--http-ethernet-probe',action='store_true',help='MAC-selected Ethernet Supported checks; no controller Start or network transaction')
parser.add_argument('--http-cleanup-probe',action='store_true',help='Read existing NIC and child protocol open relationships; no driver load or attachment')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[1]
if args.reboot and (args.http_inspect_probe or args.http_prereq_probe or (args.http_init_probe or args.http_ethernet_probe or args.http_cleanup_probe)):
    raise SystemExit('Stage first, then use the dedicated verifier to check every recovery loader before reboot')
if sum([args.update_probe,args.policy_probe,args.nv_probe,args.network_probe,args.http_probe,args.udp_probe,args.http_inspect_probe,args.http_prereq_probe,args.http_init_probe,args.http_ethernet_probe,args.http_cleanup_probe])>1:
    raise SystemExit('Select one probe')
kind = 'http-cleanup' if args.http_cleanup_probe else 'http-ethernet' if args.http_ethernet_probe else 'http-init' if args.http_init_probe else 'http-prereq' if args.http_prereq_probe else 'http-inspect' if args.http_inspect_probe else 'udp' if args.udp_probe else 'http' if args.http_probe else 'network' if args.network_probe else 'nv' if args.nv_probe else 'policy' if args.policy_probe else 'update' if args.update_probe else 'fv'
profiles = {
    'http-cleanup': ('Companion HTTP Cleanup 01','httpcleanupx64.efi','http-cleanup-probe'),
    'http-ethernet': ('Companion HTTP Ethernet 02','httpethernet02x64.efi','http-ethernet-probe'),
    'http-init': ('Companion HTTP Init 01','httpinitx64.efi','http-init-probe'),
    'http-prereq': ('Companion HTTP Prereq 01','httpprereqx64.efi','http-prereq-probe'),
    'http-inspect': ('Companion HTTP Inspect 01','httpinspectx64.efi','http-inspect-probe'),
    'udp': ('Companion UDP Probe 01','udpprobex64.efi','udp-probe'),
    'http': ('Companion HTTP Probe 01','httpprobex64.efi','http-probe'),
    'network': ('Companion Network Probe 01','networkprobex64.efi','network-probe'),
    'nv': ('Companion NV Probe 01','nvprobex64.efi','nv-extension'),
    'policy': ('Companion Policy Probe 01','policyprobex64.efi','policy-probe'),
    'update': ('Companion Update Probe 01','updateprobex64.efi','update-probe'),
    'fv': ('Companion Firmware Probe 01','fvprobex64.efi','firmware-probe'),
}
label,efi_name,build_dir = profiles[kind]
binary = repo / ('build/'+build_dir+'/fvprobex64.efi')
deployment_scope = ('Read existing MAC-selected NIC MNP/DHCP/SNP/private protocol open relationships. '
                    'No driver load/start, attachment, network request or firmware writes; SSD management retained.'
                    if args.http_cleanup_probe else 'Initialize pinned driver and execute Supported checks on unique MAC-selected Ethernet NIC. '
                    'No controller Start/Stop or HTTP transfer; remove owned bindings, preserve SSD management.'
                    if args.http_ethernet_probe else 'Targeted pinned HttpBootDxe initialization gated by live policy code and bounded callback-list checks. '
                    'Remove only owned binding/name protocols; no explicit controller attachment or network request. '
                    'No vendor unload callback, firmware flash writes or native PXE selection. SSD management retained.'
                    if (args.http_init_probe or args.http_ethernet_probe or args.http_cleanup_probe) else
                    'Read-only FV section comparisons, same-NIC protocol inventory and policy image ownership. '
                    'No HTTP driver load/start, policy invocation, controller attachment or network transaction; SSD recovery retained.'
                    if args.http_prereq_probe else
                    'Native UEFI test may create one exact 2048-byte owner image in the nonvolatile variable store, '
                    'then loads the retrieved bytes. Bootstrap and report remain on ESP. No BIOS-volume patch, '
                    'erase, protection change or SMM invocation.' if args.nv_probe else
                    'Bounded preboot HTTP download from the local controller; only exact pinned owner bytes may execute. '
                    'SSD fallback retained; no DHCP or firmware-volume changes.' if args.http_probe else
                    'Bounded preboot UDP transfer from the local controller; only exact pinned owner bytes may execute. '
                    'SSD fallback retained; no DHCP, HTTP policy or firmware-volume changes.' if args.udp_probe else __doc__)
data = binary.read_bytes()
pe = struct.unpack_from('<I', data, 0x3c)[0]
optional = pe + 24
if data[:2] != b'MZ' or data[pe:pe+4] != b'PE\0\0' or struct.unpack_from('<H', data, pe+4)[0] != 0x8664:
    raise SystemExit('Not an x64 PE application')
if struct.unpack_from('<H', data, optional+68)[0] != 10 or struct.unpack_from('<II', data, optional+120) != (0, 0):
    raise SystemExit('Not a freestanding EFI application')
if not all(struct.unpack_from('<II', data, optional+152)):
    raise SystemExit('Missing relocations')
digest = hashlib.sha256(data).hexdigest()
if args.http_inspect_probe or args.http_prereq_probe or (args.http_init_probe or args.http_ethernet_probe or args.http_cleanup_probe):
    verification=json.loads((repo/('build/'+build_dir+'/host-verification.json')).read_text())
    if not verification['passed'] or verification['sha256']!=digest:
        raise SystemExit('Inspection host verification does not match staged image')
    subprocess.run(['python',str(repo/'tools/backup-boot-variables.py')],check=True,timeout=60)
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
options = ['-i', str(repo / 'artifacts/ssh/companion_client_ed25519'), '-o', 'BatchMode=yes',
           '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=companion-dell',
           '-o', 'UserKnownHostsFile=' + str(repo / 'artifacts/ssh/known_hosts')]
target = 'root@' + state['address']

def run(command):
    validate_command(command)
    process = subprocess.run(['ssh'] + options + [target, command], capture_output=True, text=True, timeout=30)
    if process.returncode:
        raise SystemExit(process.stderr or process.stdout)
    return process.stdout

before = run('set -eu; test "$(cat /sys/class/dmi/id/board_name)" = 0VK62X; '
             'test "$(findmnt -n -o SOURCE /boot/efi)" = /dev/nvme0n1p1; '
             'test "$(cat /var/lib/companion/healthy-boot-id)" = "$(cat /proc/sys/kernel/random/boot_id)"; '
             'test -s /boot/efi/EFI/alpine/grubx64.efi; '
             'test ! -e /boot/efi/EFI/companion/' + kind + ('-probe-02.txt; ' if args.http_ethernet_probe else '-probe-01.txt; ') +
             'test -s /boot/efi/EFI/companion/recoveryx64.efi; efibootmgr -v')
if args.network_probe or args.http_probe or args.udp_probe or args.http_inspect_probe or args.http_prereq_probe or (args.http_init_probe or args.http_ethernet_probe or args.http_cleanup_probe):
    run('set -eu; rc-service sshd status >/dev/null; rc-service companion-watch status >/dev/null; '
        'test -s /boot/efi/companion/stable/vmlinuz-lts; test -s /boot/efi/companion/stable/initramfs-lts')
    if 'BootNext:' in before or 'BootOrder: 0005,0000\n' not in before:
        raise SystemExit('Unexpected boot configuration; no experiment staged')
    if args.http_inspect_probe or args.http_prereq_probe or (args.http_init_probe or args.http_ethernet_probe or args.http_cleanup_probe):
        drivers=run('efibootmgr --driver -v')
        if 'DriverOrder: 0000,0001\n' not in drivers or 'Companion Extension 01\t' not in drivers:
            raise SystemExit('Unexpected working driver configuration; no experiment staged')
        run('set -eu; test "$(sha256sum /boot/efi/EFI/companion/companionextx64.efi | cut -d " " -f 1)" = '
            '2ebfd763305cddb886fae0793f60e08ae073f919b72ca1d6b405e61eb7e08df4')
if args.http_probe or args.udp_probe:
    if state['address']!='10.8.22.238':
        raise SystemExit('Native test uses the currently leased target address; rebuild for a changed lease')
    ready=run('set -eu; ip -o -4 address show dev eth0; wget -T 3 -q -O - http://10.8.22.122:18080/health')
    if '10.8.22.238/24' not in ready or not ready.endswith('COMPANION_HTTP_READY_01\n'):
        raise SystemExit('Controller endpoint or target lease differs; no reboot')
if args.udp_probe:
    health_code="import socket; s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.settimeout(3); q=b'CMPNET01'+bytes(4)+bytes([99,0,0,0]); s.sendto(q,('10.8.22.122',18081)); data,peer=s.recvfrom(64); assert data==q and peer==('10.8.22.122',18081); print('COMPANION_UDP_READY_01')"
    if run('python3 -c '+shlex.quote(health_code)).strip()!='COMPANION_UDP_READY_01':
        raise SystemExit('UDP endpoint failed readiness; no reboot')
(repo / ('artifacts/firmware/' + kind + '-probe-boot-before.txt')).write_text(before, encoding='utf-8')
run('mkdir -p /var/lib/companion/firmware/probe-stage')
subprocess.run(['scp'] + options + [str(binary), target + ':/var/lib/companion/firmware/probe-stage/' + efi_name],
               check=True, timeout=30)
command = r'''set -eu
test "$(sha256sum /var/lib/companion/firmware/probe-stage/fvprobex64.efi | cut -d ' ' -f 1)" = '@HASH@'
if [ -e /boot/efi/EFI/companion/fvprobex64.efi ]; then
    test "$(sha256sum /boot/efi/EFI/companion/fvprobex64.efi | cut -d ' ' -f 1)" = '@HASH@'
fi
cp /var/lib/companion/firmware/probe-stage/fvprobex64.efi /boot/efi/EFI/companion/fvprobex64.efi
sync
efibootmgr --create-only --disk /dev/nvme0n1 --part 1 --label 'Companion Firmware Probe 01' --loader '\EFI\companion\fvprobex64.efi'
efibootmgr -v
'''.replace('@HASH@', digest).replace('fvprobex64.efi', efi_name).replace('Companion Firmware Probe 01', label)
after = run(command)
(repo / ('artifacts/firmware/' + kind + '-probe-boot-staged.txt')).write_text(after, encoding='utf-8')
order_before = next(line for line in before.splitlines() if line.startswith('BootOrder:'))
order_after = next(line for line in after.splitlines() if line.startswith('BootOrder:'))
if order_before != order_after:
    raise SystemExit('Boot order unexpectedly changed; no reboot requested')
entries = sorted(set(line for line in after.splitlines() if line.startswith('Boot') and label + '\t' in line))
if len(entries) != 1:
    raise SystemExit('Unexpected probe entry count; no reboot requested')
entry = entries[0][4:8]
if not all(c in '0123456789ABCDEFabcdef' for c in entry):
    raise SystemExit('Invalid boot entry')
report = {'scope': deployment_scope, 'sha256': digest, 'entry': entry, 'normal_boot_order': order_after,
          'previous_boot_id': run('cat /proc/sys/kernel/random/boot_id').strip(), 'reboot_requested': args.reboot}
if args.reboot:
    print(run('set -eu; efibootmgr --bootnext ' + entry + '; sync; '
              "nohup sh -c 'sleep 2; reboot' >/var/lib/companion/firmware/probe-stage/reboot.log 2>&1 </dev/null &"))
(repo / ('artifacts/firmware/' + kind + '-probe-deployment.json')).write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))

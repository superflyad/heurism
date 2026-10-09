"""Reconnect to the trusted Dell, including after DHCP changes or reboot.

Commands execute once, after an authenticated probe. Failed commands are never
retried automatically because they may have already changed the remote system.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import ipaddress
import json
from pathlib import Path
import socket
import subprocess
import sys
import time
from boot_safety import validate_command, validate_remote_power

repo = Path(__file__).resolve().parents[1]
identity = repo / 'artifacts' / 'ssh'
statefile = repo / 'artifacts' / 'targets' / 'dell.json'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--address', help='Try this address first')
parser.add_argument('--wait', type=int, default=0, help='Seconds to wait for trusted access')
parser.add_argument('--timeout', type=int, default=30, help='Command execution timeout')
parser.add_argument('--command', default='heurismctl status')
parser.add_argument('--local-recovery-ready', action='store_true',
                    help='Permit remote power only with a person able to inspect and recover the Dell locally')
parser.add_argument('--wake', action='store_true', help='Send a local Wake-on-LAN packet (firmware support required)')
parser.add_argument('--after-boot', help='Wait for a boot ID different from this value')
args = parser.parse_args()
try:
    validate_command(args.command)
    validate_remote_power(args.command, args.local_recovery_ready)
except ValueError as error:
    parser.error(str(error))
state = json.loads(statefile.read_text()) if statefile.exists() else {}
preferred = args.address or state.get('address', '10.8.22.238')
network = ipaddress.ip_network(state.get('discovery_subnet', '10.8.22.0/24'))
assert network.is_private and network.version == 4 and network.prefixlen >= 24
assert args.wait >= 0 and args.timeout > 0
if args.wake:
    mac = bytes.fromhex(state['ethernet_mac'].replace(':', ''))
    assert len(mac) == 6
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
        sender.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sender.sendto(b'\xff' * 6 + mac * 16, (str(network.broadcast_address), 9))
ssh = ['ssh', '-i', str(identity / 'companion_client_ed25519'),
       '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=3',
       '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=companion-dell',
       '-o', 'UserKnownHostsFile=' + str(identity / 'known_hosts'),
       '-o', 'ServerAliveInterval=10', '-o', 'ServerAliveCountMax=3']

def probe(address):
    try:
        result = subprocess.run(ssh + ['root@' + address,
            'case "$(cat /etc/hostname)" in companion-dell|heurism-dell) ;; *) exit 1;; esac; test "$(id -u)" = 0 && cat /proc/sys/kernel/random/boot_id'],
            capture_output=True, text=True, timeout=6, creationflags=0x08000000)
        if result.returncode == 0:
            return result.stdout.strip()
    except (subprocess.TimeoutExpired, OSError):
        pass
    return None

def listening(address):
    try:
        with socket.create_connection((str(address), 22), timeout=0.35):
            return str(address)
    except OSError:
        return None

deadline = time.monotonic() + args.wait
last_scan = float('-inf')
while True:
    boot_id = probe(preferred)
    address = preferred
    if not boot_id and time.monotonic() - last_scan >= 20:
        last_scan = time.monotonic()
        # Only addresses in the configured local /24. An open port is not trust:
        # every candidate must also match the pinned SSH host key and root key.
        with ThreadPoolExecutor(max_workers=32) as pool:
            candidates = [candidate for candidate in pool.map(listening, network.hosts()) if candidate]
        for candidate in candidates:
            boot_id = probe(candidate)
            if boot_id:
                address = candidate
                break
    if boot_id and boot_id != args.after_boot:
        state.update(address=address, discovery_subnet=str(network), boot_id=boot_id,
                     last_seen_utc=datetime.now(timezone.utc).isoformat())
        statefile.parent.mkdir(parents=True, exist_ok=True)
        statefile.write_text(json.dumps(state, indent=2))
        print(f'Trusted root connection: {address}; boot {boot_id}', file=sys.stderr, flush=True)
        result = subprocess.run(ssh + ['root@' + address, args.command], timeout=args.timeout)
        sys.exit(result.returncode)
    if time.monotonic() >= deadline:
        sys.exit('Dell is not reachable with its expected SSH identity on the configured network.')
    time.sleep(min(5, max(0, deadline - time.monotonic())))

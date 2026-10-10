"""Bounded host-side health gate for the isolated Heurism A/B candidate VM.

Run after queuing and restarting B. Hyper-V can reset a hung guest without
guest SSH; the cleared one-time GRUB entry then returns to A. Never targets
the Dell or CompanionDev's disk.
"""
import argparse
import json
from pathlib import Path
import runpy
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
KEYS = ROOT/'artifacts/prime-vm'
OUT = ROOT/'build/prime-vm'
HOST = runpy.run_path(str(ROOT/'tools/prime-vm.py'))['host']
CANDIDATE_ID = 'ef556cc9-85df-484d-b765-cc3070a315a2'
MAIN_ID = '43d94d09-f1b1-4980-a66c-4736526639e2'
ADDRESS = '172.28.50.3'


def checked_vm(disk):
    state = json.loads(HOST("$v=Get-VM -Name HeurismCandidate,CompanionDev; "
                            "$v | Select-Object Name,Id,State | ConvertTo-Json -Compress"))
    found = {item['Name']: item for item in state}
    candidate = found.get('HeurismCandidate', {})
    main = found.get('CompanionDev', {})
    if (str(candidate.get('Id')).lower() != CANDIDATE_ID or
            str(main.get('Id')).lower() != MAIN_ID or
            candidate.get('State') != 2 or main.get('State') != 3):
        raise RuntimeError('VM identity or state mismatch')
    attached = HOST("(Get-VMHardDiskDrive -VMName HeurismCandidate).Path")
    if attached.lower() != disk.lower():
        raise RuntimeError('Candidate disk path mismatch')


def pin_image():
    fields = (OUT/'image-host.pub').read_text().split()
    if len(fields) < 2 or fields[0] != 'ssh-ed25519':
        raise RuntimeError('Candidate public key missing')
    KEYS.mkdir(parents=True, exist_ok=True)
    pin = KEYS/'candidate-known_hosts'
    pin.write_text('heurism-ab-candidate '+' '.join(fields[:2])+'\n')
    return pin


def probe(pin):
    command = ('cat /etc/heurism/slot; cat /proc/sys/kernel/random/boot_id; '
               '/opt/heurism/native/current/heurism-release verify >/dev/null && '
               '/opt/heurism/native/current/heurism-release health >/dev/null && '
               '/opt/heurism/native/current/heurismctl status >/dev/null && '
               'sha256sum -c /etc/companion/vm-protected.sha256 >/dev/null')
    args = ['ssh', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=6',
            '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=heurism-ab-candidate',
            '-o', 'UserKnownHostsFile='+str(pin), '-o', 'IdentitiesOnly=yes',
            '-i', str(KEYS/'client_ed25519'), '-J', 'prime-linux',
            'root@'+ADDRESS, command]
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=12)
    except subprocess.TimeoutExpired:
        return None
    if 'Host key verification failed' in result.stderr or 'REMOTE HOST IDENTIFICATION' in result.stderr:
        raise RuntimeError('Candidate SSH identity changed')
    lines = result.stdout.splitlines()
    if len(lines) < 2 or lines[0] not in ('A', 'B'):
        return None
    return {'slot': lines[0], 'boot_id': lines[1], 'healthy': result.returncode == 0}


def wait(pin, deadline, after_boot):
    while time.monotonic() < deadline:
        result = probe(pin)
        if result and result['healthy'] and result['boot_id'] != after_boot:
            return result
        time.sleep(3)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--disk', required=True, help='Exact candidate VHDX path on PrimeServer')
    parser.add_argument('--after-boot', required=True,
                        help='Boot ID before the candidate start or restart')
    parser.add_argument('--timeout', type=int, default=90)
    args = parser.parse_args()
    if not 30 <= args.timeout <= 180:
        parser.error('timeout must be 30..180 seconds')
    if not args.disk.lower().startswith('d:\\hyperv\\heurismcandidate\\') or \
            not args.disk.lower().endswith('.vhdx'):
        parser.error('disk must be a candidate VM VHDX')
    try:
        after_boot = str(uuid.UUID(args.after_boot))
    except ValueError:
        parser.error('after-boot must be a UUID')
    pin = pin_image()
    checked_vm(args.disk)
    first = wait(pin, time.monotonic()+args.timeout, after_boot)
    if first and first['slot'] == 'B':
        outcome = {'result': 'B_healthy', **first}
    elif first and first['slot'] == 'A':
        outcome = {'result': 'A_fallback', **first}
    else:
        checked_vm(args.disk)
        HOST("$v=Get-VM -Name HeurismCandidate; "
             "if ($v.Id.ToString() -ne '"+CANDIDATE_ID+"') { throw 'candidate identity changed' }; "
             "Restart-VM -Name HeurismCandidate -Force", timeout=120)
        recovered = wait(pin, time.monotonic()+90, after_boot)
        if not recovered or recovered['slot'] != 'A':
            raise RuntimeError('Host reset did not return healthy A')
        outcome = {'result': 'A_after_host_timeout_reset', **recovered}
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT/'ab-watch.json').write_text(json.dumps(outcome, indent=2)+'\n')
    print(json.dumps(outcome), flush=True)


if __name__ == '__main__':
    main()

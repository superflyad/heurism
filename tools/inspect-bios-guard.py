"""Read only BIOS Guard capability/control MSRs through pinned root SSH.

Loads Linux's msr read interface; never writes a register or invokes BIOS Guard.
Control low-bit interpretation is corroborated with the extracted firmware.
"""
from datetime import datetime, timezone
import argparse
import json
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--policy-hashes', action='store_true',
                    help='Also read the four BGPDT hash MSRs; never write them')
args = parser.parse_args()
identity = repo / 'artifacts/ssh'
state = json.loads((repo / 'artifacts/targets/dell.json').read_text())
ssh = ['ssh', '-i', str(identity / 'companion_client_ed25519'),
       '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=5', '-o', 'StrictHostKeyChecking=yes',
       '-o', 'HostKeyAlias=companion-dell',
       '-o', 'UserKnownHostsFile=' + str(identity / 'known_hosts'), 'root@' + state['address']]
program = '''import json, os, struct, subprocess
from pathlib import Path
subprocess.run(['modprobe','msr'],check=True,capture_output=True)
if os.getuid()!=0 or Path('/sys/class/dmi/id/board_name').read_text().strip()!='0VK62X':
 raise SystemExit('Unexpected target')
records={}
for device in sorted(Path('/dev/cpu').glob('*/msr')):
 row={}
 for name,offset in [('platform_info',0xce),('bios_guard_control',0x110),('boot_guard_info',0x13a)]:
  try:
   with device.open('rb',buffering=0) as stream:
    stream.seek(offset)
    raw=stream.read(8)
   if len(raw)!=8: raise ValueError('Short read')
   row[name]=hex(struct.unpack('<Q',raw)[0])
  except (OSError,ValueError) as error: row[name]={'error':str(error)}
 records[device.parent.name]=row
print(json.dumps({'boot_id':Path('/proc/sys/kernel/random/boot_id').read_text().strip(),'cpus':records}))
'''
if args.policy_hashes:
    program = program.replace("('boot_guard_info',0x13a)",
                              "('boot_guard_info',0x13a),('bgpdt_hash_0',0x111),"
                              "('bgpdt_hash_1',0x112),('bgpdt_hash_2',0x113),('bgpdt_hash_3',0x114)")
result = subprocess.run(ssh + ['python3 -'], input=program, capture_output=True,
                        text=True, timeout=30)
if result.returncode:
    raise SystemExit(result.stderr)
report = json.loads(result.stdout)
report['utc'] = datetime.now(timezone.utc).isoformat()
report['scope'] = 'Read-only MSR observations; does not unlock or test flash writes'
name = 'bios-guard-policy-msr.json' if args.policy_hashes else 'bios-guard-msr.json'
(repo / 'artifacts/firmware' / name).write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))

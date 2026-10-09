"""Verify preboot transfer, test an offline controller, and restore normal boot."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import urllib.request
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('action',choices=['verify','offline','finalize'])
args=parser.parse_args()
repo=Path(__file__).resolve().parents[1];root=repo/'artifacts/firmware'
state=json.loads((repo/'artifacts/targets/dell.json').read_text())
deployment=json.loads((root/'udp-probe-deployment.json').read_text())
metadata_path=root/'udp-probe-verification.json'
metadata=json.loads(metadata_path.read_text()) if metadata_path.exists() else {'scope':__doc__,'runs':[]}
identity=repo/'artifacts/ssh'
ssh=['ssh','-i',str(identity/'companion_client_ed25519'),'-o','BatchMode=yes',
 '-o','StrictHostKeyChecking=yes','-o','HostKeyAlias=companion-dell',
 '-o','UserKnownHostsFile='+str(identity/'known_hosts'),'root@'+state['address']]
def run(command):
 p=subprocess.run(ssh+[command],capture_output=True,timeout=30)
 if p.returncode:raise SystemExit(p.stderr.decode(errors='replace') or p.stdout.decode(errors='replace'))
 return p.stdout
def save():metadata_path.write_text(json.dumps(metadata,indent=2))
boot_id=run('set -eu; test "$(cat /sys/class/dmi/id/board_name)" = 0VK62X; '
 'test "$(cat /var/lib/companion/healthy-boot-id)" = "$(cat /proc/sys/kernel/random/boot_id)"; '
 'rc-service sshd status >/dev/null; rc-service companion-watch status >/dev/null; '
 'cat /proc/sys/kernel/random/boot_id').decode().strip()
entry=deployment['entry']
if not re.fullmatch(r'[0-9A-Fa-f]{4}',entry) or deployment['normal_boot_order']!='BootOrder: 0005,0000':raise SystemExit('Unexpected deployment')
boot=run('efibootmgr -v').decode();lines=[x for x in boot.splitlines() if x.startswith('Boot'+entry)]
if len(lines)!=1 or 'Companion UDP Probe 01\t' not in lines[0] or '\\EFI\\companion\\udpprobex64.efi' not in lines[0]:raise SystemExit('Own test entry differs')
if args.action=='verify':
 if boot_id==deployment['previous_boot_id'] or any(r['boot_id']==boot_id for r in metadata['runs']):raise SystemExit('Fresh boot required')
 data=run('cat /boot/efi/EFI/companion/udp-probe-01.txt');text=data.decode()
 filename='udp-probe-observation-'+str(len(metadata['runs'])+1)+'.txt';(root/filename).write_bytes(data)
 complete=text.startswith('COMPANION_UDP_TRANSFER_PROBE_01\n') and text.endswith('PROBE_COMPLETE\n')
 online=not metadata['runs']
 required=['UDP_PAYLOAD_REJECTED','UDP_PAYLOAD_EXACT_MATCH','UDP_LOAD_IMAGE status=0x0000000000000000',
  'UDP_START_IMAGE status=0x0000000000000000','UDP_CHILD_PROTOCOL status=0x0000000000000000',
  'UDP_CHILD_INFO status=0x0000000000000000 bytes=0x0000000000000020 magic=0x314458454d504f43 revision=0x0000000000000001 capabilities=0x0000000000000001']
 resets=text.count('UDP_RESET status=0x0000000000000000')==2 and text.count('UDP_DESTROY status=0x0000000000000000')==2
 passed=complete and resets and (all(s in text for s in required) if online else
  metadata.get('controller_deliberately_stopped',False) and 'UDP_LOAD_IMAGE' not in text and 'UDP_PAYLOAD_EXACT_MATCH' not in text and
  ('UDP_RECEIVE status=0x8000000000000012' in text or 'UDP_RECEIVE status=0x8000000000000016' in text))
 server_state=json.loads((root/'http-server-state.json').read_text())
 packets=[json.loads(line) for line in (root/'http-server-requests.jsonl').read_text().splitlines()]
 packets=[p for p in packets if p.get('protocol')=='UDP' and p.get('source_port')==18082 and p['unix_time']>server_state['started_unix']]
 if online and {(p['mode'],p['part']) for p in packets}!={(0,0),(0,1),(1,0),(1,1)}:passed=False
 result={'boot_id':boot_id,'online_controller':online,'report':filename,'report_sha256':hashlib.sha256(data).hexdigest(),
  'passed':passed,'root_and_watcher_healthy':True,'network_image_executed':'UDP_START_IMAGE status=0x0000000000000000' in text,
  'records':[x for x in text.splitlines() if x.startswith('UDP_')]}
 metadata['runs'].append(result);save();print(json.dumps(result,indent=2))
 if not passed:
  run('set -eu; efibootmgr --quiet --bootnum '+entry+' --delete-bootnum; efibootmgr --quiet --bootorder 0005,0000')
  metadata['failed_test_entry_removed']=True;save();raise SystemExit('Test did not pass; normal boot restored')
elif args.action=='offline':
 if len(metadata['runs'])!=1 or not metadata['runs'][0]['passed'] or metadata['runs'][0]['boot_id']!=boot_id:raise SystemExit('Verify online run first')
 server_state=json.loads((root/'http-server-state.json').read_text());pid=server_state['pid']
 if not isinstance(pid,int) or pid<=0:raise SystemExit('Invalid own server PID')
 command="$p=Get-CimInstance Win32_Process -Filter 'ProcessId="+str(pid)+"'; if (!$p -or $p.CommandLine -notlike '*companion\\tools\\preboot-http-server.cjs*') { throw 'Own server process differs' }; Stop-Process -Id "+str(pid)+" -ErrorAction Stop"
 subprocess.run(['powershell','-NoProfile','-Command',command],check=True,capture_output=True,timeout=15)
 try:
  urllib.request.urlopen(server_state['url']+'/health',timeout=1)
 except OSError:pass
 else:raise SystemExit('Controller still reachable; do not schedule offline test')
 metadata['controller_deliberately_stopped']=True;save()
 run('set -eu; test ! -e /boot/efi/EFI/companion/udp-probe-first.txt; '
  'mv /boot/efi/EFI/companion/udp-probe-01.txt /boot/efi/EFI/companion/udp-probe-first.txt; '
  'efibootmgr --bootnext '+entry+'; sync; '
  "nohup sh -c 'sleep 2; reboot' >/var/lib/companion/firmware/probe-stage/udp-offline.log 2>&1 </dev/null &")
 metadata['offline_requested_from_boot']=boot_id;save();print('Own test server stopped; one bounded offline-controller boot scheduled')
else:
 if len(metadata['runs'])!=2 or not all(r['passed'] for r in metadata['runs']) or metadata['runs'][-1]['boot_id']!=boot_id:raise SystemExit('Two verified runs required')
 run('set -eu; efibootmgr --quiet --bootnum '+entry+' --delete-bootnum; efibootmgr --quiet --bootorder 0005,0000')
 final=run('efibootmgr; efibootmgr --driver').decode()
 assert 'BootNext:' not in final and 'BootOrder: 0005,0000\n' in final and 'DriverOrder: 0000,0001\n' in final
 raw=run('cat /sys/firmware/efi/efivars/CompanionExtensionImage01-1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1')
 assert len(raw)==2052 and raw[:4]==b'\7\0\0\0' and hashlib.sha256(raw[4:]).hexdigest()=='b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a'
 metadata.update(temporary_entry_removed=True,normal_boot_order='0005,0000',driver_order='0000,0001',
  board_payload_unchanged=True,ssd_independent_startup_proven=False,offline_controller_fallback_proven=True)
 save();print(json.dumps(metadata,indent=2))

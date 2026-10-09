"""Deploy/verify the NV-reading SSD driver without changing boot/driver entries.

The driver has an embedded SSD-service fallback. This tool retains the original
driver file as a rollback copy. Root/recovery still depend on the working SSD.
"""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('action',choices=['deploy','reboot','verify','rollback'])
args=parser.parse_args();repo=Path(__file__).resolve().parents[1]
build=repo/'build/nv-startup';metadata_path=repo/'artifacts/firmware/nv-startup-deployment.json'
original='b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a'
installed='/boot/efi/EFI/companion/companionextx64.efi'
backup='/boot/efi/EFI/companion/companionextx64.ssd-backup.efi'

def run(command):
 p=subprocess.run([sys.executable,str(repo/'tools/dell.py'),'--command',command,'--timeout','30'],
                  capture_output=True,text=True,timeout=50)
 if p.returncode:raise RuntimeError(p.stderr+p.stdout)
 return p.stdout

def save(metadata):metadata_path.write_text(json.dumps(metadata,indent=2))

def preflight():
 subprocess.run([sys.executable,str(repo/'tools/backup-boot-variables.py')],check=True,timeout=60)
 info=run('set -eu; test "$(cat /sys/class/dmi/id/board_name)" = 0VK62X; '
          'test "$(findmnt -n -o SOURCE /boot/efi)" = /dev/nvme0n1p1; '
          'rc-service sshd status >/dev/null; rc-service companion-watch status >/dev/null; '
          'test -s /boot/efi/companion/stable/vmlinuz-lts; test -s /boot/efi/companion/stable/initramfs-lts; '
          'cat /proc/sys/kernel/random/boot_id; efibootmgr --driver -v')
 if 'Driver0001* Companion Extension 01\t' not in info or '\\EFI\\companion\\companionextx64.efi' not in info:
  raise RuntimeError('Unexpected installed driver entry')
 return info.splitlines()[0]

boot_id=preflight()
if args.action=='deploy':
 if metadata_path.exists():raise RuntimeError('Deployment exists; use verify or rollback')
 data=(build/'companionextx64.efi').read_bytes();digest=hashlib.sha256(data).hexdigest()
 host=json.loads((build/'host-verification.json').read_text());vm=json.loads((build/'vm-verification.json').read_text())
 assert host['passed'] and host['host_cases']==13 and host['sha256']==digest
 assert vm['driver_sha256']==digest and len(vm['cases'])==3 and all(c['passed'] for c in vm['cases'])
 assert {c['case'] for c in vm['cases']}=={'valid-nv','missing-nv','corrupt-nv'}
 run('set -eu; test "$(sha256sum '+installed+' | cut -d " " -f 1)" = '+original+'; '
     'test ! -e '+backup+'; mkdir -p /var/lib/companion/firmware/probe-stage')
 state=json.loads((repo/'artifacts/targets/dell.json').read_text());identity=repo/'artifacts/ssh'
 options=['-i',str(identity/'companion_client_ed25519'),'-o','BatchMode=yes','-o','StrictHostKeyChecking=yes',
          '-o','HostKeyAlias=companion-dell','-o','UserKnownHostsFile='+str(identity/'known_hosts')]
 subprocess.run(['scp',*options,str(build/'companionextx64.efi'),'root@'+state['address']+':/var/lib/companion/firmware/probe-stage/nvstartupx64.efi'],check=True,timeout=30)
 metadata={'scope':__doc__,'driver_sha256':digest,'original_driver_sha256':original,
           'previous_boot_id':boot_id,'verified_runs':[],'status':'prepared'}
 save(metadata)
 run('set -eu; test "$(sha256sum /var/lib/companion/firmware/probe-stage/nvstartupx64.efi | cut -d " " -f 1)" = '+digest+'; '
     'test "$(sha256sum '+installed+' | cut -d " " -f 1)" = '+original+'; '
     'cp '+installed+' '+backup+'; sync; '
     'test "$(sha256sum '+backup+' | cut -d " " -f 1)" = '+original+'; '
     'cp /var/lib/companion/firmware/probe-stage/nvstartupx64.efi '+installed+'.new; sync; '
     'test "$(sha256sum '+installed+'.new | cut -d " " -f 1)" = '+digest+'; '
     'mv '+installed+'.new '+installed+'; sync; '
     'test "$(sha256sum '+installed+' | cut -d " " -f 1)" = '+digest)
 metadata['status']='installed-not-yet-booted';save(metadata)
else:
 metadata=json.loads(metadata_path.read_text());digest=metadata['driver_sha256']
 if not len(digest)==64 or any(c not in '0123456789abcdef' for c in digest):raise ValueError('Invalid saved digest')
 run('set -eu; test "$(sha256sum '+installed+' | cut -d " " -f 1)" = '+digest+'; '
     'test "$(sha256sum '+backup+' | cut -d " " -f 1)" = '+original)
 if args.action=='reboot':
  metadata['previous_boot_id']=boot_id;metadata['status']='reboot-requested';save(metadata)
  run("set -eu; sync; nohup sh -c 'sleep 2; reboot' >/var/lib/companion/firmware/probe-stage/nv-startup-reboot.log 2>&1 </dev/null &")
 elif args.action=='verify':
  if boot_id==metadata['previous_boot_id'] or any(r['boot_id']==boot_id for r in metadata['verified_runs']):raise RuntimeError('No new boot to verify')
  raw=base64.b64decode(run('base64 /sys/firmware/efi/efivars/CompanionNvStartup01-1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1').strip())
  assert len(raw)==68 and int.from_bytes(raw[:4],'little')==6
  marker=struct.unpack('<8Q',raw[4:]);assert marker[:3]==(0x31564e504d4f4343,1,1) and marker[3:7]==(0,0,0,0),marker
  record={'boot_id':boot_id,'marker_base64':base64.b64encode(raw).decode(),'path':'verified-NVRAM-child',
          'read_load_start_info_status':[hex(v) for v in marker[3:7]],'healthy_root_reconnected':True,
          'boot_order':'0005,0000','driver_order':'0000,0001','ssd_bootstrap_required':True}
  metadata['verified_runs'].append(record);metadata['status']='verified-normal-startup';save(metadata)
 elif args.action=='rollback':
  run('set -eu; cp '+backup+' '+installed+'.rollback; sync; '
      'test "$(sha256sum '+installed+'.rollback | cut -d " " -f 1)" = '+original+'; '
      'mv '+installed+'.rollback '+installed+'; sync; '
      'test "$(sha256sum '+installed+' | cut -d " " -f 1)" = '+original)
  metadata['status']='rolled-back';save(metadata)
print(json.dumps(metadata,indent=2))

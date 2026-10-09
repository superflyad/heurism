"""Survey existing Dell bootstrap modules from a pinned saved ROM, not live flash.

Only extracts files in the target's research directory and copies evidence.
Never calls a vendor service, changes firmware variables, or reboots.
"""
import base64
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import uuid

repo=Path(__file__).resolve().parents[1];root=repo/'artifacts/firmware'
out=repo/'artifacts/research/independent-bootstrap';out.mkdir(parents=True,exist_ok=True)
rom_hash='09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
revision='dac91b26733ca21cb204e41614c1b8c81cc50860'
assert hashlib.sha256((root/'bios16-a.bin').read_bytes()).hexdigest()==rom_hash
remote='/var/lib/companion/firmware/research';rom='/var/lib/companion/firmware/bios16-a.bin'
def read(command):
 p=subprocess.run([sys.executable,str(repo/'tools/dell.py'),'--timeout','45','--command',command],
                  capture_output=True,text=True,timeout=60)
 if p.returncode:raise RuntimeError(p.stderr+p.stdout)
 return p.stdout
assert read('sha256sum '+rom).split()[0]==rom_hash
assert read('git -C '+remote+'/uefitool rev-parse HEAD').strip()==revision
modules=[('bds-dxe','6D33944A-EC75-4855-A54D-809C75241F6C'),
 ('biosconnect-launcher','44F8D447-A021-46AA-9811-12C1EA02119D'),
 ('biosconnect-network','B94FC17C-579C-4AB3-BA28-678D1813D1D6'),
 ('biosconnect-download','E0E1AB16-C482-4015-AE70-64BDFCAA89AB'),
 ('http-boot-dxe','ECEBCB00-D9C8-11E4-AF3D-8CDCD426C973'),
 ('auto-os-recovery','7EF09900-7397-45C0-9CA6-698324391870'),
 ('special-boot-stub','6B287864-759C-42C4-B435-A74AB694CD3B')]
protocols={'loadfile':'56ec3091-954c-11d2-8e3f-00a0c969723b',
 'loadfile2':'4006c0c1-fcb3-403e-996d-4a6c8724e06d',
 'fv2':'220e73b6-6bdb-4413-8405-b974b108619a',
 'http':'7a59b29b-910b-4171-8242-a85a0df25b5b',
 'owner_nv':'1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1'}
records=[]
for name,guid in modules:
 folder=remote+'/independent-'+name+'-'+rom_hash[:8]
 read('set -eu; test -f '+folder+'/body.bin || '+remote+'/uefitool-build/uefiextract '+rom+' '+guid+' -o '+folder+' -m body -t 10')
 data=base64.b64decode(read('base64 '+folder+'/body.bin'))
 digest=hashlib.sha256(data).hexdigest()
 assert read('sha256sum '+folder+'/body.bin').split()[0]==digest
 (out/(name+'.bin')).write_bytes(data)
 read('objdump -d '+folder+'/body.bin > '+folder+'/disassembly.txt')
 (out/(name+'.txt')).write_text(read('cat '+folder+'/disassembly.txt'))
 strings=sorted(set(x.decode('ascii') for x in re.findall(rb'[\x20-\x7e]{6,}',data))|
                set(x.decode('utf-16-le') for x in re.findall(rb'(?:[\x20-\x7e]\x00){6,}',data)))
 relevant=[s for s in strings if re.search(r'http|dell|boot|recover|image|signature|certif|loadfile|variable|\.efi|\.bin|\.xml',s,re.I)]
 (out/(name+'-strings.json')).write_text(json.dumps(relevant,indent=2))
 record={'name':name,'ffs_guid':guid,'sha256':digest,'bytes':len(data),
         'protocol_guid_offsets':{k:[hex(m.start()) for m in re.finditer(re.escape(uuid.UUID(v).bytes_le),data)] for k,v in protocols.items()},
         'relevant_string_count':len(relevant)}
 records.append(record);print(name+': '+str(len(data))+' bytes; copied and hashed',flush=True)
report={'scope':__doc__,'rom_sha256':rom_hash,'parser_revision':revision,'modules':records,
 'limits':'GUID/string presence identifies candidates, not provider publication, accepted images or an executable NVRAM hook.'}
(out/'module-survey.json').write_text(json.dumps(report,indent=2))
configuration=read('set -eu; cat /proc/sys/kernel/random/boot_id; rc-service sshd status; rc-service companion-watch status; '
 'for name in BIOSConnect SupportAssistOSRecovery AutoOSRecoveryThreshold UefiNwStack; do '
 'printf "\\n%s\\n" "$name"; cat /sys/class/firmware-attributes/dell-wmi-sysman/attributes/"$name"/current_value; done; '
 'efibootmgr; efibootmgr --driver')
(out/'live-settings-and-health.txt').write_text(configuration)

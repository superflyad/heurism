"""Collect live capsule/settings evidence through pinned root SSH; read-only."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

repo = Path(__file__).resolve().parents[1]
state = json.loads((repo/'artifacts/targets/dell.json').read_text())
options = ['-i',str(repo/'artifacts/ssh/companion_client_ed25519'),'-o','BatchMode=yes',
           '-o','StrictHostKeyChecking=yes','-o','HostKeyAlias=companion-dell',
           '-o','UserKnownHostsFile='+str(repo/'artifacts/ssh/known_hosts')]
target = 'root@'+state['address']
remote = r'''
import json, os
from pathlib import Path
import subprocess
assert os.getuid()==0 and Path('/etc/hostname').read_text().strip()=='companion-dell'
def read(path):
    try:return Path(path).read_text().strip()
    except OSError:return None
settings={}
base=Path('/sys/class/firmware-attributes/dell-wmi-sysman/attributes')
for name in ['CapsuleFirmwareUpdate','AllowBiosDowngrade','SecureBoot','SecureBootMode',
             'SmmSecurityMitigation','BIOSConnect','BiosRcvrFrmHdd','FOTA','Fastboot']:
    settings[name]={field:read(base/name/field) for field in ['current_value','possible_values','type','display_name']}
entries=[]
for path in sorted(Path('/sys/firmware/efi/esrt/entries').iterdir()):
    item={field:read(path/field) for field in ['fw_class','fw_type','fw_version',
           'lowest_supported_fw_version','capsule_flags','last_attempt_version','last_attempt_status']}
    entries.append(item)
mode={}
for name in ['SecureBoot','SetupMode','AuditMode','DeployedMode']:
    p=Path('/sys/firmware/efi/efivars')/(name+'-8be4df61-93ca-11d2-aa0d-00e098032b8c')
    try:
        d=p.read_bytes();mode[name]=d[4] if len(d)==5 else None
    except OSError:mode[name]=None
print(json.dumps({'boot_id':read('/proc/sys/kernel/random/boot_id'),
    'healthy_boot_id':read('/var/lib/companion/healthy-boot-id'),
    'bios_version':read('/sys/class/dmi/id/bios_version'),'settings':settings,'esrt':entries,
    'secure_boot_modes':mode,'boot_entries':subprocess.check_output(['efibootmgr'],text=True)}))
'''
result=subprocess.run(['ssh']+options+[target,'python3 -'],input=remote.encode(),capture_output=True,timeout=30)
if result.returncode:raise SystemExit(result.stderr.decode(errors='replace'))
snapshot=json.loads(result.stdout)
if snapshot['boot_id']!=snapshot['healthy_boot_id']:raise SystemExit('Management boot is not healthy')
result=subprocess.run(['ssh']+options+[target,'cat /boot/efi/EFI/companion/update-probe-01.txt'],
                      capture_output=True,timeout=30)
if result.returncode:raise SystemExit('Cannot read physical probe report')
expected='c6e006fbdf92eb0eb9217df751d08f74c075ecfd4931bff474a63b5a713e568e'
if hashlib.sha256(result.stdout).hexdigest()!=expected:raise SystemExit('Physical report changed')
lines=result.stdout.decode('ascii').splitlines()
if lines[0]!='COMPANION_UPDATE_PROBE_01' or lines[-1]!='PROBE_COMPLETE':
    raise SystemExit('Incomplete update probe')
if 'FMP_LOCATE status=0x800000000000000e count=0x0000000000000000' not in lines:
    raise SystemExit('Expected physical FMP result changed')
snapshot.update({'utc':datetime.now(timezone.utc).isoformat(),'scope':__doc__,
                 'physical_probe_sha256':expected,'physical_fmp_handles':0,
                 'firmware_write_authorization_verified':False})
folder=repo/'artifacts/firmware'
(folder/'update-interfaces-live.json').write_text(json.dumps(snapshot,indent=2),encoding='utf-8')
(folder/'update-probe-01.txt').write_bytes(result.stdout)
print(json.dumps({'boot_id':snapshot['boot_id'],'bios_version':snapshot['bios_version'],
                  'capsule_resources':len(snapshot['esrt']),'physical_fmp_handles':0,
                  'system_minimum_version':snapshot['esrt'][0]['lowest_supported_fw_version'],
                  'firmware_write_authorization_verified':False},indent=2))

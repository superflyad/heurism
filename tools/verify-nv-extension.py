"""Verify the native NVRAM payload experiment, repeat once, then remove its boot entry.

The owner payload is retained in NVRAM. The normal SSD extension and Dell entry
stay registered; the native test loader is not made the default boot target.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('action',choices=['verify','retest','finalize'])
args=parser.parse_args()
repo=Path(__file__).resolve().parents[1];root=repo/'artifacts/firmware'
state=json.loads((repo/'artifacts/targets/dell.json').read_text())
deployment=json.loads((root/'nv-probe-deployment.json').read_text())
metadata_path=root/'nv-extension-verification.json'
metadata=json.loads(metadata_path.read_text()) if metadata_path.exists() else {'scope':__doc__,'runs':[]}
identity=repo/'artifacts/ssh'
ssh=['ssh','-i',str(identity/'companion_client_ed25519'),'-o','BatchMode=yes',
     '-o','StrictHostKeyChecking=yes','-o','HostKeyAlias=companion-dell',
     '-o','UserKnownHostsFile='+str(identity/'known_hosts'),'root@'+state['address']]
def run(command):
    p=subprocess.run(ssh+[command],capture_output=True,timeout=30)
    if p.returncode:raise SystemExit(p.stderr.decode(errors='replace') or p.stdout.decode(errors='replace'))
    return p.stdout
boot_id=run('set -eu; test "$(cat /sys/class/dmi/id/board_name)" = 0VK62X; '
            'test "$(cat /var/lib/companion/healthy-boot-id)" = "$(cat /proc/sys/kernel/random/boot_id)"; '
            'rc-service sshd status >/dev/null; rc-service companion-watch status >/dev/null; '
            'cat /proc/sys/kernel/random/boot_id').decode().strip()
entry=deployment['entry']
if not re.fullmatch(r'[0-9A-Fa-f]{4}',entry) or deployment['normal_boot_order']!='BootOrder: 0005,0000':
    raise SystemExit('Unexpected deployment entries/order')
boot=run('efibootmgr -v').decode()
lines=[line for line in boot.splitlines() if line.startswith('Boot'+entry)]
if len(lines)!=1 or 'Companion NV Probe 01\t' not in lines[0] or '\\EFI\\companion\\nvprobex64.efi' not in lines[0]:
    raise SystemExit('Own test entry differs')
def save():metadata_path.write_text(json.dumps(metadata,indent=2),encoding='utf-8')
if args.action=='verify':
    if boot_id==deployment['previous_boot_id'] or any(r['boot_id']==boot_id for r in metadata['runs']):
        raise SystemExit('No fresh boot to verify')
    data=run('cat /boot/efi/EFI/companion/nv-probe-01.txt');text=data.decode()
    if not text.startswith('COMPANION_NV_EXTENSION_PROBE_01\n') or not text.endswith('PROBE_COMPLETE\n'):
        raise SystemExit('Incomplete report')
    filename='nv-extension-observation-'+str(len(metadata['runs'])+1)+'.txt'
    (root/filename).write_bytes(data)
    required=['NV_CAPACITY status=0x0000000000000000', 'NV_PAYLOAD_EXACT_MATCH',
              'NV_LOAD_IMAGE status=0x0000000000000000','NV_START_IMAGE status=0x0000000000000000',
              'NV_CHILD_PROTOCOL status=0x0000000000000000',
              'NV_CHILD_INFO status=0x0000000000000000 bytes=0x0000000000000020 magic=0x314458454d504f43 revision=0x0000000000000001 capabilities=0x0000000000000001']
    passed=all(s in text for s in required)
    result={'boot_id':boot_id,'report':filename,'report_sha256':hashlib.sha256(data).hexdigest(),
            'native_readback_load_and_child_service_passed':passed,
            'created_payload_this_boot':'NV_CREATE status=0x0000000000000000' in text,
            'payload_present_at_entry':'NV_INITIAL_READ status=0x0000000000000000' in text,
            'healthy_root_reconnected':True,'records':[s for s in text.splitlines() if s.startswith('NV_')]}
    if passed:
        raw=run('cat /sys/firmware/efi/efivars/CompanionExtensionImage01-1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1')
        if len(raw)!=2052 or raw[:4]!=b'\7\0\0\0' or hashlib.sha256(raw[4:]).hexdigest()!='b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a':
            raise SystemExit('OS-side persisted payload does not match')
        (root/'nv-extension-payload-readback.bin').write_bytes(raw[4:])
        result['os_side_nonvolatile_payload_sha256']=hashlib.sha256(raw[4:]).hexdigest()
    metadata['runs'].append(result);save();print(json.dumps(result,indent=2))
    if not passed:
        run('set -eu; efibootmgr --quiet --bootnum '+entry+' --delete-bootnum; efibootmgr --quiet --bootorder 0005,0000')
        metadata['failed_test_boot_registration_removed']=True;save()
        raise SystemExit('Native path did not pass; own test boot entry removed')
elif args.action=='retest':
    if len(metadata['runs'])!=1 or metadata['runs'][-1]['boot_id']!=boot_id or not metadata['runs'][-1]['native_readback_load_and_child_service_passed']:
        raise SystemExit('Verify first run before retest')
    run('set -eu; test ! -e /boot/efi/EFI/companion/nv-probe-first.txt; '
        'mv /boot/efi/EFI/companion/nv-probe-01.txt /boot/efi/EFI/companion/nv-probe-first.txt; '
        'efibootmgr --bootnext '+entry+'; sync; '
        "nohup sh -c 'sleep 2; reboot' >/var/lib/companion/firmware/probe-stage/nv-retest.log 2>&1 </dev/null &")
    metadata['retest_requested_from_boot']=boot_id;save();print('One persistence retest scheduled')
else:
    if len(metadata['runs'])!=2 or metadata['runs'][-1]['boot_id']!=boot_id or not all(r['native_readback_load_and_child_service_passed'] for r in metadata['runs']):
        raise SystemExit('Two verified runs required')
    if not metadata['runs'][1]['payload_present_at_entry'] or metadata['runs'][1]['created_payload_this_boot']:
        raise SystemExit('Second run did not establish persistence without rewriting')
    run('set -eu; efibootmgr --quiet --bootnum '+entry+' --delete-bootnum; efibootmgr --quiet --bootorder 0005,0000')
    final=run('efibootmgr; efibootmgr --driver').decode()
    if 'BootNext:' in final or 'BootOrder: 0005,0000\n' not in final or 'DriverOrder: 0000,0001\n' not in final:
        raise SystemExit('Final normal boot/driver configuration differs')
    metadata['owner_payload_retained_in_nonvolatile_store']=True
    metadata['temporary_probe_entry_removed']=True
    metadata['normal_boot_order']='0005,0000';metadata['original_extension_driver_order']='0000,0001'
    metadata['ssd_independent_bootstrap_proven']=False;save()
    print(json.dumps(metadata,indent=2))

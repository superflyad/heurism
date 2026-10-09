"""Install/test the additive owner UEFI driver via DriverOrder, preserving Dell.

Driver binary stays on the ESP. Only its registration is motherboard NVRAM.
No BIOS code, flash protection, existing driver entry or Dell protocol is altered.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('action',choices=['deploy','verify','retest','finalize','uninstall'])
args=parser.parse_args()
repo=Path(__file__).resolve().parents[1]
root=repo/'artifacts/firmware'
state=json.loads((repo/'artifacts/targets/dell.json').read_text())
identity=repo/'artifacts/ssh'
options=['-i',str(identity/'companion_client_ed25519'),'-o','BatchMode=yes',
         '-o','StrictHostKeyChecking=yes','-o','HostKeyAlias=companion-dell',
         '-o','UserKnownHostsFile='+str(identity/'known_hosts')]
target='root@'+state['address']
metadata_path=root/'extension-deployment.json'
def run(command):
    p=subprocess.run(['ssh']+options+[target,command],capture_output=True,text=True,timeout=30)
    if p.returncode:raise SystemExit(p.stderr or p.stdout)
    return p.stdout
def healthy():
    return run('set -eu; test "$(cat /sys/class/dmi/id/board_name)" = 0VK62X; '
               'test "$(findmnt -n -o SOURCE /boot/efi)" = /dev/nvme0n1p1; '
               'test "$(cat /var/lib/companion/healthy-boot-id)" = "$(cat /proc/sys/kernel/random/boot_id)"; '
               'rc-service sshd status >/dev/null; rc-service companion-watch status >/dev/null; '
               'cat /proc/sys/kernel/random/boot_id').strip()
def order(output,name):
    matches=sorted(set(line[len(name)+2:] for line in output.splitlines() if line.startswith(name+': ')))
    if len(matches)!=1 or not re.fullmatch(r'[0-9A-Fa-f]{4}(,[0-9A-Fa-f]{4})*',matches[0]):
        raise SystemExit('Unexpected '+name)
    return matches[0]
def entry(output,prefix,label,path):
    lines=sorted(set(line for line in output.splitlines() if line.startswith(prefix) and label+'\t' in line))
    if len(lines)!=1 or path not in lines[0]:raise SystemExit('Unexpected entry for '+label)
    number=lines[0][len(prefix):len(prefix)+4]
    if not re.fullmatch(r'[0-9A-Fa-f]{4}',number):raise SystemExit('Invalid entry number')
    return number
def image(path,subsystem):
    data=path.read_bytes();pe=struct.unpack_from('<I',data,60)[0];optional=pe+24
    if data[:2]!=b'MZ' or data[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',data,pe+4)[0]!=0x8664:
        raise SystemExit('Invalid x64 PE')
    if struct.unpack_from('<H',data,optional+68)[0]!=subsystem or struct.unpack_from('<II',data,optional+120)!=(0,0) or not all(struct.unpack_from('<II',data,optional+152)):
        raise SystemExit('Invalid EFI type/imports/relocations')
    return hashlib.sha256(data).hexdigest()
def save(metadata):
    metadata_path.write_text(json.dumps(metadata,indent=2),encoding='utf-8')
def reboot(metadata):
    save(metadata)
    run('set -eu; efibootmgr --bootnext '+metadata['observer_entry']+'; sync; '
        "nohup sh -c 'sleep 2; reboot' >/var/lib/companion/firmware/probe-stage/extension-reboot.log 2>&1 </dev/null &")
    print(json.dumps(metadata,indent=2))

boot_id=healthy()
if args.action=='deploy':
    if metadata_path.exists():raise SystemExit('Deployment metadata exists; refusing duplicate installation')
    driver=repo/'build/extension/companionextx64.efi'
    observer=repo/'build/extension-probe/fvprobex64.efi'
    driver_hash=image(driver,11);observer_hash=image(observer,10)
    boot_before=run('efibootmgr -v');driver_before=run('efibootmgr --driver -v')
    if 'BootNext:' in boot_before:
        raise SystemExit('Pending boot')
    normal_order=order(boot_before,'BootOrder');driver_order=order(driver_before,'DriverOrder')
    if normal_order!='0005,0000' or driver_order!='0000':raise SystemExit('Review changed normal orders')
    run('set -eu; test ! -e /boot/efi/EFI/companion/extension-probe-01.txt; '
        'test -s /boot/efi/EFI/companion/recoveryx64.efi; mkdir -p /var/lib/companion/firmware/probe-stage')
    for source,name,digest in [(driver,'companionextx64.efi',driver_hash),(observer,'extensionprobex64.efi',observer_hash)]:
        subprocess.run(['scp']+options+[str(source),target+':/var/lib/companion/firmware/probe-stage/'+name],check=True,timeout=30)
        run('set -eu; if [ -e /boot/efi/EFI/companion/'+name+' ]; then test "$(sha256sum /boot/efi/EFI/companion/'+name+' | cut -d " " -f 1)" = '+digest+'; fi; '
            'test "$(sha256sum /var/lib/companion/firmware/probe-stage/'+name+' | cut -d " " -f 1)" = '+digest+'; '
            'cp /var/lib/companion/firmware/probe-stage/'+name+' /boot/efi/EFI/companion/'+name+'; sync')
    # This RST/VMD target rejects efibootmgr's full-device-path construction.
    # Use the same GPT HD()+File() form as our working boot entries.
    # A matching staged entry can be resumed before DriverOrder was activated.
    driver_after=driver_before if 'Companion Extension 01\t' in driver_before else run("set -eu; efibootmgr --driver --create-only --disk /dev/nvme0n1 --part 1 --label 'Companion Extension 01' --loader '\\EFI\\companion\\companionextx64.efi'; efibootmgr --driver -v")
    if order(driver_after,'DriverOrder')!=driver_order:raise SystemExit('Unexpected driver order mutation')
    driver_entry=entry(driver_after,'Driver','Companion Extension 01','\\EFI\\companion\\companionextx64.efi')
    boot_after=boot_before if 'Companion Extension Probe 01\t' in boot_before else run("set -eu; efibootmgr --create-only --disk /dev/nvme0n1 --part 1 --label 'Companion Extension Probe 01' --loader '\\EFI\\companion\\extensionprobex64.efi'; efibootmgr -v")
    if order(boot_after,'BootOrder')!=normal_order:raise SystemExit('Unexpected normal boot order mutation')
    observer_entry=entry(boot_after,'Boot','Companion Extension Probe 01','\\EFI\\companion\\extensionprobex64.efi')
    metadata={'scope':__doc__,'driver_sha256':driver_hash,'observer_sha256':observer_hash,
              'driver_entry':driver_entry,'observer_entry':observer_entry,'original_driver_order':driver_order,
              'active_driver_order':driver_order+','+driver_entry,'normal_boot_order':normal_order,
              'previous_boot_id':boot_id,'verified_runs':[]}
    save(metadata)
    result=run('set -eu; efibootmgr --driver --bootorder '+metadata['active_driver_order']+'; efibootmgr --driver -v')
    if order(result,'DriverOrder')!=metadata['active_driver_order']:raise SystemExit('Driver order write not accepted')
    (root/'extension-driver-before.txt').write_text(driver_before,encoding='utf-8')
    (root/'extension-boot-before.txt').write_text(boot_before,encoding='utf-8')
    reboot(metadata)
else:
    metadata=json.loads(metadata_path.read_text())
    for key in ['driver_entry','observer_entry']:
        if not re.fullmatch(r'[0-9A-Fa-f]{4}',metadata[key]):raise SystemExit('Invalid saved entry')
    for key in ['original_driver_order','active_driver_order','normal_boot_order']:
        if not re.fullmatch(r'[0-9A-Fa-f]{4}(,[0-9A-Fa-f]{4})*',metadata[key]):
            raise SystemExit('Invalid saved order')
    driver_now=run('efibootmgr --driver -v');boot_now=run('efibootmgr -v')
    if entry(driver_now,'Driver','Companion Extension 01','\\EFI\\companion\\companionextx64.efi')!=metadata['driver_entry']:
        raise SystemExit('Driver entry changed')
    if order(driver_now,'DriverOrder')!=metadata['active_driver_order']:raise SystemExit('Driver order changed')
    if args.action=='uninstall':
        # Remove only our validated registration, retaining the original Dell entry.
        run('set -eu; efibootmgr --driver --bootorder '+metadata['original_driver_order']+'; '
            'efibootmgr --driver --bootnum '+metadata['driver_entry']+' --delete-bootnum')
        if any(line.startswith('Boot'+metadata['observer_entry']) for line in boot_now.splitlines()):
            if entry(boot_now,'Boot','Companion Extension Probe 01','\\EFI\\companion\\extensionprobex64.efi')!=metadata['observer_entry']:
                raise SystemExit('Observer differs; driver removed but observer left intact')
            run('set -eu; efibootmgr --bootnum '+metadata['observer_entry']+' --delete-bootnum; '
                'efibootmgr --bootorder '+metadata['normal_boot_order'])
        final=run('efibootmgr --driver -v')
        if order(final,'DriverOrder')!=metadata['original_driver_order']:
            raise SystemExit('Driver order rollback failed')
        metadata['owner_extension_uninstalled']=True;save(metadata)
        print(final)
        raise SystemExit(0)
    observer_present=any(line.startswith('Boot'+metadata['observer_entry']) for line in boot_now.splitlines())
    if observer_present:
        if entry(boot_now,'Boot','Companion Extension Probe 01','\\EFI\\companion\\extensionprobex64.efi')!=metadata['observer_entry']:
            raise SystemExit('Observer entry changed')
    elif args.action!='finalize':
        raise SystemExit('Observer entry absent')
    if args.action=='verify':
        if boot_id==metadata['previous_boot_id'] or any(r['boot_id']==boot_id for r in metadata['verified_runs']):
            raise SystemExit('No new boot to verify')
        data=run('cat /boot/efi/EFI/companion/extension-probe-01.txt')
        if not data.startswith('COMPANION_EXTENSION_PROBE_01\n') or not data.endswith('PROBE_COMPLETE\n'):
            raise SystemExit('Incomplete observation')
        filename='extension-observation-'+str(len(metadata['verified_runs'])+1)+'.txt'
        (root/filename).write_text(data,encoding='utf-8')
        required=['EXTENSION_LOCATE status=0x0000000000000000',
                  'EXTENSION_INFO status=0x0000000000000000 bytes=0x0000000000000020 magic=0x314458454d504f43 revision=0x0000000000000001',
                  'capabilities=0x0000000000000001']
        loaded=all(s in data for s in required)
        result={'boot_id':boot_id,'report':filename,'report_sha256':hashlib.sha256(data.encode()).hexdigest(),
                'loaded_by_firmware_before_observer':loaded,'healthy_root_reconnected':True}
        metadata['verified_runs'].append(result);save(metadata)
        if not loaded:
            run('set -eu; efibootmgr --driver --bootorder '+metadata['original_driver_order']+'; '
                'efibootmgr --driver --bootnum '+metadata['driver_entry']+' --delete-bootnum; '
                'efibootmgr --bootnum '+metadata['observer_entry']+' --delete-bootnum; '
                'efibootmgr --bootorder '+metadata['normal_boot_order'])
            metadata['failed_extension_registration_removed']=True;save(metadata)
            raise SystemExit('Physical observation did not find extension; own registration removed; report saved')
        print(json.dumps(result,indent=2))
    elif args.action=='retest':
        if len(metadata['verified_runs'])!=1 or metadata['verified_runs'][-1]['boot_id']!=boot_id:
            raise SystemExit('Verify first boot before retesting')
        # Preserve the first ESP report before allowing the observer to write a fresh one.
        run('set -eu; test ! -e /boot/efi/EFI/companion/extension-probe-first.txt; '
            'mv /boot/efi/EFI/companion/extension-probe-01.txt /boot/efi/EFI/companion/extension-probe-first.txt; sync')
        metadata['previous_boot_id']=boot_id;reboot(metadata)
    elif args.action=='finalize':
        if len(metadata['verified_runs'])!=2 or metadata['verified_runs'][-1]['boot_id']!=boot_id:
            raise SystemExit('Two healthy observed boots required')
        delete='efibootmgr --quiet --bootnum '+metadata['observer_entry']+' --delete-bootnum; ' if observer_present else ''
        result=run('set -eu; '+delete+'efibootmgr --quiet --bootorder '+metadata['normal_boot_order']+'; efibootmgr -v; efibootmgr --driver -v')
        if 'BootNext:' in result or order(result,'BootOrder')!=metadata['normal_boot_order'] or order(result,'DriverOrder')!=metadata['active_driver_order']:
            raise SystemExit('Final boot configuration mismatch')
        metadata['persistent_driver_registration_verified']=True;metadata['temporary_observer_entry_removed']=True
        metadata['motherboard_executable_flash_installation']=False;save(metadata)
        print(json.dumps(metadata,indent=2))

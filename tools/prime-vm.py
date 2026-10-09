"""Dedicated Heurism development VM; never imports or contacts the Dell helper.

Uses the owner's already pinned primeserver/prime-linux SSH aliases. Guest keys
are separate from Dell keys. Hyper-V operations are restricted to CompanionDev.
"""
import argparse
import base64
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import time

REPO = Path(__file__).resolve().parents[1]
OUT = REPO/'build/prime-vm'
KEYS = REPO/'artifacts/prime-vm'
TARGET = KEYS/'target.json'
NAME = 'CompanionDev'
VM_DIR = r'D:\HyperV\CompanionDev'
GUEST = '172.28.50.3'


def call(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def host(script):
    encoded = base64.b64encode(("$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue';\n"+script).encode('utf-16le')).decode()
    result = subprocess.run(['ssh', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=8', 'primeserver',
                   'powershell', '-NoProfile', '-NonInteractive', '-EncodedCommand', encoded],
                  capture_output=True, text=True, timeout=60)
    if result.returncode:
        raise RuntimeError(result.stderr[:4000] or result.stdout[:4000] or 'PrimeServer command failed')
    return result.stdout.strip()


def guest_options():
    return ['-o', 'BatchMode=yes', '-o', 'ConnectTimeout=20', '-o', 'StrictHostKeyChecking=yes',
            '-o', 'HostKeyAlias=companion-prime-vm', '-o', 'UserKnownHostsFile='+str(KEYS/'known_hosts'),
            '-o', 'IdentitiesOnly=yes', '-i', str(KEYS/'client_ed25519'), '-J', 'prime-linux']


def guest(command):
    return call(['ssh', *guest_options(), 'root@'+GUEST, command])


def wait_ready(after_boot):
    check_target()
    deadline = time.monotonic()+90
    command = ("if test -x /opt/heurism/native/current/heurism-release; then "
               "/opt/heurism/native/current/heurism-release verify >/dev/null && "
               "/opt/heurism/native/current/heurism-release health && "
               "/opt/heurism/native/current/heurismctl status; "
               "elif test -x /opt/companion/native/current/companion-release; then "
               "/opt/companion/native/current/companion-release verify >/dev/null && "
               "/opt/companion/native/current/companion-release health && "
               "/opt/companion/native/current/companionctl status; "
               "else python3 -c 'import sys; sys.path.insert(0, \"/opt/companion/desktop\"); "
               "import release, system_actions; release.verify(release.LINK); "
               "system_actions.verify_boot(); release.health()'; fi")
    while time.monotonic() < deadline:
        try:
            result = subprocess.run(['ssh', *guest_options(), 'root@'+GUEST, command],
                                    capture_output=True, text=True, timeout=15)
        except subprocess.TimeoutExpired:
            time.sleep(2)
            continue
        if result.returncode == 0:
            lines = [json.loads(line) for line in result.stdout.splitlines() if line.strip()]
            value = lines[0]
            native = len(lines) == 2
            management = lines[1].get('data', {}).get('management', {}) if native else {}
            if ((not native or (lines[1].get('ok') and
                                all(management.get(key) for key in ('ssh', 'watch', 'boot_healthy'))))
                    and (not after_boot or value['boot_id'] != after_boot)):
                print(json.dumps(value))
                return
        time.sleep(2)
    raise TimeoutError('VM management and desktop did not become ready; inspect the host console')


def check_target():
    value = json.loads(TARGET.read_text())
    # Bind host-level reset/restore to the VM created by this tool, not its name alone.
    actual = json.loads(host(f"Get-VM -Name '{NAME}' | Select-Object Name,Id,Path,State | ConvertTo-Json -Compress"))
    if str(actual['Id']).lower() != value['id'].lower() or actual['Name'] != NAME:
        raise ValueError('Companion VM identity changed')
    return actual


def source_archive():
    OUT.mkdir(parents=True, exist_ok=True)
    archive = OUT/'desktop.tar.gz'
    paths = [p for p in (REPO/'userspace').iterdir() if p.suffix in ('.py', '.sh', '.xml', '.html', '.initd')]
    paths += list((REPO/'tests').glob('desktop*.py'))
    with tarfile.open(archive, 'w:gz') as stream:
        for path in paths:
            info = stream.gettarinfo(str(path), arcname=path.name)
            info.uid = info.gid = 0
            data = path.read_bytes().replace(b'\r\n', b'\n')
            import io
            info.size = len(data)
            stream.addfile(info, io.BytesIO(data))
    return archive


def build():
    KEYS.mkdir(parents=True, exist_ok=True)
    key = KEYS/'client_ed25519'
    if not key.exists():
        call(['ssh-keygen', '-q', '-t', 'ed25519', '-N', '', '-C', 'companion-prime-vm', '-f', str(key)])
    archive = source_archive()
    stage = '/var/lib/companion-vm-build/'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    call(['ssh', 'prime-linux', 'sudo -n mkdir -p '+stage+' && sudo -n chown ubuntu:ubuntu '+stage])
    for source, name in [(archive, 'desktop.tar.gz'), (key.with_suffix('.pub'), 'client.pub'),
                         (REPO/'tools/build-hyperv-guest.sh', 'build.sh'),
                         (REPO/'platform/hyperv/companion-watch', 'companion-watch'),
                         (REPO/'platform/hyperv/watch.initd', 'watch.initd')]:
        call(['scp', str(source), 'prime-linux:'+stage+'/'+name])
    (OUT/'builder.json').write_text(json.dumps({'stage': stage, 'source_sha256': hashlib.sha256(archive.read_bytes()).hexdigest()}, indent=2))
    call(['ssh', 'prime-linux', 'sudo -n bash '+stage+'/build.sh '+stage], timeout=1200)
    # Building a candidate must not replace the running VM's trusted host pin.
    call(['scp', 'prime-linux:'+stage+'/guest-host.pub', str(OUT/'image-host.pub')])
    call(['scp', 'prime-linux:'+stage+'/companion-dev.vhdx', str(OUT/'companion-dev.vhdx')], timeout=600)
    print('Fresh guest image built; candidate public identity saved. No physical target contacted.')


def pin_image_host():
    fields = (OUT/'image-host.pub').read_text().split()
    if len(fields) < 2 or fields[0] != 'ssh-ed25519':
        raise ValueError('Invalid candidate SSH public identity')
    KEYS.mkdir(parents=True, exist_ok=True)
    (KEYS/'host.pub').write_text(' '.join(fields)+'\n')
    (KEYS/'known_hosts').write_text('companion-prime-vm '+' '.join(fields[:2])+'\n')


def publish():
    check_target()
    native = subprocess.run(['ssh', *guest_options(), 'root@'+GUEST,
                             'test -x /opt/companion/native/current/companion-release'],
                            capture_output=True, timeout=15)
    if native.returncode == 0:
        raise RuntimeError('Legacy Python/Tk publish is disabled while the native C runtime is active')
    # Require working management and a verified previous release before iteration.
    guest('python3 /usr/local/sbin/companion-release verify && python3 /usr/local/sbin/companion-release health')
    archive = source_archive()
    remote = '/var/lib/companion/desktop-stage/desktop.tar.gz'
    call(['scp', *guest_options(), str(archive), 'root@'+GUEST+':'+remote], timeout=60)
    digest = hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest()
    guest(f"echo '{digest}  {remote}' | sha256sum -c - && sh /var/lib/companion/desktop-stage/source/install.sh && rc-service companion-desktop stop && rc-service companion-control restart && rc-service companion-desktop start")


def capture(name, surface='root', require_ui=False):
    import re
    import struct
    from PIL import Image
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,60}', name):
        raise ValueError('Invalid screenshot name')
    check_target()
    remote = '/var/lib/companion/desktop-stage/evidence/'+name+'.xwd'
    selection = '-root' if surface == 'root' else '-id "$(xdotool search --onlyvisible --name ^\\(Heurism\\|Companion\\)$ | tail -1)"'
    guest('mkdir -p /var/lib/companion/desktop-stage/evidence; export DISPLAY=:0; '
          'if test -r /run/heurism-desktop/Xauthority; then '
          'export XAUTHORITY=/run/heurism-desktop/Xauthority; '
          'else export XAUTHORITY=/run/companion-desktop/Xauthority; fi; '
          'xwd '+selection+' -silent -out '+remote)
    local = OUT/(name+'.xwd')
    call(['scp', *guest_options(), 'root@'+GUEST+':'+remote, str(local)], timeout=60)
    raw = local.read_bytes()
    header = struct.unpack('>25I', raw[:100])
    size, version, format_, depth, width, height, offset, order = header[:8]
    bits, stride, visual, red, green, blue, _, _, colors = header[11:20]
    if not (version == 7 and format_ == 2 and depth == 24 and bits in (24, 32) and order == 0
            and (red, green, blue) == (0xff0000, 0xff00, 0xff) and offset == 0
            and 0 < width <= 4096 and 0 < height <= 4096 and stride >= width*(bits//8)):
        raise ValueError('Unexpected VM XWD format')
    pixels = raw[size+colors*12:]
    if len(pixels) != stride*height:
        raise ValueError('Invalid XWD payload length')
    image = Image.frombytes('RGB', (width, height), pixels, 'raw', 'BGRX' if bits == 32 else 'BGR', stride)
    image.save(OUT/(name+'.png'))
    if require_ui:
        colors = image.getcolors(width*height)
        nonblack = sum(count for count, rgb in colors if rgb != (0, 0, 0))
        if len(colors) < 8 or nonblack < width*height//5:
            raise ValueError('The running VM has no visible desktop frame')
        (OUT/(name+'-frame.json')).write_text(json.dumps({'visible_frame': True,
            'width': width, 'height': height, 'colors': len(colors), 'nonblack_pixels': nonblack,
            'source': 'actual guest X framebuffer'}, indent=2))
    print(OUT/(name+'.png'))


def console():
    """Hyper-V framebuffer readback remains available without guest SSH."""
    check_target()
    result = host(f"""
$vm = Get-VM -Name '{NAME}'
$video = Get-WmiObject -Namespace root/virtualization/v2 -Class Msvm_VideoHead | Where-Object SystemName -eq $vm.Id.ToString()
$settings = Get-WmiObject -Namespace root/virtualization/v2 -Class Msvm_VirtualSystemSettingData | Where-Object {{ $_.VirtualSystemIdentifier -eq $vm.Id.ToString() -and $_.VirtualSystemType -eq 'Microsoft:Hyper-V:System:Realized' }}
$service = Get-WmiObject -Namespace root/virtualization/v2 -Class Msvm_VirtualSystemManagementService
$params = $service.GetMethodParameters('GetVirtualSystemThumbnailImage')
$params.TargetSystem = $settings.__PATH
$params.WidthPixels = $video.CurrentHorizontalResolution
$params.HeightPixels = $video.CurrentVerticalResolution
$result = $service.InvokeMethod('GetVirtualSystemThumbnailImage', $params, $null)
if ($result.ReturnValue -ne 0) {{ throw ('Console capture failed: '+$result.ReturnValue) }}
@{{width=$params.WidthPixels; height=$params.HeightPixels; data=[Convert]::ToBase64String($result.ImageData)}} | ConvertTo-Json -Compress
""")
    data = json.loads(result)
    width, height = data['width'], data['height']
    raw = base64.b64decode(data['data'])
    from PIL import Image
    if len(raw) == width*height*2+4:
        # This provider prefixes the RGB565 plane with its big-endian total size.
        if int.from_bytes(raw[:4], 'big') == len(raw):
            raw = raw[4:]
    if not (0 < width <= 4096 and 0 < height <= 4096) or len(raw) != width*height*2:
        raise ValueError(f'Unexpected Hyper-V thumbnail dimensions: {width}x{height}, {len(raw)} bytes, prefix {raw[:12].hex()}, suffix {raw[-8:].hex()}')
    pixels = bytearray()
    for pos in range(0, len(raw), 2):
        value = int.from_bytes(raw[pos:pos+2], 'little')
        pixels.extend((((value >> 11) & 31)*255//31, ((value >> 5) & 63)*255//63, (value & 31)*255//31))
    Image.frombytes('RGB', (width, height), bytes(pixels)).save(OUT/'console.png')
    print(OUT/'console.png')


def create():
    disk = OUT/'companion-dev.vhdx'
    digest = hashlib.file_digest(disk.open('rb'), 'sha256').hexdigest()
    host(f"if (Get-VM -Name '{NAME}' -ErrorAction SilentlyContinue) {{ throw 'VM already exists' }}; "
         f"if (Test-Path -LiteralPath '{VM_DIR}') {{ throw 'VM directory already exists' }}; "
         f"New-Item -ItemType Directory -Path '{VM_DIR}' | Out-Null")
    call(['scp', str(disk), 'primeserver:D:/HyperV/CompanionDev/companion-dev.vhdx'], timeout=600)
    result = host(f"""
if ((Get-FileHash -LiteralPath '{VM_DIR}\\companion-dev.vhdx' -Algorithm SHA256).Hash -ne '{digest}') {{ throw 'Disk transfer hash mismatch' }}
$vm = New-VM -Name '{NAME}' -Generation 2 -MemoryStartupBytes 4GB -Path '{VM_DIR}' -VHDPath '{VM_DIR}\\companion-dev.vhdx' -SwitchName 'PrimeLinuxInternal'
Set-VMProcessor -VM $vm -Count 4
Set-VMMemory -VM $vm -DynamicMemoryEnabled $false
Set-VM -VM $vm -AutomaticStartAction Start -AutomaticStartDelay 20 -AutomaticStopAction ShutDown -CheckpointType Standard
Set-VMFirmware -VM $vm -EnableSecureBoot Off -FirstBootDevice (Get-VMHardDiskDrive -VM $vm)
Set-VMVideo -VMName '{NAME}' -HorizontalResolution 1280 -VerticalResolution 800 -ResolutionType Single
@{{name=$vm.Name; id=$vm.Id.ToString(); path=$vm.Path; disk_sha256='{digest}'; address='{GUEST}'}} | ConvertTo-Json -Compress
""")
    TARGET.write_text(json.dumps(json.loads(result), indent=2))
    pin_image_host()
    print(host(f"Start-VM -Name '{NAME}'; Get-VM -Name '{NAME}' | Select-Object Name,State,Id | ConvertTo-Json -Compress"))


def replace_disk():
    check_target()
    disk = OUT/'companion-dev.vhdx'
    digest = hashlib.file_digest(disk.open('rb'), 'sha256').hexdigest()
    call(['scp', str(disk), 'primeserver:D:/HyperV/CompanionDev/incoming.vhdx'], timeout=600)
    backup = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-boot-failed.vhdx'
    result = host(f"""
if ((Get-FileHash -LiteralPath '{VM_DIR}\\incoming.vhdx').Hash -ne '{digest}') {{ throw 'Disk hash mismatch' }}
$disk = Get-VMHardDiskDrive -VMName '{NAME}'
if ($disk.Count -ne 1 -or $disk.Path -ne '{VM_DIR}\\companion-dev.vhdx') {{ throw 'Unexpected VM disk' }}
Stop-VM -Name '{NAME}' -TurnOff -Force
Move-Item -LiteralPath '{VM_DIR}\\companion-dev.vhdx' -Destination '{VM_DIR}\\{backup}'
Move-Item -LiteralPath '{VM_DIR}\\incoming.vhdx' -Destination '{VM_DIR}\\companion-dev.vhdx'
& icacls.exe '{VM_DIR}\\companion-dev.vhdx' /grant ('NT VIRTUAL MACHINE\\'+(Get-VM -Name '{NAME}').Id.ToString()+':(F)') | Out-Null
if ($LASTEXITCODE -ne 0) {{ throw 'VM disk access repair failed' }}
Start-VM -Name '{NAME}'
Get-VM -Name '{NAME}' | Select-Object Name,State | ConvertTo-Json -Compress
""")
    pin_image_host()
    metadata = json.loads(TARGET.read_text())
    metadata.update(last_staged_disk_sha256=digest, previous_disk=backup)
    TARGET.write_text(json.dumps(metadata, indent=2))
    print(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['inventory', 'build', 'create', 'replace-disk', 'start', 'status', 'guest', 'wait', 'publish', 'capture', 'console', 'checkpoint', 'reset', 'restore'])
    parser.add_argument('--command')
    parser.add_argument('--snapshot', default='Companion-C-runtime-ready')
    parser.add_argument('--name', default='heurism-vm')
    parser.add_argument('--surface', choices=['root', 'shell'], default='root')
    parser.add_argument('--after-boot')
    parser.add_argument('--require-ui', action='store_true')
    args = parser.parse_args()
    if args.action == 'inventory':
        print(host("@{vm=@(Get-VM | Select-Object Name,State,Generation,MemoryAssigned,Path); switches=@(Get-VMSwitch | Select-Object Name,SwitchType); nat=@(Get-NetNat | Select-Object Name,InternalIPInterfaceAddressPrefix)} | ConvertTo-Json -Depth 4 -Compress"))
    elif args.action == 'build':
        build()
    elif args.action == 'create':
        create()
    elif args.action == 'replace-disk':
        replace_disk()
    elif args.action == 'publish':
        publish()
    elif args.action == 'capture':
        capture(args.name, args.surface, args.require_ui)
    elif args.action == 'console':
        console()
    elif args.action == 'wait':
        wait_ready(args.after_boot)
    elif args.action == 'guest':
        if not args.command:
            parser.error('guest requires --command')
        check_target()
        guest(args.command)
    else:
        value = check_target()
        if args.action == 'status':
            print(json.dumps(value, indent=2))
        elif args.action == 'start':
            print(host(f"& icacls.exe '{VM_DIR}\\companion-dev.vhdx' /grant ('NT VIRTUAL MACHINE\\'+(Get-VM -Name '{NAME}').Id.ToString()+':(F)') | Out-Null; if ($LASTEXITCODE -ne 0) {{ throw 'VM disk access repair failed' }}; Start-VM -Name '{NAME}'; Get-VM -Name '{NAME}' | Select-Object Name,State | ConvertTo-Json -Compress"))
        elif args.action == 'reset':
            print(host(f"Restart-VM -Name '{NAME}' -Force; Get-VM -Name '{NAME}' | Select-Object Name,State | ConvertTo-Json -Compress"))
        else:
            import re
            if not re.fullmatch(r'[A-Za-z0-9_-]{1,60}', args.snapshot):
                raise ValueError('Invalid checkpoint name')
            if args.action == 'checkpoint':
                print(host(f"if (Get-VMSnapshot -VMName '{NAME}' -Name '{args.snapshot}' -ErrorAction SilentlyContinue) {{ throw 'Checkpoint name already exists' }}; Checkpoint-VM -Name '{NAME}' -SnapshotName '{args.snapshot}'; Get-VMSnapshot -VMName '{NAME}' -Name '{args.snapshot}' | Select-Object Name,Id,CreationTime | ConvertTo-Json -Compress"))
            else:
                print(host(f"$s=@(Get-VMSnapshot -VMName '{NAME}' -Name '{args.snapshot}'); if ($s.Count -ne 1) {{ throw 'Checkpoint must be unique' }}; $s[0] | Restore-VMSnapshot -Confirm:$false; Start-VM -Name '{NAME}' -ErrorAction SilentlyContinue; Get-VM -Name '{NAME}' | Select-Object Name,State | ConvertTo-Json -Compress"))


if __name__ == '__main__':
    main()

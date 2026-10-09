"""Build and verify native Companion startup and bounded failure paths offline."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

repo=Path(__file__).resolve().parents[1]
def run(*args):subprocess.run([sys.executable,*map(str,args)],cwd=repo,check=True)
run('tools/build-kernel.py','--vm-diagnostics')
normal=(repo/'build/native/manifest.json').read_bytes()
run('tests/native_host.py')
run('tests/native_smoke.py')
run('tests/native_smoke.py','--reject','missing')
run('tests/native_smoke.py','--reject','corrupt')
for name,option in [('stale','--stale-map-test'),('exception','--exception-test'),('pagefault','--pagefault-test')]:
    out=repo/('build/native-'+name)
    run('tools/build-kernel.py','--vm-diagnostics','--out',out,option)
    run('tests/native_host.py','--build',out)
    run('tests/native_smoke.py','--build',out)
out=repo/'build/native-noinput'
run('tools/build-kernel.py','--vm-diagnostics','--keyboard','none','--out',out)
run('tests/native_host.py','--build',out)
run('tests/native_smoke.py','--build',out)
# Same inputs/toolchain must give byte-identical normal binary artifacts.
run('tools/build-kernel.py','--vm-diagnostics')
assert (repo/'build/native/manifest.json').read_bytes()==normal,'Non-reproducible native build'
network_out=repo/'build/native-network'
run('tools/build-kernel.py','--vm-diagnostics','--vm-network','--out',network_out)
network_manifest=(network_out/'manifest.json').read_bytes()
run('tests/native_host.py','--build',network_out)
run('tests/network_host.py','--build',network_out)
run('tests/native_network_smoke.py','--build',network_out)
run('tools/build-kernel.py','--vm-diagnostics','--vm-network','--out',network_out)
assert (network_out/'manifest.json').read_bytes()==network_manifest,'Non-reproducible network build'
usb_out=repo/'build/native-usb'
run('tools/build-kernel.py','--vm-diagnostics','--vm-usb','--out',usb_out)
usb_manifest=(usb_out/'manifest.json').read_bytes()
run('tests/native_host.py','--build',usb_out)
run('tests/usb_host.py','--build',usb_out)
for case in ['devices','empty','absent','highbar']:run('tests/native_usb_smoke.py','--build',usb_out,'--case',case)
run('tools/build-kernel.py','--vm-diagnostics','--vm-usb','--out',usb_out)
assert (usb_out/'manifest.json').read_bytes()==usb_manifest,'Non-reproducible USB build'
usb_network_out=repo/'build/native-usb-network'
run('tools/build-kernel.py','--vm-diagnostics','--vm-usb','--vm-usb-network','--out',usb_network_out)
usb_network_manifest=(usb_network_out/'manifest.json').read_bytes()
run('tests/native_host.py','--build',usb_network_out)
run('tests/usb_host.py','--build',usb_network_out)
run('tests/native_network_smoke.py','--transport','usb_ecm','--build',usb_network_out)
run('tools/build-kernel.py','--vm-diagnostics','--vm-usb','--vm-usb-network','--out',usb_network_out)
assert (usb_network_out/'manifest.json').read_bytes()==usb_network_manifest,'Non-reproducible USB network build'
# A production build must contain neither the public fixture credential nor
# the VM-only network/reset path. This is a build property, not hardware proof.
production=repo/'build/native-production'
run('tools/build-kernel.py','--out',production)
run('tests/native_host.py','--build',production)
for name in ['kernel.elf','companion-loader.efi']:
    data=(production/name).read_bytes()
    assert bytes(range(32)) not in data and b'KERNEL_NETWORK_READY' not in data and b'KERNEL_AUTHENTICATED_REBOOT' not in data and b'KERNEL_USB_READY' not in data
bad=subprocess.run([sys.executable,'-O','tools/build-kernel.py','--vm-network','--out',str(repo/'build/native-invalid')],cwd=repo,capture_output=True,text=True)
assert bad.returncode and 'VM-only' in bad.stderr
bad_usb=subprocess.run([sys.executable,'-O','tools/build-kernel.py','--vm-usb','--out',str(repo/'build/native-invalid-usb')],cwd=repo,capture_output=True,text=True)
assert bad_usb.returncode and 'VM-only' in bad_usb.stderr
for options in [['--vm-usb-network'],['--vm-diagnostics','--vm-usb-network'],['--vm-diagnostics','--vm-usb','--vm-network','--vm-usb-network']]:
    rejected=subprocess.run([sys.executable,'-O','tools/build-kernel.py',*options,'--out',str(repo/'build/native-invalid-usb-network')],cwd=repo,capture_output=True,text=True)
    assert rejected.returncode and 'VM-only' in rejected.stderr
reports=[repo/'build/native/vm-normal/verification.json',repo/'build/native/vm-missing/verification.json',repo/'build/native/vm-corrupt/verification.json',repo/'build/native-stale/vm-stale-map/verification.json',repo/'build/native-exception/vm-exception/verification.json',repo/'build/native-pagefault/vm-pagefault/verification.json',repo/'build/native-noinput/vm-normal/verification.json']
reports.append(network_out/'vm-network/verification.json')
reports.extend(usb_out/('vm-usb-'+case)/'verification.json' for case in ['devices','empty','absent','highbar'])
reports.append(usb_network_out/'vm-usb-network/verification.json')
result={'native_vm_cases_passed':len(reports),'deterministic_build_passed':True,'network_deterministic_build_passed':True,'usb_deterministic_build_passed':True,'usb_network_deterministic_build_passed':True,'production_fixture_excluded':True,'profile_guard_passed':True,'usb_profile_guard_passed':True,'usb_network_profile_guard_passed':True,'reports':{str(p.relative_to(repo)):hashlib.sha256(p.read_bytes()).hexdigest() for p in reports},'network_host_report_sha256':hashlib.sha256((network_out/'network-host-verification.json').read_bytes()).hexdigest(),'usb_host_report_sha256':hashlib.sha256((usb_out/'usb-host-verification.json').read_bytes()).hexdigest(),'usb_network_host_report_sha256':hashlib.sha256((usb_network_out/'usb-host-verification.json').read_bytes()).hexdigest(),'scope':'VM and host only; physical native management not implemented'}
(repo/'build/native/acceptance.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))

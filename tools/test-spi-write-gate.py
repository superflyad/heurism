"""Compile controls, then issue the fixed unchanged-padding SPI write-gate test."""
import hashlib
import json
from pathlib import Path
import subprocess

repo=Path(__file__).resolve().parents[1]
rom=(repo/'artifacts/firmware/bios16-a.bin').read_bytes()
if hashlib.sha256(rom).hexdigest()!='09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c':
    raise SystemExit('Baseline ROM changed')
if rom[0x111000:0x111040]!=b'\xff'*64:raise SystemExit('Selected baseline padding not empty')
destination=repo/'artifacts/firmware/spi-write-gate-live.json'
if destination.exists():raise SystemExit('Physical test already recorded; no repeat issued')
state=json.loads((repo/'artifacts/targets/dell.json').read_text())
ssh=['ssh','-i',str(repo/'artifacts/ssh/companion_client_ed25519'),'-o','BatchMode=yes',
     '-o','StrictHostKeyChecking=yes','-o','HostKeyAlias=companion-dell',
     '-o','UserKnownHostsFile='+str(repo/'artifacts/ssh/known_hosts'),'root@'+state['address']]
source=(repo/'tools/target/companion-spi-write-gate.c').read_bytes()
command=r'''set -eu
test "$(cat /sys/class/dmi/id/board_name)" = 0VK62X
test "$(cat /var/lib/companion/healthy-boot-id)" = "$(cat /proc/sys/kernel/random/boot_id)"
umask 077
cat > /etc/companion/tools/companion-spi-write-gate.c
gcc -std=c11 -O2 -Wall -Wextra -Werror -DGATE_HOST_TEST /etc/companion/tools/companion-spi-write-gate.c -o /etc/companion/tools/spi-gate-controls
/etc/companion/tools/spi-gate-controls >&2
gcc -std=c11 -O2 -Wall -Wextra -Werror /etc/companion/tools/companion-spi-write-gate.c -o /etc/companion/tools/companion-spi-write-gate
/etc/companion/tools/companion-spi-write-gate
'''
result=subprocess.run(ssh+[command],input=source,capture_output=True,timeout=30)
if result.returncode:raise SystemExit(result.stderr.decode(errors='replace')+result.stdout.decode(errors='replace'))
report=json.loads(result.stdout)
report.update({'source_sha256':hashlib.sha256(source).hexdigest(),'boot_id':state['boot_id'],
               'controls':result.stderr.decode(errors='replace').strip(),
               'write_permission_proven':False})
destination.write_text(json.dumps(report,indent=2),encoding='utf-8')
if not report['contents_unchanged']:raise SystemExit('Unexpected contents changed')
print(json.dumps(report,indent=2))

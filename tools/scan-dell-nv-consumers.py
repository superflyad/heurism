"""Read-only exact-reference scan of the pinned Dell firmware PE dump."""

import base64
import hashlib
import json
import subprocess
import sys
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
state = json.loads((repo / "artifacts/targets/dell.json").read_text())
identity = repo / "artifacts/ssh"
remote = r'''
import base64, hashlib, json, pathlib, subprocess, uuid

rom = pathlib.Path('/var/lib/companion/firmware/bios16-a.bin')
expected = '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
assert hashlib.sha256(rom.read_bytes()).hexdigest() == expected
assert pathlib.Path('/etc/hostname').read_text().strip() == 'companion-dell'
root = pathlib.Path('/var/lib/companion/firmware/bios16-a.bin.dump')
patterns = {
    'owner_guid': uuid.UUID('1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1').bytes_le,
    'owner_name_utf16': ('CompanionExtensionImage01' + chr(0)).encode('utf-16-le'),
    'dell_menu_guid': uuid.UUID('5990c250-676b-4ff7-8a0d-529319d0b254').bytes_le,
    'special_boot_file_guid': uuid.UUID('6b287864-759c-42c4-b435-a74ab694cd3b').bytes_le,
    'internal_shell_file_guid': uuid.UUID('c57ad6b7-0515-40a8-9d21-551652854e37').bytes_le,
    'shell_gate_protocol_guid': uuid.UUID('f2feff56-a85b-4489-9099-2d54411dfc6d').bytes_le,
    'platform_recovery_protocol_guid': uuid.UUID('919383de-ebac-4924-0194-5259e00d657a').bytes_le,
}
files = sorted(p for p in root.rglob('body.bin')
               if p.parent.name.endswith('PE32 image section'))
assert len(files) >= 600, 'Expected Dell PE image sections were not found'
hits = {key: [] for key in patterns}
digests = set()
for path in files:
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    digests.add(digest)
    for key, needle in patterns.items():
        if needle in data:
            hits[key].append({'path': str(path.relative_to(root)),
                              'bytes': len(data), 'sha256': digest})
bbs_file = next(p for p in files if 'BBSManagerDxe/' in str(p))
bbs_disassembly = subprocess.run(['objdump', '-d', str(bbs_file)],
                                 check=True, capture_output=True).stdout
print(json.dumps({'rom_sha256': expected, 'pe_sections_scanned': len(files),
                  'unique_pe_sha256': len(digests), 'exact_reference_hits': hits,
                  'internal_shell_dump_paths': [str(p.relative_to(root)) for p in
                      files if 'C57AD6B7-0515-40A8-9D21-551652854E37' in str(p)],
                  'bbs_base64': base64.b64encode(bbs_file.read_bytes()).decode(),
                  'bbs_disassembly_base64': base64.b64encode(bbs_disassembly).decode()}))
'''
ssh = [
    "ssh", "-i", str(identity / "companion_client_ed25519"),
    "-o", "BatchMode=yes", "-o", "ConnectTimeout=3",
    "-o", "StrictHostKeyChecking=yes", "-o", "HostKeyAlias=companion-dell",
    "-o", "UserKnownHostsFile=" + str(identity / "known_hosts"),
    "root@" + state["address"], "python3 -",
]
result = subprocess.run(ssh, input=remote, text=True, capture_output=True,
                        timeout=120)
if result.returncode:
    sys.exit(result.stderr or f"SSH exit {result.returncode}")
report = json.loads(result.stdout)
bbs = base64.b64decode(report.pop("bbs_base64"))
bbs_disassembly = base64.b64decode(report.pop("bbs_disassembly_base64"))
assert hashlib.sha256(bbs).hexdigest() == (
    "c4ad0c1889767773c26c196d402c795999f1af4d3f4a8f18ca2c989250e0e570"
)
if "--extract-bbs" in sys.argv:
    out = repo / "artifacts/research/nv-dispatch/bbs-manager-dxe.bin"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(bbs)
    out.with_suffix(".txt").write_bytes(bbs_disassembly)
    report["local_extraction"] = str(out)
print(json.dumps(report, indent=2))

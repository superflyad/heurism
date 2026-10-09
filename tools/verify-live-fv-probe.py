"""Validate a downloaded physical FV inventory without touching the target."""
import hashlib
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
folder = root / 'artifacts/firmware'
path = folder / 'fv-probe-01.txt'
raw = path.read_bytes()
expected = 'c8a5de80bd5e6937c9c4c5b20f5b7f3b3cf92fa8af1c38b026e16c48a84686f6'
if hashlib.sha256(raw).hexdigest() != expected:
    raise SystemExit('Physical report changed; re-establish provenance before interpreting it')
lines = raw.decode('ascii').splitlines()
if lines[0] != 'COMPANION_FV_PROBE_01' or lines[-1] != 'PROBE_COMPLETE':
    raise SystemExit('Incomplete or unexpected probe report')

def fields(line):
    return {key: int(value, 16) for key, value in
            re.findall(r'(\w+)=(0x[0-9a-f]+)', line)}

locate = fields(next(line for line in lines if line.startswith('FV_LOCATE ')))
if locate['status'] != 0:
    raise SystemExit('FV enumeration failed')
volumes = []
for index, line in enumerate(lines):
    if not line.startswith('FV index='):
        continue
    fv = fields(line)
    block = fields(lines[index + 1])
    files = fields(lines[index + 2])
    if any(fv[key] for key in ('interface_status', 'attr_status')) or any(
            block[key] for key in ('interface_status', 'physical_status', 'attr_status', 'header_status')):
        raise SystemExit('Failed volume inspection')
    if block['header_bytes'] != 128 or files['stop_status'] != 0x800000000000000e:
        raise SystemExit('Incomplete volume header or file enumeration')
    name = re.search(r' name=([0-9a-f-]+)', lines[index + 1])
    volumes.append({'index': fv['index'], 'base': hex(block['base']),
                    'bytes': block['size'], 'name': name.group(1) if name else None,
                    'fv_attributes': hex(fv['attrs']), 'fvb_attributes': hex(block['attrs']),
                    'file_count': files['count']})
if len(volumes) != locate['count'] or [v['index'] for v in volumes] != list(range(locate['count'])):
    raise SystemExit('Missing or duplicated volume records')
candidate = [v for v in volumes if v['base'] == '0xff110000' or
             v['name'] == '8b570fe1-48bc-4830-92d5-244b1b93c2e4']
deployment = json.loads((folder / 'fv-probe-deployment.json').read_text())
state = json.loads((root / 'artifacts/targets/dell.json').read_text())
publication = json.loads((folder / 'fv-publication-emulation.json').read_text())
mode = int(next(line.split()[1] for line in lines if line.startswith('HOB_BOOT_MODE ')), 16)
mode_cases = [c for c in publication['cases'] if int(c['boot_mode_fixture'], 16) == mode]
if len(mode_cases) != 1:
    raise SystemExit('Missing original-code comparison for physical boot mode')
report = {'scope': 'Physical read-only FV2/FVB inventory, plus separately labelled offline publisher comparison.',
          'physical_report_sha256': expected, 'probe_binary_sha256': deployment['sha256'],
          'previous_boot_id': deployment['previous_boot_id'], 'management_boot_id': state['boot_id'],
          'volume_count': len(volumes), 'candidate_volume_matches': candidate,
          'hob_boot_mode': hex(mode), 'hob_walk_complete': 'HOB_BAD_RANGE' not in lines,
          'offline_same_mode_published_candidate': bool(mode_cases[0]['unsigned_volume_ppis']),
          'firmware_write_path_verified': False, 'volumes': volumes,
          'limitations': ['No flash program/erase or driver dispatch tested.',
                          'HOB walk rejected its bounds; authentication HOBs were not inspected.',
                          'Probe did not log chainload status; returned management used Boot0000 fallback.']}
(folder / 'fv-probe-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({k: v for k, v in report.items() if k != 'volumes'}, indent=2))

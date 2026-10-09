"""Decode saved HttpBootDxe dependencies and compare saved physical FV names.

Reads a previously extracted dependency section from the saved ROM on the Dell.
Does not schedule/dispatch a driver, invoke vendor code, change boot or reboot.
"""
import base64
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import uuid

repo = Path(__file__).resolve().parents[1]
out = repo / 'artifacts/research/independent-bootstrap'
path = out / 'http-boot-depex.bin'
if not path.exists():
    command = 'base64 /var/lib/companion/firmware/research/http-boot-depex-09bc0470/body.bin'
    p = subprocess.run([sys.executable, str(repo/'tools/dell.py'), '--command', command],
                       capture_output=True, text=True, timeout=45)
    if p.returncode:
        raise RuntimeError(p.stderr+p.stdout)
    path.write_bytes(base64.b64decode(''.join(p.stdout.split()), validate=True))
data = path.read_bytes()
assert 1 <= len(data) <= 4096
assert hashlib.sha256(data).hexdigest() == 'b73f346c1fb9a891c672d684f5d6eceb36ba1371c6f6f77bd2e6bbfa569a7cdf'
at = 0
stack = []
tokens = []
guids = []
sor = False
while at < len(data):
    offset = at; op = data[at]; at += 1
    if op == 9:
        assert offset == 0 and not sor
        sor = True; tokens.append({'offset': offset, 'opcode': 'SOR'})
    elif op == 2:
        assert at+16 <= len(data)
        guid = str(uuid.UUID(bytes_le=data[at:at+16])); at += 16
        stack.append(('PUSH', guid)); guids.append(guid)
        tokens.append({'offset': offset, 'opcode': 'PUSH', 'guid': guid})
    elif op in (3, 4):
        assert len(stack) >= 2
        right, left = stack.pop(), stack.pop()
        name = 'AND' if op == 3 else 'OR'
        stack.append((name, left, right)); tokens.append({'offset': offset, 'opcode': name})
    elif op == 5:
        assert stack
        stack.append(('NOT', stack.pop())); tokens.append({'offset': offset, 'opcode': 'NOT'})
    elif op in (6, 7):
        name = 'TRUE' if op == 6 else 'FALSE'
        stack.append((name,)); tokens.append({'offset': offset, 'opcode': name})
    elif op == 8:
        assert at == len(data) and len(stack) == 1
        tokens.append({'offset': offset, 'opcode': 'END'}); break
    else:
        raise ValueError('Unsupported dependency opcode ' + hex(op))
assert tokens[-1]['opcode'] == 'END'
expression = stack[0]
def evaluate(node, present):
    if node[0] == 'PUSH': return node[1] in present
    if node[0] == 'TRUE': return True
    if node[0] == 'FALSE': return False
    if node[0] == 'NOT': return not evaluate(node[1], present)
    a, b = evaluate(node[1], present), evaluate(node[2], present)
    return (a and b) if node[0] == 'AND' else (a or b)

# Reconstruct the report ancestry using UEFIExtract's hyphen depth.
report = (repo/'artifacts/firmware/uefi-report.txt').read_text()
ancestors = []
http_ancestors = None
for line in report.splitlines():
    m = re.search(r'\|\s+(-+) (.*)$', line)
    if not m: continue
    depth = len(m[1]); content = m[2]
    while ancestors and ancestors[-1]['depth'] >= depth: ancestors.pop()
    record = {'depth': depth, 'type': line.split('|')[0].strip(), 'description': content}
    if 'ECEBCB00-D9C8-11E4-AF3D-8CDCD426C973' in content:
        http_ancestors = ancestors.copy()
    ancestors.append(record)
assert http_ancestors
physical = (repo/'artifacts/firmware/http-inspect-observation.txt').read_text()
assert 'HTTP_INSPECTION_COMPLETE' in physical and 'PROBE_COMPLETE' in physical
physical_fv = sorted(set(re.findall(r'^FVB .*? name=([0-9a-f-]{36})', physical, re.M)))
volumes = []
for a in http_ancestors:
    if a['type'] != 'Volume': continue
    m = re.search(r'[0-9A-Fa-f]{8}(?:-[0-9A-Fa-f]{4}){3}-[0-9A-Fa-f]{12}', a['description'])
    if m:
        guid = m[0].lower()
        volumes.append({'guid': guid, 'observed_named_fv_handle': guid in physical_fv})
assert volumes
requester_references = {}
http_guid = uuid.UUID('ecebcb00-d9c8-11e4-af3d-8cdcd426c973').bytes_le
survey = json.loads((out/'module-survey.json').read_text())
for module in survey['modules']:
    binary = (out/(module['name']+'.bin')).read_bytes()
    assert hashlib.sha256(binary).hexdigest() == module['sha256']
    requester_references[module['name']] = [hex(m.start()) for m in
        re.finditer(re.escape(http_guid), binary)]
result = {'scope': __doc__, 'rom_sha256': '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c',
          'module_sha256': hashlib.sha256((out/'http-boot-dxe.bin').read_bytes()).hexdigest(),
          'depex_sha256': hashlib.sha256(data).hexdigest(), 'depex_bytes': len(data),
          'schedule_on_request': sor, 'tokens': tokens, 'expression': expression,
          'fixture_all_protocols_present': evaluate(expression, set(guids)),
          'fixture_no_protocols_present': evaluate(expression, set()),
          'individually_required_with_others_present': [g for g in sorted(set(guids))
              if not evaluate(expression, set(guids)-{g})],
          'saved_rom_ancestry': http_ancestors,
          'containing_volumes': volumes,
          'http_file_guid_references_in_seven_surveyed_modules': requester_references,
          'physical_report_sha256': hashlib.sha256(physical.encode()).hexdigest(),
          'limits': 'Dependency evaluation uses fixtures, not measured live protocol presence. SOR indicates an explicit scheduling gate, not permission to call global DXE Dispatch. Missing named FV handle is snapshot evidence, not proof the volume can never be exposed. No GUID match in a module does not exclude computed/indirect requests or an unexamined requester.'}
assert result['module_sha256'] == 'd992982aaef66ab249a4811d1a619eac3166e0cc66aef6a75e88014869020788'
(out/'dispatch-analysis.json').write_text(json.dumps(result, indent=2))
print(json.dumps({k: result[k] for k in ['depex_bytes', 'schedule_on_request',
      'fixture_all_protocols_present', 'fixture_no_protocols_present',
      'individually_required_with_others_present', 'containing_volumes']}, indent=2))

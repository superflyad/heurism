"""Check copied BIOS bytes and BPM signing-key binding without hardware access."""
import base64
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
image = (root / 'bios16-a.bin').read_bytes()
metadata = json.loads((root / 'bios16-a.json').read_text())
if len(image) != 0x1000000 or hashlib.sha256(image).hexdigest() != metadata['sha256']:
    raise SystemExit('BIOS backup size/hash mismatch')
if image != (root / 'bios16-b.bin').read_bytes():
    raise SystemExit('Independent BIOS reads differ')
signatures = json.loads((root / 'boot-policy-analysis.json').read_text())
for name in ['km_verify', 'bpm_verify', 'read_config']:
    if signatures['commands'][name]['returncode'] != 0:
        raise SystemExit('Upstream manifest check failed: ' + name)
config = json.loads((root / 'bootguard-config.json').read_text())
bpm, km = config['v2-bootpolicy'], config['v2-keymanifest']
algorithms = {4: 'sha1', 11: 'sha256', 12: 'sha384', 18: 'sm3'}
report = {'bios_sha256': metadata['sha256'], 'parser_commit': signatures['parser_commit'],
          'scope': 'Offline backup verification; hardware-fused OEM key hash not compared',
          'security_elements': [], 'bpm_key_bindings': []}
for element in bpm['bpmSE']:
    chunks, segments = [], []
    for segment in element['seIBBSegments']:
        if segment['ibbSegFlags'] != 0:
            raise SystemExit('Unsupported IBB segment flags')
        address, size = segment['ibbSegBase'], segment['ibbSegSize']
        offset = address - 0xff000000
        if size <= 0 or offset < 0 or offset + size > len(image):
            raise SystemExit('IBB segment outside BIOS backup')
        chunks.append(image[offset:offset + size])
        segments.append({'physical_start': hex(address), 'bytes': size,
                         'bios_offset': hex(offset), 'flash_offset': hex(0x800000 + offset)})
    ibb = b''.join(chunks)
    digests = []
    for entry in element['seDigestList']['hlList']:
        algorithm = algorithms[entry['hsAlg']]
        actual = hashlib.new(algorithm, ibb).digest()
        expected = base64.b64decode(entry['hsBuffer'], validate=True)
        if actual != expected:
            raise SystemExit('IBB digest mismatch: ' + algorithm)
        digests.append({'algorithm': algorithm, 'digest': actual.hex(), 'matches': True})
    if not segments or not digests:
        raise SystemExit('Empty IBB coverage or digest list')
    report['security_elements'].append({'flags': element['seFlags'], 'segments': segments,
                                      'bytes_hashed': len(ibb), 'digests': digests})
key = bpm['bpmSignature']['sigKeySignature']['ksKey']
key_data = base64.b64decode(key['keyData'], validate=True)
if key['keyAlg'] != 1 or len(key_data) != 4 + key['keyBitsize'] // 8:
    raise SystemExit('Unsupported BPM RSA key')
for entry in km['kmHash']:
    if not entry['hashUsage'] & 1:
        continue
    algorithm = algorithms[entry['hashStruct']['hsAlg']]
    # Fiano CBnT ValidateBPMKey hashes the encoded RSA modulus, omitting exponent.
    actual = hashlib.new(algorithm, key_data[4:]).digest()
    expected = base64.b64decode(entry['hashStruct']['hsBuffer'], validate=True)
    if actual != expected:
        raise SystemExit('BPM signing key does not match KM digest')
    report['bpm_key_bindings'].append({'algorithm': algorithm, 'digest': actual.hex(),
                                       'rsa_bits': key['keyBitsize'], 'matches': True})
if not report['bpm_key_bindings']:
    raise SystemExit('No BPM signing-key digest in KM')
(root / 'boot-policy-data-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))

"""Negative controls against saved Boot Guard policy; never install a firmware.

These verify digest and key binding, not a physical modified-firmware boot.
The fused OEM key is not directly compared and no key enrollment is attempted.
"""
import base64
import hashlib
import json
from pathlib import Path
from cryptography.hazmat.primitives.asymmetric import rsa

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
image = (root / 'bios16-a.bin').read_bytes()
current = (root / 'bios16-after-write-gate.bin').read_bytes()
current_metadata = json.loads((root / 'bios16-after-write-gate.json').read_text())
if len(current) != len(image) or hashlib.sha256(current).hexdigest() != current_metadata['sha256']:
    raise SystemExit('Current hardware read mismatch')
expected_hash = '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
if hashlib.sha256(image).hexdigest() != expected_hash:
    raise SystemExit('Pinned BIOS changed')
config = json.loads((root / 'bootguard-config.json').read_text())
bpm, km = config['v2-bootpolicy'], config['v2-keymanifest']
algorithms = {4: 'sha1', 11: 'sha256', 12: 'sha384', 18: 'sm3'}
results = []
for index, element in enumerate(bpm['bpmSE']):
    chunks = []
    for segment in element['seIBBSegments']:
        offset, size = segment['ibbSegBase'] - 0xff000000, segment['ibbSegSize']
        if segment['ibbSegFlags'] or size <= 0 or offset < 0 or offset + size > len(image):
            raise SystemExit('Unsupported IBB range')
        chunks.append(image[offset:offset+size])
        if current[offset:offset+size] != chunks[-1]:
            raise SystemExit('Current protected startup bytes differ from baseline')
    original = b''.join(chunks)
    for entry in element['seDigestList']['hlList']:
        algorithm = algorithms[entry['hsAlg']]
        expected = base64.b64decode(entry['hsBuffer'], validate=True)
        if hashlib.new(algorithm, original).digest() != expected:
            raise SystemExit('Positive baseline failed')
        at = 0
        for number, chunk in enumerate(chunks):
            changed = bytearray(original)
            changed[at] ^= 1
            digest = hashlib.new(algorithm, changed).digest()
            if digest == expected:
                raise SystemExit('Changed startup data unexpectedly accepted')
            results.append({'element': index, 'segment': number, 'algorithm': algorithm,
                            'changed_bios_offset': hex(element['seIBBSegments'][number]['ibbSegBase']-0xff000000),
                            'baseline_matches': True, 'changed_matches': False})
            at += len(chunk)
key = bpm['bpmSignature']['sigKeySignature']['ksKey']
encoded = base64.b64decode(key['keyData'], validate=True)
if key['keyAlg'] != 1 or len(encoded) != 4 + key['keyBitsize']//8:
    raise SystemExit('Unsupported key encoding')
# A valid fresh RSA public key. Private key exists only in memory and is discarded.
owner = rsa.generate_private_key(public_exponent=65537, key_size=key['keyBitsize']).public_key().public_numbers()
owner_modulus = owner.n.to_bytes(key['keyBitsize']//8, 'little')
bindings = []
for entry in km['kmHash']:
    if not entry['hashUsage'] & 1:
        continue
    algorithm = algorithms[entry['hashStruct']['hsAlg']]
    expected = base64.b64decode(entry['hashStruct']['hsBuffer'], validate=True)
    if hashlib.new(algorithm, encoded[4:]).digest() != expected:
        raise SystemExit('Vendor key baseline failed')
    candidate = hashlib.new(algorithm, owner_modulus).digest()
    if candidate == expected:
        raise SystemExit('Fresh owner key unexpectedly bound by existing KM')
    bindings.append({'algorithm': algorithm, 'vendor_key_matches': True,
                     'fresh_owner_key_matches': False, 'owner_modulus_digest': candidate.hex()})
if not results or not bindings:
    raise SystemExit('Missing startup policy tests')
report = {'scope': __doc__, 'bios_sha256': expected_hash,
          'current_hardware_read_sha256': current_metadata['sha256'],
          'current_protected_startup_bytes_match_baseline': True,
          'modified_ibb_controls': results, 'owner_key_controls': bindings,
          'physical_modified_firmware_boot_tested': False, 'hardware_writes': False}
(root / 'startup-acceptance-controls.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'changed_startup_digest_rejections': len(results),
                  'owner_key_binding_rejections': len(bindings),
                  'positive_baselines_passed': True, 'physical_modified_boot_tested': False}, indent=2))

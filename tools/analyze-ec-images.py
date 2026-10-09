"""Validate observed Microchip PHCM headers in vendor EC update files only.

Header/payload hashing is integrity evidence, not physical authentication state.
Version-1 flags are compared with pinned official generator controls.
"""
import hashlib
import json
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
folder = root / 'vendor/recovery-1.35.0-extracted'
manifest = json.loads((root / 'recovery-package-analysis.json').read_text())
controls = json.loads((root / 'ec-format-controls.json').read_text())
if controls['generator_sha256'] != 'bf6a5f07d5e373173beed53215e06c32e4395f60d5cec1233835a892a8354512' or len(controls['cases']) != 8:
    raise SystemExit('Missing expected generator controls')
# NIST P-256 field and curve constant. Check public ephemeral coordinates only.
prime = 0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff
curve_b = int('5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b', 16)

def on_curve(point):
    if len(point) != 64:
        return False
    x = int.from_bytes(point[:32], 'big')
    y = int.from_bytes(point[32:], 'big')
    return x < prime and y < prime and (y * y - x * x * x + 3 * x - curve_b) % prime == 0

cases = []
for item in manifest['files']:
    if 'Embedded Controller' not in item['path'] or not item['path'].endswith('.data.bin'):
        continue
    data = (folder / item['path']).read_bytes()
    if hashlib.sha256(data).hexdigest() != item['sha256'] or len(data) != item['bytes']:
        raise SystemExit('EC file changed')
    if data[:4] != b'PHCM' or data[4] != 1 or len(data) < 96:
        raise SystemExit('Unexpected EC header')
    original = hashlib.sha256(data[:64]).digest() == data[64:96]
    changed = bytearray(data[:64])
    changed[8] ^= 1
    negative = hashlib.sha256(changed).digest() == data[64:96]
    if not original or negative:
        raise SystemExit('Header integrity control failed')
    size = struct.unpack_from('<H', data, 16)[0] * 64
    offset = struct.unpack_from('<I', data, 20)[0]
    end = offset + size
    if data[6] != 0x80 or offset != 192 or end + 192 > len(data):
        raise SystemExit('Unexpected encrypted version-1 layout')
    key_header = data[end:end + 64]
    covered = data[offset:end + 64]
    payload_matches = hashlib.sha256(covered).digest() == data[end + 64:end + 96]
    changed_payload = bytearray(covered)
    changed_payload[0] ^= 1
    changed_key = bytearray(covered)
    changed_key[-1] ^= 1
    negative_payload = hashlib.sha256(changed_payload).digest() == data[end + 64:end + 96]
    negative_key = hashlib.sha256(changed_key).digest() == data[end + 64:end + 96]
    if not on_curve(key_header) or not payload_matches or negative_payload or negative_key:
        raise SystemExit('Payload/key-header integrity control failed')
    cases.append({'path': item['path'], 'bytes': len(data), 'sha256': item['sha256'],
                  'header_version': data[4], 'raw_flags_at_6': hex(data[6]),
                  'load_address': hex(struct.unpack_from('<I', data, 8)[0]),
                  'entry_address': hex(struct.unpack_from('<I', data, 12)[0]),
                  'payload_offset_field': hex(struct.unpack_from('<I', data, 20)[0]),
                  'header_sha256_matches': original,
                  'altered_header_hash_matches': negative,
                  'security_flags_decoded': True,
                  'flag_meaning_basis': 'Eight pinned Microchip version-1 generator controls',
                  'encryption_requested': True, 'authentication_flag_requested': False,
                  'payload_length': size, 'post_payload_bytes': len(data) - end,
                  'key_header_p256_point_valid': True,
                  'payload_and_key_header_sha256_matches': payload_matches,
                  'altered_payload_hash_matches': negative_payload,
                  'altered_key_header_hash_matches': negative_key,
                  'physical_otp_authentication_policy_known': False,
                  'payload_decrypted': False})
if len(cases) != 4:
    raise SystemExit('Expected four EC update images')
result = {'scope': 'Vendor EC header integrity/format evidence only; '
                   'no proof of physical EC type, decryption, ownership or write acceptance',
          'cases': cases}
(root / 'ec-image-analysis.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))

"""Map ROM recovery key material to signed IBB coverage with in-memory controls."""
import base64
import hashlib
import json
from pathlib import Path
import struct
import uuid

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
rom = (root / 'bios16-a.bin').read_bytes()
expected = '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
if hashlib.sha256(rom).hexdigest() != expected:
    raise SystemExit('ROM changed')
policy = json.loads((root / 'bootguard-config.json').read_text())['v2-bootpolicy']
algorithms = {4: 'sha1', 11: 'sha256', 12: 'sha384', 18: 'sm3'}
covered = []
for index in range(11):
    record = struct.unpack_from('<I', rom, 0xf95c20 + index * 8)[0] - 0xff000000
    pointer, size, flags = struct.unpack_from('<III', rom, record)
    key_offset = pointer - 0xff000000
    if size != 512 or flags != 0x221 or not 0 <= key_offset <= len(rom) - size:
        raise SystemExit('Invalid key descriptor')
    covered.append({'name': 'key_' + str(index), 'offset': key_offset, 'bytes': size,
                    'guid': str(uuid.UUID(bytes_le=rom[record + 12:record + 28]))})
    covered.append({'name': 'key_record_' + str(index), 'offset': record, 'bytes': 28})
covered.append({'name': 'key_pointer_list', 'offset': 0xf95c20, 'bytes': 88})
results = []
for item in covered:
    enclosing = []
    failures = []
    changed = bytearray(rom)
    changed[item['offset']] ^= 1  # No candidate image is written to disk or hardware.
    for element in policy['bpmSE']:
        segments = [(s['ibbSegBase'] - 0xff000000, s['ibbSegSize']) for s in element['seIBBSegments'] if s['ibbSegFlags'] == 0]
        enclosing.extend({'offset': hex(at), 'bytes': size} for at, size in segments
                         if at <= item['offset'] and item['offset'] + item['bytes'] <= at + size)
        original_ibb = b''.join(rom[at:at + size] for at, size in segments)
        changed_ibb = b''.join(changed[at:at + size] for at, size in segments)
        for digest in element['seDigestList']['hlList']:
            algorithm = algorithms[digest['hsAlg']]
            wanted = base64.b64decode(digest['hsBuffer'], validate=True)
            if hashlib.new(algorithm, original_ibb).digest() != wanted:
                raise SystemExit('Original signed IBB digest does not match')
            if hashlib.new(algorithm, changed_ibb).digest() == wanted:
                raise SystemExit('Mutated key byte still matched signed IBB')
            failures.append(algorithm)
    if not enclosing:
        raise SystemExit('Recovery key bytes outside expected signed IBB coverage')
    results.append({**item, 'offset': hex(item['offset']), 'covering_segments': enclosing,
                    'one_byte_change_mismatches': failures})
report = {'scope': 'Offline signed-IBB coverage, not a new fused-key verification or hardware boot test',
          'rom_sha256': expected, 'key_count': 11, 'objects': results}
(root / 'recovery-key-coverage.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'key_count': 11, 'covered_objects': len(results),
                  'negative_digest_controls': sum(len(x['one_byte_change_mismatches']) for x in results)}, indent=2))

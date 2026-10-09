"""Map firmware volumes and look for digest references in protected Dell data."""
import hashlib
import json
from pathlib import Path
import re
import struct

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
image = (root / 'bios16-a.bin').read_bytes()
mapping = json.loads((root / 'uefi-map.json').read_text())
report_bytes = (root / 'uefi-report.txt').read_bytes()
if hashlib.sha256(image).hexdigest() != mapping['bios_sha256']:
    raise SystemExit('BIOS backup differs from parsed source')
if hashlib.sha256(report_bytes).hexdigest() != mapping['report_sha256']:
    raise SystemExit('Parser report changed')
tables = [m.start() for m in re.finditer(b'__MHTS__', image)]
volumes, matches = [], []
for line in report_bytes.decode().splitlines():
    fields = [field.strip() for field in line.split('|')]
    if len(fields) < 6 or fields[0] != 'Volume' or fields[2] == 'N/A':
        continue
    start, size = int(fields[2], 16), int(fields[3], 16)
    if start + size > len(image) or image[start + 0x28:start + 0x2c] != b'_FVH':
        raise SystemExit('Invalid mapped firmware volume')
    header_size = struct.unpack_from('<H', image, start + 0x30)[0]
    if not 0x38 <= header_size <= size or header_size % 2:
        raise SystemExit('Invalid firmware volume header length')
    checksum = sum(struct.unpack('<' + 'H' * (header_size // 2),
                                image[start:start + header_size])) & 0xffff
    volume = {'bios_offset': hex(start), 'bytes': size, 'name': fields[5],
              'header_checksum_valid': checksum == 0,
              'overlaps_ibb': start + size > 0xe60000}
    volumes.append(volume)
    for skip in sorted({0, header_size, 0x48, 0x60, 0x78}):
        for algorithm in ['sha1', 'sha256', 'sha384', 'sha512']:
            digest = hashlib.new(algorithm, image[start + skip:start + size]).digest()
            for table in tables:
                position = image[table:table + 0x1000].find(digest)
                if position >= 0:
                    matches.append({'volume_offset': hex(start), 'bytes_hashed': size - skip,
                                    'header_bytes_skipped': skip, 'algorithm': algorithm,
                                    'table_offset': hex(table), 'digest_offset': hex(table + position),
                                    'table_inside_ibb': table >= 0xe60000,
                                    'digest': digest.hex()})
module = image[0xfafc20:0xfafc20 + 0x1d60]
if module[:2] != b'MZ' or hashlib.sha256(module).hexdigest() != 'ca630da6dc32c03468b2ba2b05279a38ebaf6af67f184a8f44bc1523cdb116c2':
    raise SystemExit('Trust-chaining PE section differs from independent extraction')
(root / 'dell-trust-chaining-pei.bin').write_bytes(module)
result = {'bios_sha256': mapping['bios_sha256'], 'parser_commit': mapping['parser_commit'],
          'scope': 'Offline structural/digest checks; does not prove runtime enforcement or writable space',
          'volumes': volumes, 'dell_hash_table_offsets': [hex(p) for p in tables],
          'digest_references': matches,
          'trust_chaining_pei_sha256': hashlib.sha256(module).hexdigest(),
          'parser_warnings': ['Two invalid-size file headers', 'Six TE image-base warnings',
                              'An unreferenced FIT candidate; authoritative FIT identified']}
(root / 'uefi-volume-analysis.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({'mapped_volumes': len(volumes), 'invalid_volume_headers':
                  sum(not v['header_checksum_valid'] for v in volumes),
                  'dell_hash_tables': result['dell_hash_table_offsets'],
                  'digest_reference_count': len(matches),
                  'referenced_volumes': sorted({entry['volume_offset'] for entry in matches})}, indent=2))

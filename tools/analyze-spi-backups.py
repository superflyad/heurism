"""Validate paired flash reads and inspect descriptor/FIT metadata offline."""
import hashlib
import json
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
verified = {}
for prefix, expected in [('descriptor', 4096), ('bios16', 16*1024*1024), ('csme', 0x6ff000)]:
    copies = []
    for suffix in ['a', 'b']:
        filename = root / f'{prefix}-{suffix}.bin'
        data = filename.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        metadata = json.loads(filename.with_suffix('.json').read_text())
        if len(data) != expected or metadata['sha256'] != digest:
            raise SystemExit('Size/hash mismatch: ' + str(filename))
        copies.append(data)
    if copies[0] != copies[1]:
        raise SystemExit('Independent reads differ: ' + prefix)
    verified[prefix] = {'bytes': expected, 'sha256': hashlib.sha256(copies[0]).hexdigest(),
                        'independent_copies_identical': True}
d = (root / 'descriptor-a.bin').read_bytes()
if struct.unpack_from('<I', d, 16)[0] != 0x0ff0a55a:
    raise SystemExit('Invalid Intel descriptor signature')
map0, map1 = struct.unpack_from('<II', d, 20)
component = struct.unpack_from('<I', d, (map0 & 255)*16)[0]
regions = []
names = {0: 'descriptor', 1: 'BIOS', 2: 'CSME', 8: 'EC'}
master = struct.unpack_from('<I', d, (map1 & 255)*16)[0]
read_mask, write_mask = (master >> 8) & 0xfff, (master >> 20) & 0xfff
for i in range(16):
    raw = struct.unpack_from('<I', d, ((map0 >> 16) & 255)*16 + 4*i)[0]
    start, end = (raw & 0x7fff) << 12, (((raw >> 16) & 0x7fff) << 12) | 0xfff
    if start <= end:
        regions.append({'index': i, 'name': names.get(i, 'other'), 'start': hex(start),
            'end': hex(end), 'bytes': end-start+1,
            'host_descriptor_read_permission': bool(read_mask & (1 << i)),
            'host_descriptor_write_permission': bool(write_mask & (1 << i))})
image = (root / 'bios16-a.bin').read_bytes()
physical_base = 0x100000000 - len(image)
pointer = struct.unpack_from('<Q', image, len(image)-64)[0]
offset = pointer - physical_base
if offset < 0 or offset + 16 > len(image) or image[offset:offset+8] != b'_FIT_   ':
    raise SystemExit('Invalid FIT pointer/header')
count = int.from_bytes(image[offset+8:offset+11], 'little')
if not 1 <= count <= 4096 or offset + count*16 > len(image):
    raise SystemExit('Invalid FIT count')
if sum(image[offset:offset+count*16]) % 256:
    raise SystemExit('FIT checksum mismatch')
entries = []
for i in range(1, count):
    p = offset + i*16
    address = struct.unpack_from('<Q', image, p)[0]
    size = int.from_bytes(image[p+8:p+11], 'little')
    kind = image[p+14] & 127
    entry = {'index': i, 'type': hex(kind), 'address': hex(address), 'size_field': size}
    if kind in (0xb, 0xc):
        local = address - physical_base
        if local < 0 or not size or local + size > len(image):
            raise SystemExit('Manifest outside BIOS region')
        blob = image[local:local+size]
        name = 'key-manifest' if kind == 0xb else 'boot-policy-manifest'
        (root / (name + '.bin')).write_bytes(blob)
        entry.update(name=name, bytes=size, sha256=hashlib.sha256(blob).hexdigest(),
                     structure_id=blob[:8].decode('ascii', errors='replace'))
    entries.append(entry)
report = {'verified_backups': verified,
    'descriptor': {'component_count': ((map0 >> 8) & 3)+1,
        'component_density_codes': [component & 15, (component >> 4) & 15],
        'host_master_raw': hex(master), 'regions': regions},
    'fit': {'physical_address': hex(pointer), 'entries': entries, 'checksum_valid': True},
    'limitations': ['EC region denied by host descriptor permissions and observed hardware read failure.',
        'These backups are not a complete 24 MiB flash image; no erased filler is synthesized.',
        'Descriptor write permission is not effective write permission: BIOS control, '
        'SMM protection and Boot Guard verification remain separate.',
        'Manifest presence/structure extraction does not verify their cryptographic signatures or unlock Boot Guard.'],
    'sources': ['https://github.com/coreboot/coreboot/blob/e9a90afe4d77b8e27ad07aec620c9e1299190944/util/ifdtool/ifdtool.h',
        'https://edc.intel.com/content/www/us/en/design/products-and-solutions/software-and-services/firmware-and-bios/firmware-interface-table/1.2/boot-policy-manifest-type-0x0c-rules/']}
(root / 'spi-backup-analysis.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))

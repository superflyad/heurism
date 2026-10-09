"""Compare the extracted vendor baseline with saved physical ROM evidence."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
extracted = root / 'vendor/recovery-1.35.0-extracted'
manifest = json.loads((root / 'recovery-package-analysis.json').read_text())
for item in manifest['files']:
    path = extracted / item['path']
    if not path.resolve().is_relative_to(extracted.resolve()):
        raise SystemExit('Invalid manifest path')
    data = path.read_bytes()
    if len(data) != item['bytes'] or hashlib.sha256(data).hexdigest() != item['sha256']:
        raise SystemExit('Extracted content changed: ' + item['path'])
image_path = next((extracted / 'Firmware').glob('*System BIOS with BiosGuard*.data.bin'))
vendor = image_path.read_bytes()
physical = (root / 'bios16-a.bin').read_bytes()
policy = json.loads((root / 'boot-policy-data-verification.json').read_text())
mapping = json.loads((root / 'uefi-volume-analysis.json').read_text())
if len(vendor) != 0x1000000 or hashlib.sha256(physical).hexdigest() != policy['bios_sha256']:
    raise SystemExit('Unexpected BIOS baseline or physical backup')
segments = []
for element in policy['security_elements']:
    joined = b''
    for segment in element['segments']:
        offset, length = int(segment['bios_offset'], 16), segment['bytes']
        same = vendor[offset:offset + length] == physical[offset:offset + length]
        segments.append({'bios_offset': hex(offset), 'bytes': length, 'identical': same})
        joined += vendor[offset:offset + length]
    for digest in element['digests']:
        if hashlib.new(digest['algorithm'], joined).hexdigest() != digest['digest']:
            raise SystemExit('Vendor IBB differs from current signed manifest')
volumes = []
for volume in mapping['volumes']:
    offset, length = int(volume['bios_offset'], 16), volume['bytes']
    volumes.append({'bios_offset': hex(offset), 'bytes': length,
                    'identical': vendor[offset:offset + length] == physical[offset:offset + length]})
result = {'scope': 'Vendor baseline comparison; not a complete machine backup or '
                   'proof of recovery acceptance',
          'physical_bios_sha256': policy['bios_sha256'],
          'vendor_bios_sha256': hashlib.sha256(vendor).hexdigest(),
          'ibb_segments': segments, 'all_vendor_ibb_digests_match': True,
          'volumes': volumes,
          'equal_4k_pages': sum(vendor[i:i + 4096] == physical[i:i + 4096]
                                for i in range(0, len(vendor), 4096)),
          'total_4k_pages': len(vendor) // 4096,
          'complete_images_identical': vendor == physical,
          'signature_files_present': sum(item['path'].endswith('.sig') for item in manifest['files']),
          'signature_validation_performed': False}
(root / 'recovery-content-verification.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({key: value for key, value in result.items() if key != 'volumes'}, indent=2))
print('Identical mapped firmware volumes:', sum(item['identical'] for item in volumes), '/', len(volumes))

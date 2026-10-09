"""Locate the exact owner variable payload in a hardware-read BIOS snapshot.

This is a physical storage observation, not autonomous firmware dispatch.
No hardware accesses or writes are performed by this script.
"""
import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'artifacts/firmware'
payload=(root/'nv-extension-payload-readback.bin').read_bytes()
if len(payload)!=2048 or hashlib.sha256(payload).hexdigest()!='b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a':
    raise SystemExit('Persisted payload differs')
current=(root/'bios16-with-nv-extension.bin').read_bytes()
metadata=json.loads((root/'bios16-with-nv-extension.json').read_text())
previous=(root/'bios16-after-write-gate.bin').read_bytes()
previous_metadata=json.loads((root/'bios16-after-write-gate.json').read_text())
if len(current)!=0x1000000 or hashlib.sha256(current).hexdigest()!=metadata['sha256'] or hashlib.sha256(previous).hexdigest()!=previous_metadata['sha256']:
    raise SystemExit('Hardware-read image differs')
if payload in previous:
    raise SystemExit('Payload was already present in earlier snapshot')
name=('CompanionExtensionImage01\0').encode('utf-16-le')
locations=[];at=0
while True:
    at=current.find(payload,at)
    if at<0:break
    if not 0x20000<=at or at+len(payload)>0xd0000:
        raise SystemExit('Owner payload unexpectedly outside known variable-store area')
    if name not in current[max(0,at-512):at]:
        raise SystemExit('Owner variable name absent near image data')
    locations.append({'bios_offset':hex(at),'logical_spi_flash_offset':hex(0x800000+at),
                      'bytes':len(payload),'nearby_utf16_owner_variable_name':True})
    at+=len(payload)
if not locations:raise SystemExit('Image not found in physical variable storage')
if current[0xd0000:]!=previous[0xd0000:]:
    raise SystemExit('Code outside variable area changed; review')
report={'scope':__doc__,'current_bios_sha256':metadata['sha256'],
        'previous_bios_sha256':previous_metadata['sha256'],
        'payload_sha256':hashlib.sha256(payload).hexdigest(),'locations':locations,
        'payload_absent_in_previous_snapshot':True,'all_bios_bytes_from_0xd0000_unchanged':True,
        'ssd_independent_bootstrap_proven':False}
(root/'nv-extension-flash-location.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))

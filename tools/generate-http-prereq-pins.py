"""Generate comparison-only arrays from the previously examined BIOS files."""
import hashlib
from pathlib import Path
repo=Path(__file__).resolve().parents[1]
root=repo/'artifacts/research/independent-bootstrap'
pins=[('http_pin','http-boot-dxe.bin','d992982aaef66ab249a4811d1a619eac3166e0cc66aef6a75e88014869020788'),
      ('depex_pin','http-boot-depex.bin','b73f346c1fb9a891c672d684f5d6eceb36ba1371c6f6f77bd2e6bbfa569a7cdf'),
      ('provider_pin','provider-DellBoardPolicyDxe.bin','31cf5121722bbff0cc5096b7a87d628669a8700ee149787477c72b2d787226fa')]
out=['/* Comparison data only; never passed to LoadImage or executed. */']
for name,file,digest in pins:
 data=(root/file).read_bytes();assert hashlib.sha256(data).hexdigest()==digest
 out.append('static const U8 '+name+'[]={')
 out.extend(','.join(f'0x{x:02x}' for x in data[i:i+16])+',' for i in range(0,len(data),16))
 out.append('};')
(repo/'boot/http_prereq_pins.h').write_text('\n'.join(out)+'\n')

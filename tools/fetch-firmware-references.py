"""Download primary reference files for offline parsing; never execute firmware."""
import hashlib
import json
from pathlib import Path
import urllib.request

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware/vendor'
root.mkdir(exist_ok=True)
items = [
    ('BIOS_IMG-1.35.0.rcv', 'https://dl.dell.com/FOLDER12727393M/1/BIOS_IMG.rcv',
     'e649ae3fc5684a7bc7790bc3f2dca7f4235986d80af469ec60b926fc0e7fa9ed'),
    ('intel-500-pch-vol2.pdf', 'https://cdrdv2-public.intel.com/631120/631120-002.pdf', None)]
for name, url, expected in items:
    destination = root / name
    if destination.exists():
        data = destination.read_bytes()
    else:
        with urllib.request.urlopen(url, timeout=30) as response:
            data = response.read(100000001)
        if len(data) > 100000000:
            raise ValueError('Reference exceeds download limit')
    digest = hashlib.sha256(data).hexdigest()
    if expected and digest != expected:
        raise ValueError(name + ' checksum differs from Dell publication')
    destination.write_bytes(data)
    destination.with_suffix(destination.suffix + '.json').write_text(json.dumps(
        {'url': url, 'bytes': len(data), 'sha256': digest,
         'published_sha256_verified': bool(expected)}, indent=2), encoding='utf-8')
    print(name, len(data), digest)

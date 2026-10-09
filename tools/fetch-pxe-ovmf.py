"""Fetch Debian's PXE-enabled OVMF; validate the official package-index SHA256."""
import hashlib,lzma,tarfile,io,json,urllib.request
from pathlib import Path
out=Path(__file__).resolve().parents[1]/'build/network-root/ovmf'
base='https://deb.debian.org/debian/'
index=urllib.request.urlopen(base+'dists/bookworm/main/binary-amd64/Packages.xz',timeout=30).read()
text=lzma.decompress(index).decode()
entry=next(e for e in text.split('\n\n') if e.startswith('Package: ovmf\n'))
fields=dict(line.split(': ',1) for line in entry.splitlines() if ': ' in line and not line.startswith(' '))
data=urllib.request.urlopen(base+fields['Filename'],timeout=30).read()
assert hashlib.sha256(data).hexdigest()==fields['SHA256']
assert data[:8]==b'!<arch>\n'
offset=8
while offset<len(data):
 header=data[offset:offset+60];size=int(header[48:58]);name=header[:16].strip().decode().rstrip('/')
 body=data[offset+60:offset+60+size];offset+=60+size+(size&1)
 if name.startswith('data.tar'):
  with tarfile.open(fileobj=io.BytesIO(body),mode='r:*') as tar:
   for member in tar:
    if member.isfile() and member.name.endswith(('OVMF_CODE.fd','OVMF_VARS.fd')):
     (out/Path(member.name).name).write_bytes(tar.extractfile(member).read())
report={k:fields[k] for k in ['Package','Version','Filename','SHA256']};(out/'source.json').write_text(json.dumps(report,indent=2));print(report)

"""Embed only the exact previously hardware-verified 2048-byte owner driver."""
import hashlib
from pathlib import Path
repo=Path(__file__).resolve().parents[1]
data=(repo/'build/extension/companionextx64.efi').read_bytes()
if len(data)!=2048 or hashlib.sha256(data).hexdigest()!='b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a':
    raise SystemExit('Payload differs from the physically verified owner driver')
destination=repo/'build/nv-extension/payload.h'
destination.parent.mkdir(exist_ok=True)
destination.write_text('static const U8 expected_payload[2048]={\n'+',\n'.join(','.join(f'0x{x:02x}' for x in data[i:i+16]) for i in range(0,len(data),16))+'\n};\n',encoding='ascii')
print('Pinned 2048-byte owner payload; no arbitrary image input')

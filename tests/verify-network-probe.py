import ctypes
import hashlib
from pathlib import Path
import struct
import sys
root=Path(sys.argv[1]).resolve();data=(root/'fvprobex64.efi').read_bytes()
pe=struct.unpack_from('<I',data,60)[0];optional=pe+24
assert data[:2]==b'MZ' and data[pe:pe+4]==b'PE\0\0'
assert struct.unpack_from('<H',data,pe+4)[0]==0x8664
assert struct.unpack_from('<H',data,optional+68)[0]==10
assert struct.unpack_from('<II',data,optional+120)==(0,0)
assert all(struct.unpack_from('<II',data,optional+152))
dll=ctypes.CDLL(str(root/'network-tests.dll'));assert dll.network_run_tests()==0
print('PASS: bounded interface inventory, absent protocols, excessive counts, malformed paths; freestanding EFI application')
print('EFI SHA256:',hashlib.sha256(data).hexdigest())

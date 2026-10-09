"""Validate the driver image type, relocations, EFI ABI and bounded service."""
import ctypes
import hashlib
from pathlib import Path
import struct
import sys
root=Path(sys.argv[1]).resolve()
data=(root/'companionextx64.efi').read_bytes()
assert data[:2]==b'MZ'
pe=struct.unpack_from('<I',data,60)[0]
assert data[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',data,pe+4)[0]==0x8664
optional=pe+24
assert struct.unpack_from('<H',data,optional)[0]==0x20b
assert struct.unpack_from('<H',data,optional+68)[0]==11
assert struct.unpack_from('<II',data,optional+120)==(0,0)
assert all(struct.unpack_from('<II',data,optional+152))
dll=ctypes.CDLL(str(root/'extension-tests.dll'))
assert dll.extension_run_tests()==0
print('PASS: x64 boot-service DRIVER, no OS imports, relocations; protocol installation, size bounds, bad inputs and install errors')
print('EFI SHA256:',hashlib.sha256(data).hexdigest())

import ctypes
import hashlib
import json
from pathlib import Path
import struct
import sys
root=Path(sys.argv[1]).resolve();data=(root/'companionextx64.efi').read_bytes()
pe=struct.unpack_from('<I',data,60)[0];optional=pe+24
assert data[:2]==b'MZ' and data[pe:pe+4]==b'PE\0\0'
assert struct.unpack_from('<H',data,pe+4)[0]==0x8664
assert struct.unpack_from('<H',data,optional+68)[0]==11
assert struct.unpack_from('<II',data,optional+120)==(0,0)
assert all(struct.unpack_from('<II',data,optional+152))
dll=ctypes.CDLL(str(root/'nv-startup-tests.dll'))
result=dll.nv_startup_run_tests();assert result==0,result
report={'sha256':hashlib.sha256(data).hexdigest(),'host_cases':13,'passed':True,
        'scope':'Exact retrieved NV source, invalid/missing/short payload, attributes, load/start/protocol/info failures, runtime absence, diagnostic failure and SSD fallback.'}
(root/'host-verification.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))

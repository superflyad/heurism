import ctypes
import hashlib
import json
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve()
dll=ctypes.CDLL(str(root/'prereq-tests.dll'));result=dll.prereq_run_tests();assert result==0,result
report=(root/'mock-report.txt').read_text()
assert 'COMPANION_HTTP_PREREQ_01' in report and 'HTTP_PREREQUISITES_COMPLETE' in report
assert 'POLICY_INTERFACE_IMAGE_OWNERS count=0x0000000000000000' in report
proof={'passed':True,'sha256':hashlib.sha256((root/'fvprobex64.efi').read_bytes()).hexdigest(),
 'scope':'Exact section comparison, corrupt/truncated/changed buffers, warning status, target MAC/file paths, malformed paths, loaded-image range limits; common EFI ABI and SSD handoff fixtures.'}
(root/'host-verification.json').write_text(json.dumps(proof,indent=2));print(json.dumps(proof,indent=2))

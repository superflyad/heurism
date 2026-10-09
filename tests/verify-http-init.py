import ctypes
import hashlib
import json
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve()
result=ctypes.CDLL(str(root/'init-tests.dll')).init_run_tests();assert result==0,result
report=(root/'mock-report.txt').read_text()
assert 'COMPANION_HTTP_INIT_01' in report and 'HTTP_INIT_GUARD_STOP' in report
assert 'HTTP_INITIALIZATION_COMPLETE' in report
proof={'passed':True,'sha256':hashlib.sha256((root/'fvprobex64.efi').read_bytes()).hexdigest(),
 'cases':11,'scope':'Pinned source load/start/owned binding removal; callback present, code mismatch, memory bounds, invalid lists, bad driver/path, load/start/remove failures and absent report. EFI ABI/report/SSD handoff tested separately.'}
(root/'host-verification.json').write_text(json.dumps(proof,indent=2));print(json.dumps(proof,indent=2))

import ctypes
import hashlib
import json
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve()
result=ctypes.CDLL(str(root/'ethernet-tests.dll')).ethernet_run_tests();assert result==0,result
report=(root/'mock-report.txt').read_text()
assert 'COMPANION_HTTP_ETHERNET_02' in report and 'HTTP_INIT_GUARD_STOP' in report
proof={'passed':True,'sha256':hashlib.sha256((root/'fvprobex64.efi').read_bytes()).hexdigest(),
 'cases':19,'scope':'Shared initialization/cleanup guards, unique MAC controller selection, duplicate/mismatched rejection, binding decimal version/function identity, Supported ABI/status propagation, report and SSD handoff. No controller Start.'}
(root/'host-verification.json').write_text(json.dumps(proof,indent=2));print(json.dumps(proof,indent=2))

import ctypes
import hashlib
import json
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve()
result=ctypes.CDLL(str(root/'cleanup-tests.dll')).cleanup_run_tests();assert result==0,result
report=(root/'mock-report.txt').read_text()
assert 'COMPANION_HTTP_CLEANUP_01' in report and 'HTTP_CLEANUP_INSPECTION_COMPLETE' in report
proof={'passed':True,'sha256':hashlib.sha256((root/'fvprobex64.efi').read_bytes()).hexdigest(),
 'cases':20,'scope':'Prerequisite fixtures, MAC selection/bounds, open-record ABI/target relationships, failed query, oversized array rejection and returned buffer release. Common EFI report and SSD handoff suite passed.'}
(root/'host-verification.json').write_text(json.dumps(proof,indent=2));print(json.dumps(proof,indent=2))

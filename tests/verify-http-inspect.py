import ctypes
import hashlib
import json
from pathlib import Path
import sys
root=Path(sys.argv[1]).resolve();dll=ctypes.CDLL(str(root/'inspect-tests.dll'))
result=dll.inspect_run_tests();assert result==0,result
report={'sha256':hashlib.sha256((root/'fvprobex64.efi').read_bytes()).hexdigest(),
        'passed':True,'scope':'Interface/binding metadata, HII export summaries, missing providers, handle/export limits, malformed package/IFR/path guards; separate common probe tests verify SSD handoff.'}
(root/'host-verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

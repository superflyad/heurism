"""Start the bounded test server hidden and record its PID and readiness."""
import json
import argparse
from pathlib import Path
import subprocess
import time
import urllib.request
repo=Path(__file__).resolve().parents[1];root=repo/'artifacts/firmware'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--node',default='C:/Program Files/nodejs/node.exe')
args=parser.parse_args()
if (root/'http-server-state.json').exists():raise SystemExit('Existing server state requires inspection; do not start a duplicate')
with (root/'http-server-process.log').open('wb') as log:
 process=subprocess.Popen([str(Path(args.node).resolve()),str(repo/'tools/preboot-http-server.cjs')],
  stdout=log,stderr=log,creationflags=0x08000000)
report={'pid':process.pid,'runtime':str(Path(args.node).resolve()),'url':'http://10.8.22.122:18080','started_unix':time.time(),'expires_seconds':1800}
(root/'http-server-state.json').write_text(json.dumps(report,indent=2))
for _ in range(30):
 if process.poll() is not None:raise SystemExit('Server exited: '+(root/'http-server-process.log').read_text())
 try:
  with urllib.request.urlopen(report['url']+'/health',timeout=1) as r:
   assert r.read()==b'COMPANION_HTTP_READY_01\n'
  print(json.dumps(report,indent=2));break
 except OSError:time.sleep(.1)
else:
 process.terminate();raise SystemExit('Server readiness check failed')

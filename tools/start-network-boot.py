"""Start the temporary target-only PXE server hidden; leave SSD boot unchanged."""
import argparse,json,subprocess,time,urllib.request
from pathlib import Path
repo=Path(__file__).resolve().parents[1];out=repo/'artifacts/firmware'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('mode',choices=['probe','ram']);args=p.parse_args()
raise SystemExit('Physical PXE experiments are disabled after the Dell stopped at SupportAssist. Use isolated VM tests; retain the working SSD loader for physical network experiments.')
runtime='C:/Program Files/Adobe/Adobe Dreamweaver 2021/node/node.exe'
with (out/'network-boot-process.log').open('wb') as log:
 process=subprocess.Popen([runtime,str(repo/'tools/network-boot-server.cjs'),args.mode],stdout=log,stderr=log,creationflags=0x08000000)
for _ in range(30):
 if process.poll() is not None:raise SystemExit((out/'network-boot-process.log').read_text())
 try:
  with urllib.request.urlopen('http://10.8.22.122:18080/health',timeout=1) as response:report=json.load(response)
  if report['pid']!=process.pid:process.terminate();raise SystemExit('Another boot server is already listening; inspect its identity before restarting')
  assert report['mode']==args.mode;report.update(runtime=runtime)
  (out/'network-boot-state.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2));break
 except OSError:time.sleep(.1)
else:process.terminate();raise SystemExit('Boot server failed readiness')

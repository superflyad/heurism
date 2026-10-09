"""Find HTTP driver request/configuration candidates in an already saved ROM dump.

Runs bounded file searches on extracted files, never live flash or vendor code.
The dump must already exist, and the source saved ROM's pinned hash is checked.
"""
import base64
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
out = repo/'artifacts/research/independent-bootstrap'
remote = r'''
import hashlib,json,pathlib,re,uuid
rom=pathlib.Path('/var/lib/companion/firmware/bios16-a.bin')
expected='09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
assert hashlib.sha256(rom.read_bytes()).hexdigest()==expected
root=pathlib.Path(str(rom)+'.dump'); assert root.is_dir()
targets={'http_file':'ecebcb00-d9c8-11e4-af3d-8cdcd426c973',
         'http_transport_file':'2366c20f-e15a-11e3-8bf1-e4115b28bc50',
         'vendor_interface':'8f63ff6d-b7d4-474d-8c53-68258a224d58',
         'http_policy':'a686d83e-e7e7-4c01-8335-6fccb8e7c024'}
total=count=0; matches=[]
for path in sorted(root.rglob('body.bin')):
 count+=1; assert count<=20000
 size=path.stat().st_size; total+=size; assert total<=268435456
 if size>4194304: continue
 data=path.read_bytes()
 found={name:[hex(m.start()) for m in re.finditer(re.escape(uuid.UUID(g).bytes_le),data)]
        for name,g in targets.items()}
 if not any(found.values()) and ' HttpDxe/' not in str(path): continue
 ancestors=[]
 for parent in [path.parent,*path.parent.parents]:
  if parent==root: break
  info=parent/'info.txt'
  if info.is_file(): ancestors.append({'path':str(parent.relative_to(root)),
      'info':info.read_text(errors='replace')[:4096]})
 matches.append({'path':str(path),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),
                 'mz':data[:2]==b'MZ','matches':found,'ancestors':ancestors[:8]})
assert len(matches)<=200
print(json.dumps({'rom_sha256':expected,'files_scanned':count,'bytes_scanned':total,
                  'matches':matches}))
'''
p = subprocess.run([sys.executable,str(repo/'tools/dell.py'),'--timeout','60','--command',
                    'python3 -c '+shlex.quote(remote)],capture_output=True,text=True,timeout=75)
if p.returncode: raise RuntimeError(p.stderr+p.stdout)
report=json.loads(p.stdout)
(out/'provider-dump-survey.json').write_text(json.dumps(report,indent=2))
for label in ['BindingsDxe','DellPolicyDxe','DellBoardPolicyDxe','DellOnboardNicDxe','HttpDxe']:
    selected=[m for m in report['matches'] if m['mz'] and
              (' '+label+'/') in m['path']]
    assert len(selected)==1
    m=selected[0]; remote_path=shlex.quote(m['path'])
    result=subprocess.run([sys.executable,str(repo/'tools/dell.py'),'--command',
                           'base64 '+remote_path],capture_output=True,text=True,timeout=45)
    if result.returncode: raise RuntimeError(result.stderr+result.stdout)
    binary=base64.b64decode(''.join(result.stdout.split()),validate=True)
    assert hashlib.sha256(binary).hexdigest()==m['sha256']
    (out/('provider-'+label+'.bin')).write_bytes(binary)
    result=subprocess.run([sys.executable,str(repo/'tools/dell.py'),'--command',
                           'objdump -d '+remote_path],capture_output=True,text=True,timeout=45)
    if result.returncode: raise RuntimeError(result.stderr+result.stdout)
    (out/('provider-'+label+'.txt')).write_text(result.stdout)
print(json.dumps({'files_scanned':report['files_scanned'],'bytes_scanned':report['bytes_scanned'],
                 'pe_candidates':len([m for m in report['matches'] if m['mz']]),
                 'http_policy_candidates':len([m for m in report['matches'] if m['mz'] and m['matches']['http_policy']]),
                 'copied_candidates':['BindingsDxe','DellPolicyDxe','DellBoardPolicyDxe','DellOnboardNicDxe','HttpDxe']},indent=2))

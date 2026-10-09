"""Build a self-contained RAM management initramfs from verified packages.

Reads the running Dell's matching kernel and selected Ethernet/USB modules.
Does not change boot registration or installed boot files.
"""
import gzip
import hashlib
import io
import json
from pathlib import Path
import shlex
import stat
import subprocess
import tarfile
repo=Path(__file__).resolve().parents[1];out=repo/'build/network-root';out.mkdir(parents=True,exist_ok=True)
state=json.loads((repo/'artifacts/targets/dell.json').read_text());keys=repo/'artifacts/ssh'
options=['-i',str(keys/'companion_client_ed25519'),'-o','BatchMode=yes','-o','StrictHostKeyChecking=yes',
 '-o','HostKeyAlias=companion-dell','-o','UserKnownHostsFile='+str(keys/'known_hosts')]
target='root@'+state['address']
export=r'''
import json,tarfile,platform
from pathlib import Path
version=platform.release();base=Path('/lib/modules')/version
deps={}
for line in (base/'modules.dep').read_text().splitlines():
 name,rest=line.split(':',1);deps[name]=rest.split()
names={Path(name).name.split('.ko')[0].replace('-','_'):name for name in deps}
selected=set()
def visit(name):
 if name in selected:return
 selected.add(name)
 for dep in deps[name]:visit(dep)
for name in ['r8152','e1000','e1000e','ax88179_178a','xhci_pci','af_packet','virtio_net','virtio_pci']:
 visit(names[name])
stage=Path('/var/lib/companion/network-root-build');stage.mkdir(exist_ok=True)
with tarfile.open(stage/'kernel-modules.tar','w') as t:
 t.add('/boot/vmlinuz-lts',arcname='boot/vmlinuz-lts')
 for name in sorted(selected):t.add(base/name,arcname='lib/modules/'+version+'/'+name)
 for p in sorted(base.glob('modules.*')):
  if p.is_file():t.add(p,arcname='lib/modules/'+version+'/'+p.name)
 fw=Path('/lib/firmware/rtl_nic')
 if fw.exists():
  for p in fw.glob('rtl8153*'):t.add(p,arcname='lib/firmware/rtl_nic/'+p.name)
(stage/'sources.json').write_text(json.dumps({'kernel':version,'modules':sorted(selected)},indent=2))
print(version,len(selected),'selected modules')
'''
subprocess.run(['ssh']+options+[target,'python3 -c '+shlex.quote(export)],check=True,timeout=45)
for name in ['kernel-modules.tar','sources.json']:
 subprocess.run(['scp']+options+[target+':/var/lib/companion/network-root-build/'+name,str(out/name)],check=True,timeout=60)
entries={}
def put(name,data=b'',mode=stat.S_IFREG|0o644):
 name=name.removeprefix('./').strip('/')
 if not name:return
 if '..' in Path(name).parts:raise ValueError('Unsafe archive path')
 entries[name]=(data,mode)
def unpack(t):
 links=[]
 for item in t:
  n=item.name.removeprefix('./').strip('/')
  if not n or n.startswith('.'):continue
  if item.isfile():put(n,t.extractfile(item).read(),stat.S_IFREG|item.mode)
  elif item.isdir():put(n,b'',stat.S_IFDIR|item.mode)
  elif item.issym():put(n,item.linkname.encode(),stat.S_IFLNK|0o777)
  elif item.islnk():links.append((n,item.linkname.removeprefix('./').strip('/')))
 for name,link in links:
  if link not in entries:raise ValueError('Unresolved hardlink '+name)
  entries[name]=entries[link]
source=repo/'build/recovery/payload';manifest=json.loads((source/'manifest.json').read_text())
for p in sorted((source/'apks/x86_64').glob('*.apk')):
 assert hashlib.sha256(p.read_bytes()).hexdigest()==manifest[p.relative_to(source).as_posix()]
 with tarfile.open(p,'r:gz',ignore_zeros=True) as t:unpack(t)
with tarfile.open(out/'kernel-modules.tar') as t:unpack(t)
kernel,_=entries.pop('boot/vmlinuz-lts');(out/'vmlinuz').write_bytes(kernel)
for n in list(entries):
 if n.startswith('etc/runlevels/') or n.startswith('etc/ssh/ssh_host_'):del entries[n]
put('bin/sh',b'busybox',stat.S_IFLNK|0o777)
put('init',b'#!/bin/sh\n/bin/busybox --install -s /bin\nexec /bin/busybox init\n',stat.S_IFREG|0o755)
put('etc/inittab',b'::sysinit:/etc/companion/ram-start\n::respawn:/etc/companion/ram-watch\n::shutdown:/bin/busybox umount -a -r\n')
put('etc/passwd',b'root:x:0:0:Companion RAM management:/root:/bin/sh\nsshd:x:22:22:SSH:/var/empty:/sbin/nologin\n')
put('etc/group',b'root:x:0:\nsshd:x:22:\n')
put('etc/shadow',b'root::0:0:99999:7:::\nsshd:!:0:0:99999:7:::\n',stat.S_IFREG|0o600)
put('etc/hostname',b'companion-dell\n')
put('etc/companion/network-root',b'COMPANION_NETWORK_RAM_ROOT_01\n')
put('etc/companion/host-identity.pub',(keys/'companion_target_ed25519.pub').read_bytes())
put('etc/ssh/ssh_host_ed25519_key',(keys/'companion_target_ed25519').read_bytes(),stat.S_IFREG|0o600)
put('etc/ssh/ssh_host_ed25519_key.pub',(keys/'companion_target_ed25519.pub').read_bytes())
put('root/.ssh/authorized_keys',(keys/'companion_client_ed25519.pub').read_bytes(),stat.S_IFREG|0o600)
put('etc/ssh/sshd_config',b'Port 22\nHostKey /etc/ssh/ssh_host_ed25519_key\nPermitRootLogin prohibit-password\nPasswordAuthentication no\nKbdInteractiveAuthentication no\nPermitEmptyPasswords no\nAuthorizedKeysFile /root/.ssh/authorized_keys\nSubsystem sftp internal-sftp\n')
put('etc/companion/ram-start',(repo/'provisioning/network-ram-start').read_bytes().replace(b'\r\n',b'\n'),stat.S_IFREG|0o755)
put('etc/companion/ram-watch',(repo/'provisioning/network-ram-watch').read_bytes().replace(b'\r\n',b'\n'),stat.S_IFREG|0o755)
put('etc/companion/dhcp-script',(repo/'provisioning/network-dhcp-script').read_bytes().replace(b'\r\n',b'\n'),stat.S_IFREG|0o755)
for name in list(entries):
 parent=Path(name).parent
 while str(parent)!='.':
  n=parent.as_posix()
  if n not in entries:put(n,b'',stat.S_IFDIR|(0o700 if n.startswith('root') else 0o755))
  parent=parent.parent
for name in ['proc','sys','dev','run','tmp','var/empty','dev/pts']:put(name,b'',stat.S_IFDIR|0o755)
put('dev/console',b'',stat.S_IFCHR|0o600)
def record(stream,name,data,mode,inode):
 encoded=name.encode()+b'\0';rmajor,rminor=(5,1) if name=='dev/console' else (0,0)
 fields=[inode,mode,0,0,1,0,len(data),0,0,rmajor,rminor,len(encoded),0]
 stream.write(b'070701'+b''.join(f'{x:08x}'.encode() for x in fields)+encoded)
 stream.write(bytes((-stream.tell())%4));stream.write(data);stream.write(bytes((-stream.tell())%4))
raw=io.BytesIO()
for i,(name,(data,mode)) in enumerate(sorted(entries.items(),key=lambda x:(len(Path(x[0]).parts),x[0])),1):record(raw,name,data,mode,i)
record(raw,'TRAILER!!!',b'',0,len(entries)+1)
(out/'initramfs').write_bytes(gzip.compress(raw.getvalue(),compresslevel=6,mtime=0))
assert not any('companion_client_ed25519' in n for n in entries)
hashes={n:hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ['vmlinuz','initramfs','kernel-modules.tar']}
report={'scope':__doc__,'files':len(entries),'uncompressed_bytes':raw.tell(),'hashes':hashes,
 'kernel_sources':json.loads((out/'sources.json').read_text()),'storage_modules_included':False}
(out/'manifest.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k!='kernel_sources'},indent=2))

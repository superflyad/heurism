"""Check the selected LAN boot payload from authenticated root before reboot."""
import json,shlex,subprocess
from pathlib import Path
repo=Path(__file__).resolve().parents[1];keys=repo/'artifacts/ssh'
command=r'''
import json,urllib.request,socket,hashlib
info=json.load(urllib.request.urlopen('http://10.8.22.122:18080/health',timeout=3))
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);s.settimeout(5)
s.sendto(b'\x00\x01'+info['filename'].encode()+b'\0octet\0blksize\0'+b'1468\0tsize\0'+b'0\0',('10.8.22.122',69))
data=bytearray();expected=0;size=1468
while True:
 packet,peer=s.recvfrom(65535);op=int.from_bytes(packet[:2],'big')
 if op==6:
  fields=packet[2:].rstrip(b'\0').split(b'\0');options=dict(zip(fields[::2],fields[1::2]));size=int(options.get(b'blksize',512));block=0
 elif op==3:
  block=int.from_bytes(packet[2:4],'big')
  if block==expected+1:data.extend(packet[4:]);expected=block
 else:raise RuntimeError(packet)
 s.sendto(b'\x00\x04'+block.to_bytes(2,'big'),peer)
 if op==3 and len(packet)-4<size:break
assert len(data)==info['bytes'];assert hashlib.sha256(data).hexdigest()==info['sha256']
print('PASS: Dell LAN readiness and exact TFTP payload',json.dumps(info))
'''
subprocess.run(['ssh','-i',str(keys/'companion_client_ed25519'),'-o','BatchMode=yes','-o','StrictHostKeyChecking=yes',
 '-o','HostKeyAlias=companion-dell','-o','UserKnownHostsFile='+str(keys/'known_hosts'),'root@10.8.22.238','python3 -c '+shlex.quote(command)],check=True,timeout=180)

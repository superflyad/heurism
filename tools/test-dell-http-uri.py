"""Run only the saved Dell HTTP-boot scheme check offline; no network or boot."""
import hashlib
import json
from pathlib import Path
import struct
import unicorn
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RCX,UC_X86_REG_RAX,UC_X86_REG_RSP,UC_X86_REG_RIP
repo=Path(__file__).resolve().parents[1];out=repo/'artifacts/research/independent-bootstrap'
body=(out/'http-boot-dxe.bin').read_bytes()
assert hashlib.sha256(body).hexdigest()=='d992982aaef66ab249a4811d1a619eac3166e0cc66aef6a75e88014869020788'
assert unicorn.__version__=='2.1.4'
pe=struct.unpack_from('<I',body,60)[0];section=pe+24+struct.unpack_from('<H',body,pe+20)[0]
cases=[]
for uri,expected in [('http://10.8.22.122:18080/companion.efi',0),
                     ('https://10.8.22.122/companion.efi',0),
                     ('HTTPS://owner.example/companion.efi',0),
                     ('ftp://owner.example/companion.efi',0x8000000000000002),
                     ('file:///CompanionExtensionImage01',0x8000000000000002),
                     ('nvram://CompanionExtensionImage01',0x8000000000000002),
                     ('',0x8000000000000002)]:
 cpu=Uc(UC_ARCH_X86,UC_MODE_64);base=0x1000000;cpu.mem_map(base,0x20000)
 for i in range(struct.unpack_from('<H',body,pe+6)[0]):
  _,virtual,rva,size,start=struct.unpack_from('<8sIIII',body,section+40*i)
  assert rva+max(virtual,size)<=0x20000 and start+size<=len(body)
  if size:cpu.mem_write(base+rva,body[start:start+size])
 cpu.mem_map(0x200000,0x10000);source,stack,sentinel=0x201000,0x208008,0x200000
 cpu.mem_write(source,uri.encode()+b'\0');cpu.mem_write(stack,struct.pack('<Q',sentinel))
 cpu.reg_write(UC_X86_REG_RCX,source);cpu.reg_write(UC_X86_REG_RSP,stack)
 def trace(cpu,address,length,data):
  if not base+0x34dc<=address<=base+0x35a7:raise RuntimeError('Unexpected instruction '+hex(address))
 cpu.hook_add(UC_HOOK_CODE,trace)
 cpu.emu_start(base+0x34dc,sentinel,timeout=1000000,count=20000)
 assert cpu.reg_read(UC_X86_REG_RIP)==sentinel
 result=cpu.reg_read(UC_X86_REG_RAX);assert result==expected,(uri,hex(result))
 normalized=bytes(cpu.mem_read(source,len(uri))).decode()
 cases.append({'uri':uri,'status':hex(result),'normalized':normalized})
report={'scope':__doc__,'module_sha256':hashlib.sha256(body).hexdigest(),
 'function_rva':'0x34dc','cases':cases,
 'limits':'Scheme acceptance only: does not establish complete URL validity, protocol publication, plaintext policy, DHCP, TLS trust, transfer, image execution or physical fallback.'}
(out/'uri-scheme-tests.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))

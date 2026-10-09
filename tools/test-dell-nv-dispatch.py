"""Run the saved Dell image-source resolver offline, with mocked providers.

No physical firmware executes, no boot entry changes, no LoadImage/StartImage.
Positive provider fixtures are synthetic controls, not observed Dell services.
"""
import hashlib,json,struct,uuid
from pathlib import Path
import unicorn
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64,UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_RAX,UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9,UC_X86_REG_RSP,UC_X86_REG_RIP
repo=Path(__file__).resolve().parents[1];root=repo/'artifacts/firmware';out=repo/'artifacts/research/nv-dispatch'
body=(root/'update-modules/dxe-core.bin').read_bytes();payload=(root/'nv-extension-payload-readback.bin').read_bytes()
assert hashlib.sha256(body).hexdigest()=='8641816dae6964f620aeac118b0303376f1abe0d3e6be6f066e17cbf52d6c246'
assert hashlib.sha256(payload).hexdigest()=='b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a'
assert unicorn.__version__=='2.1.4'
pe=struct.unpack_from('<I',body,60)[0];count=struct.unpack_from('<H',body,pe+6)[0];section=pe+24+struct.unpack_from('<H',body,pe+20)[0]
fv=uuid.UUID('220e73b6-6bdb-4413-8405-b974b108619a');owner=uuid.UUID('1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1')
end=bytes.fromhex('7fff0400');flash=0xff040494
mempath=struct.pack('<BBHIQQ',1,3,24,11,flash,flash+len(payload)-1)+end
vendorpath=struct.pack('<BBH',1,4,20)+owner.bytes_le+end
fvpath=struct.pack('<BBH',4,7,20)+uuid.UUID('fff12b8d-7696-4c8b-a985-2747075b4f50').bytes_le+struct.pack('<BBH',4,6,20)+owner.bytes_le+end
def execute(name,path,policy=False,provider=False):
 cpu=Uc(UC_ARCH_X86,UC_MODE_64);module=0x1000000;cpu.mem_map(module,0x50000)
 for i in range(count):
  _,virtual,rva,size,start=struct.unpack_from('<8sIIII',body,section+40*i)
  assert rva+max(virtual,size)<=0x50000 and start+size<=len(body)
  if size:cpu.mem_write(module+rva,body[start:start+size])
 cpu.mem_map(0x200000,0x200000);cpu.mem_map(flash&~4095,4096);cpu.mem_write(flash,payload)
 table,locate,handle,readsection=0x220000,0x230000,0x230100,0x230200
 interface,original,copy,buffer,sizeout,authout,stack,sentinel=0x240000,0x250000,0x250100,0x260000,0x270000,0x270010,0x3f8008,0x200000
 cpu.mem_write(module+0x475f8,struct.pack('<Q',table));cpu.mem_write(table+0xb8,struct.pack('<Q',locate));cpu.mem_write(table+0x98,struct.pack('<Q',handle))
 cpu.mem_write(interface+0x18,struct.pack('<Q',readsection));cpu.mem_write(original,path);cpu.mem_write(buffer,payload)
 cpu.mem_write(stack,struct.pack('<Q',sentinel)+bytes(0x80));cpu.mem_write(sizeout,struct.pack('<Q',0xabcdef));cpu.mem_write(authout,struct.pack('<I',0xabcdef))
 for reg,value in [(UC_X86_REG_RSP,stack),(UC_X86_REG_RCX,int(policy)),(UC_X86_REG_RDX,original),(UC_X86_REG_R8,sizeout),(UC_X86_REG_R9,authout)]:cpu.reg_write(reg,value)
 calls=[];flash_reads=[]
 def q(at):return struct.unpack('<Q',cpu.mem_read(at,8))[0]
 def put(at,value):cpu.mem_write(at,struct.pack('<Q',value))
 def ret(value):
  sp=cpu.reg_read(UC_X86_REG_RSP);cpu.reg_write(UC_X86_REG_RAX,value);cpu.reg_write(UC_X86_REG_RSP,sp+8);cpu.reg_write(UC_X86_REG_RIP,q(sp))
 def trace(cpu,address,length,data):
  if address==module+0xf0e4:
   assert cpu.reg_read(UC_X86_REG_RCX)==original;cpu.mem_write(copy,path);ret(copy)
  elif address==module+0xad70:
   assert cpu.reg_read(UC_X86_REG_RCX)==copy;ret(0)
  elif address==locate:
   guid=uuid.UUID(bytes_le=bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_RCX),16)));calls.append(str(guid))
   if provider and guid==fv:
    put(cpu.reg_read(UC_X86_REG_RDX),copy+20);put(cpu.reg_read(UC_X86_REG_R8),0x123456);ret(0)
   else:ret(0x800000000000000e)
  elif address==handle:
   assert provider and uuid.UUID(bytes_le=bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_RDX),16)))==fv
   put(cpu.reg_read(UC_X86_REG_R8),interface);ret(0)
  elif address==readsection:
   assert provider and uuid.UUID(bytes_le=bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_RDX),16)))==owner
   assert cpu.reg_read(UC_X86_REG_R8)&255==0x10
   sp=cpu.reg_read(UC_X86_REG_RSP);put(q(sp+40),buffer);put(q(sp+48),len(payload));cpu.mem_write(q(sp+56),struct.pack('<I',0));ret(0)
  elif not module+0x11308<=address<=module+0x117ea:raise RuntimeError('Unexpected execution '+hex(address))
 def memory(cpu,access,address,length,value,data):
  if flash<=address<flash+len(payload):flash_reads.append(hex(address))
 cpu.hook_add(UC_HOOK_CODE,trace);cpu.hook_add(UC_HOOK_MEM_READ,memory)
 cpu.emu_start(module+0x11308,sentinel,timeout=1000000,count=5000)
 assert cpu.reg_read(UC_X86_REG_RIP)==sentinel
 result=cpu.reg_read(UC_X86_REG_RAX);assert result==(buffer if provider else 0);assert not flash_reads
 if provider:assert q(sizeout)==len(payload) and bytes(cpu.mem_read(result,len(payload)))==payload
 return {'name':name,'boot_policy':policy,'synthetic_provider':provider,'return_buffer':hex(result),'protocol_lookups':calls,'direct_flash_reads':len(flash_reads),'returned_bytes':q(sizeout) if provider else 0}
cases=[execute('memory-mapped-flash-no-provider',mempath),execute('memory-mapped-flash-boot-policy',mempath,True),
 execute('vendor-nvram-guid-no-provider',vendorpath),execute('nvram-volume-and-owner-guid-no-provider',fvpath),
 execute('synthetic-fv-provider-positive-control',fvpath,provider=True)]
report={'scope':__doc__,'dell_core_sha256':hashlib.sha256(body).hexdigest(),'resolver_rva':'0x11308','emulator':unicorn.__version__,'cases':cases,
 'conclusion':'This Dell resolver needs a matching FV/filesystem/LoadFile provider. A raw memory-mapped address or variable GUID alone does not cause direct flash reads. No exhaustive vendor-module claim.'}
(out/'dell-resolver-tests.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

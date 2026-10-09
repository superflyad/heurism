"""Execute saved Dell IPv4 Supported, full Start and Stop with EFI fixtures.

All internal driver instructions, including HII setup/removal and path/string
helpers, execute unmodified. External boot/runtime/protocol services are an
explicit stateful model, not live firmware. Unknown calls fail closed.
"""
import importlib.util
import json
from pathlib import Path
import struct
import uuid
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RAX

spec=importlib.util.spec_from_file_location('http_entry_fixture',Path(__file__).with_name('test-dell-http-entry.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
PRIVATE='ecebcb00-d9c8-11e4-af3d-8cdcd426c973'
DHCP='8a219718-4ef5-4761-91c8-c0f04bda9e56'
SB='9d9a39d8-bd42-4a73-a4d5-8ee94be11380'
HTTP_SB='bdc8e6af-d9bc-4379-a72a-e0c4e75dae1c'
LOAD='56ec3091-954c-11d2-8e3f-00a0c969723b'
DP='09576e91-6d3f-11d2-8e39-00a0c969723b'
SNP='a19832b9-ac25-11d3-9a2d-0090273fc14d'
IP4='5b446ed1-e30b-4faa-871a-3654eca36080'
HII='330d4706-f2a0-4e4f-a369-b66fa8d54385'
NF=m.NOT_FOUND;SMALL=0x8000000000000005;DENIED=0x800000000000000f

class Machine:
 def __init__(self,fail=None):
  self.cpu=Uc(UC_ARCH_X86,UC_MODE_64);self.cpu.mem_map(m.base,len(m.mapped));self.cpu.mem_write(m.base,bytes(m.mapped));self.cpu.mem_map(0x200000,0x400000)
  self.table=0x202000;self.runtime=0x203000;self.sentinel=0x200000;self.stack=0x5f8008;self.heap=0x300000
  self.nic=0x1111;self.agent=0x4444;self.next_handle=0x5000;self.fail=fail
  self.allocations={};self.freed=set();self.opens=[];self.trace=[];self.packages=set();self.created=[];self.destroyed=[]
  self.initial_protocols={(self.nic,SB):0x250000,(self.nic,HTTP_SB):0x251000,(self.nic,DP):0x252000,(self.nic,SNP):0x253000,(self.nic,IP4):0x255000}
  self.protocols=dict(self.initial_protocols)
  for off in range(24,384,8):self.w(self.table+off,0x210000+off)
  for off in range(24,136,8):self.w(self.runtime+off,0x220000+off)
  for off,value in [(0xd6a0,self.table),(0xd6b0,self.runtime),(0xd6c8,0x261000),(0xd6e0,0x262000)]:self.w(m.base+off,value)
  self.w(m.base+0xc188+40,self.agent)
  self.w(0x250000,0x280000);self.w(0x250008,0x280008)
  for off in range(0,40,8):self.w(0x261000+off,0x281000+off)
  for off in range(0,56,8):self.w(0x262000+off,0x282000+off)
  self.w(0x253000+0x78,0x254000)
  self.cpu.mem_write(0x254004,struct.pack('<I',6));self.cpu.mem_write(0x254228,bytes.fromhex('7cc2c61db2f5'))
  self.cpu.mem_write(0x252000,bytes([3,11,37,0])+bytes.fromhex('7cc2c61db2f5')+bytes(27)+bytes([127,255,4,0]))
  self.cpu.hook_add(UC_HOOK_CODE,self.hook)
 def w(self,a,v):self.cpu.mem_write(a,struct.pack('<Q',v))
 def r(self,a):return struct.unpack('<Q',bytes(self.cpu.mem_read(a,8)))[0]
 def guid(self,a):return str(uuid.UUID(bytes_le=bytes(self.cpu.mem_read(a,16))))
 def alloc(self,size):
  assert 0<size<=0x10000 and self.heap+size<0x500000
  address=self.heap;self.heap+=(size+15)&~15;self.allocations[address]=size;return address
 def ret(self,v):
  rsp=self.cpu.reg_read(UC_X86_REG_RSP);self.cpu.reg_write(UC_X86_REG_RAX,v);self.cpu.reg_write(UC_X86_REG_RIP,self.r(rsp));self.cpu.reg_write(UC_X86_REG_RSP,rsp+8)
 def call(self,rva,*args):
  self.w(self.stack,self.sentinel);self.cpu.reg_write(UC_X86_REG_RSP,self.stack)
  for register,value in zip((UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9),args):self.cpu.reg_write(register,value)
  self.cpu.emu_start(m.base+rva,self.sentinel,timeout=3000000,count=500000)
  assert self.cpu.reg_read(UC_X86_REG_RIP)==self.sentinel,hex(self.cpu.reg_read(UC_X86_REG_RIP))
  return self.cpu.reg_read(UC_X86_REG_RAX)
 def hook(self,cpu,address,size,data):
  if m.base<=address<m.base+len(m.mapped):return
  args=[cpu.reg_read(x) for x in (UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9)];rsp=cpu.reg_read(UC_X86_REG_RSP)
  record={'address':hex(address),'caller_rva':hex(self.r(rsp)-m.base)};self.trace.append(record)
  if 0x210000<=address<0x210180:
   off=address-0x210000;record['boot_service']=hex(off)
   if off==0x40:self.w(args[2],self.alloc(args[1]));self.ret(0)
   elif off==0x48:
    assert args[0] in self.allocations and args[0] not in self.freed,('invalid/double free',hex(args[0]))
    self.freed.add(args[0]);self.ret(0)
   elif off in (0x118,0x98):
    key=(args[0],self.guid(args[1]));record['protocol']=key[1];value=self.protocols.get(key)
    attr=self.r(rsp+48) if off==0x118 else 2
    record.update(handle=hex(args[0]),attributes=attr)
    if self.fail=='ip4_open' and key==(self.nic,IP4):self.ret(DENIED);return
    if value:
     if args[2]:self.w(args[2],value)
     if attr in (8,16):self.opens.append((key,args[3],self.r(rsp+40),attr))
    self.ret(0 if value else NF)
   elif off==0x120:
    key=(args[0],self.guid(args[1]));matching=[x for x in self.opens if x[:3]==(key,args[2],args[3])]
    assert len(matching)<=1
    if not matching:record['unmatched_close']=True;self.ret(NF);return
    self.opens.remove(matching[0]);self.ret(0)
   elif off==0x128:
    key=(args[0],self.guid(args[1]));entries=[x for x in self.opens if x[0]==key]
    if key not in self.protocols:self.ret(NF);return
    pointer=self.alloc(max(24,len(entries)*24))
    for i,(_,agent,controller,attr) in enumerate(entries):cpu.mem_write(pointer+i*24,struct.pack('<QQII',agent,controller,attr,1))
    self.w(args[2],pointer);self.w(args[3],len(entries));self.ret(0)
   elif off in (0x80,0x148,0x90,0x150):
    install=off in (0x80,0x148);single=off in (0x80,0x90)
    if single:handle=self.r(args[0]) if install else args[0];pairs=[(self.guid(args[1]),args[3] if install else args[2])]
    else:
     handle=self.r(args[0]) if install else args[0];raw=args[1:]+[self.r(rsp+40+i*8) for i in range(7)];pairs=[]
     for i in range(0,len(raw)-1,2):
      if not raw[i]:break
      pairs.append((self.guid(raw[i]),raw[i+1]))
     else:raise RuntimeError('unterminated protocol list')
    record.update(install=install,handle=hex(handle),protocols=[g for g,_ in pairs])
    if not install and ((self.fail=='http_uninstall' and any(g==LOAD for g,_ in pairs)) or (self.fail=='hii_uninstall' and any(g==HII for g,_ in pairs))):self.ret(DENIED);return
    if not install:
     if not handle or any(self.protocols.get((handle,g))!=value for g,value in pairs):
      record['absent_protocol_removal']=True;self.ret(0x8000000000000002);return
     if any(x[0]==(handle,g) for g,_ in pairs for x in self.opens):self.ret(DENIED);return
    if install and not handle:handle=self.next_handle;self.next_handle+=1;self.w(args[0],handle)
    for g,value in pairs:
     key=(handle,g)
     if install:assert key not in self.protocols;self.protocols[key]=value
     else:
      assert self.protocols[key]==value,(key,hex(value));assert not any(x[0]==key for x in self.opens),('still opened',key)
      del self.protocols[key]
    self.ret(0)
   else:raise RuntimeError('unreviewed boot service '+hex(off))
  elif address==0x220048:
   record['runtime']='GetVariable';self.ret(NF)
  elif address==0x280000:
   assert args[0]==0x250000
   if self.fail=='dhcp_create':self.ret(DENIED);return
   child=self.next_handle;self.next_handle+=1;self.protocols[(child,DHCP)]=0x256000;self.created.append(child);self.w(args[1],child);self.ret(0)
  elif address==0x280008:
   assert args[0]==0x250000 and (args[1],DHCP) in self.protocols
   if self.fail=='dhcp_destroy':self.ret(DENIED);return
   assert not any(x[0]==(args[1],DHCP) for x in self.opens)
   del self.protocols[(args[1],DHCP)];self.destroyed.append(args[1]);self.ret(0)
  elif address==0x282000:
   # HII database NewPackageList: the driver builds the real pinned packages.
   assert args[0]==0x262000 and (args[2],HII) in self.protocols
   header=bytes(cpu.mem_read(args[1],20));length=struct.unpack_from('<I',header,16)[0];assert 24<length<0x10000
   if self.fail=='hii_add':self.ret(DENIED);return
   package=0x7777;self.packages.add(package);self.w(args[3],package);self.ret(0)
  elif address==0x282008:
   assert args[:2]==[0x262000,0x7777] and args[1] in self.packages
   if self.fail=='hii_remove':self.ret(DENIED);return
   self.packages.remove(args[1]);self.ret(0)
  elif address==0x281018:
   assert args[:2]==[0x261000,0x7777]
   language=b'en-US\0';size=self.r(args[3]);self.w(args[3],len(language))
   if size<len(language):self.ret(SMALL)
   else:cpu.mem_write(args[2],language);self.ret(0)
  elif address==0x281008:
   assert args[0]==0x261000 and args[2]==0x7777 and args[3]==3
   buffer=self.r(rsp+40);sizeptr=self.r(rsp+48);text='HTTP Boot\0'.encode('utf-16-le');size=self.r(sizeptr);self.w(sizeptr,len(text))
   if size<len(text):self.ret(SMALL)
   else:cpu.mem_write(buffer,text);self.ret(0)
  elif address==0x281010:
   assert args[:3]==[0x261000,0x7777,3];assert self.r(rsp+40);self.ret(0)
  else:raise RuntimeError('unreviewed interface/instruction '+hex(address))
 def state(self):
  leftovers=[{'handle':hex(h),'guid':g,'pointer':hex(p)} for (h,g),p in self.protocols.items() if (h,g) not in self.initial_protocols]
  dangling=[]
  for item in leftovers:
   p=int(item['pointer'],16)
   if any(a<=p<a+self.allocations[a] for a in self.freed):dangling.append(item)
  return {'remaining_protocols':leftovers,'owned_open_records':len(self.opens),'hii_packages':len(self.packages),'outstanding_allocations':[{'pointer':hex(a),'size':n} for a,n in self.allocations.items() if a not in self.freed],'dangling_protocols':dangling,'dhcp_created':self.created,'dhcp_destroyed':self.destroyed}

def execute(stop_handle='original_nic',fail=None):
 machine=Machine(fail);binding=m.base+0xc188
 assert machine.call(0xf54,binding,machine.nic,0)==0
 start=machine.call(0xffc,binding,machine.nic,0)
 after_start=machine.state();stop=None
 if not start:
  handle=machine.nic if stop_handle=='original_nic' else machine.created[0]
  stop=machine.call(0x13e0,binding,handle,0,0)
 return {'stop_handle':stop_handle,'failure_fixture':fail,'start_status':hex(start),'stop_status':hex(stop) if stop is not None else None,'after_start':after_start,'after_stop':machine.state(),'trace':machine.trace}

if __name__=='__main__':
 cases=[execute(),execute('dhcp_child')]+[execute('dhcp_child',fail) for fail in ['dhcp_create','ip4_open','hii_add','http_uninstall','hii_uninstall','dhcp_destroy','hii_remove']]
 assert cases[0]['start_status']==cases[0]['stop_status']=='0x0'
 assert len(cases[0]['after_stop']['remaining_protocols'])==6 and cases[0]['after_stop']['owned_open_records']==2
 nominal=cases[1]['after_stop']
 assert cases[1]['start_status']==cases[1]['stop_status']=='0x0'
 assert not nominal['remaining_protocols'] and not nominal['owned_open_records'] and not nominal['hii_packages'] and not nominal['dangling_protocols']
 assert len(nominal['outstanding_allocations'])==1 and nominal['outstanding_allocations'][0]['size']==72
 assert nominal['dhcp_created']==nominal['dhcp_destroyed']
 for c in cases[2:5]:
  assert c['start_status']!='0x0' and c['stop_status'] is None
  assert not c['after_stop']['remaining_protocols'] and not c['after_stop']['outstanding_allocations'] and not c['after_stop']['owned_open_records'] and not c['after_stop']['hii_packages']
 assert len(cases[5]['after_stop']['dangling_protocols'])==1
 assert len(cases[6]['after_stop']['dangling_protocols'])==2
 assert len(cases[7]['after_stop']['remaining_protocols'])==1 and not cases[7]['after_stop']['dhcp_destroyed']
 assert cases[8]['after_stop']['hii_packages']==1
 assert all(c['stop_status']=='0x0' for c in cases[5:])
 report={'cases_passed':9,'module_sha256':m.digest,'scope':__doc__,'cases':cases,'limits':'Protocol callbacks and firmware services are fixtures. No hardware execution, DHCP transaction, HTTP request or firmware changes. StartImage constructor is covered separately. Only IPv4 before LoadFile transfer is exercised. These assertions confirm observed behavior, including unsafe cleanup; they are not a safety certification.'}
 (m.out/'lifecycle-tests.json').write_text(json.dumps(report,indent=2))
 print(json.dumps([{k:v for k,v in c.items() if k not in ('trace','after_start')} for c in cases],indent=2))

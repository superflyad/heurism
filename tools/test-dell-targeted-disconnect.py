"""Replay saved Dell DXE DisconnectController plus HTTP Start/Stop offline.

The dispatcher body and HTTP driver run unmodified. Core handle validation,
locking, protocol lookup and pool helpers are explicit fixtures. The linked
handle database is synthesized from recorded fixture opens, not live memory.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
from unicorn.x86_const import UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9,UC_X86_REG_RSP,UC_X86_REG_RIP,UC_X86_REG_RAX

spec=importlib.util.spec_from_file_location('lifecycle',Path(__file__).with_name('test-dell-http-lifecycle.py'))
life=importlib.util.module_from_spec(spec);spec.loader.exec_module(life)
root=Path(__file__).resolve().parents[1]
core=(root/'artifacts/firmware/update-modules/dxe-core.bin').read_bytes()
core_hash=hashlib.sha256(core).hexdigest()
assert core_hash=='8641816dae6964f620aeac118b0303376f1abe0d3e6be6f066e17cbf52d6c246'
pe=struct.unpack_from('<I',core,60)[0];optional=pe+24
sections=optional+struct.unpack_from('<H',core,pe+20)[0]
core_base=0x1200000;image=bytearray(0x60000)
assert struct.unpack_from('<Q',core,optional+24)[0]==0
for i in range(struct.unpack_from('<H',core,pe+6)[0]):
 _,virtual,rva,size,raw=struct.unpack_from('<8sIIII',core,sections+40*i)
 assert rva+max(virtual,size)<=len(image) and raw+size<=len(core)
 image[rva:rva+size]=core[raw:raw+size]
reloc,length=struct.unpack_from('<II',core,optional+112+5*8);at=reloc
while at<reloc+length:
 page,size=struct.unpack_from('<II',image,at);assert size>=8 and size%2==0 and at+size<=reloc+length
 for i in range(at+8,at+size,2):
  entry=struct.unpack_from('<H',image,i)[0];kind,offset=entry>>12,entry&4095
  if not kind:continue
  assert kind==10 and page+offset+8<=len(image)
  value=struct.unpack_from('<Q',image,page+offset)[0];struct.pack_into('<Q',image,page+offset,value+core_base)
 at+=size
assert at==reloc+length
# Derive the tested dispatcher from the saved boot-services table, not a
# guessed function name or upstream implementation address.
assert bytes(image[0x291d0:0x291d8])==b'BOOTSERV'
assert struct.unpack_from('<Q',image,0x291d0+0x110)[0]==core_base+0x4a2c
BINDING='18a031ab-b443-4d1a-a5c0-0c09261e9f71'

class Machine(life.Machine):
 def __init__(self,fail=None):
  super().__init__(fail)
  self.cpu.mem_map(core_base,len(image));self.cpu.mem_write(core_base,bytes(image))
  self.cpu.mem_map(0x1000,0x10000)
  self.old_child=0x6000;self.old_agent=0x7776
  self.initial_protocols[(self.old_child,life.DHCP)]=0x256100
  self.initial_protocols[(self.agent,BINDING)]=life.m.base+0xc188
  self.protocols.update(self.initial_protocols)
  self.old_open=((self.old_child,life.DHCP),self.old_agent,self.nic,16)
  self.opens.append(self.old_open)
  self.stops=[];self.core_calls=[];self.lock_depth=0
  self.valid_handles=set();self.database_cursor=0x270000
 def database(self):
  self.valid_handles={h for h,_ in self.protocols}|{self.agent,self.old_agent}
  for handle in self.valid_handles:
   head=handle+24;nodes=[]
   for key in self.protocols:
    if key[0]!=handle:continue
    node=self.database_cursor;self.database_cursor+=0x100;nodes.append(node)
    records=[]
    for opened,agent,controller,attr in self.opens:
     if opened!=key:continue
     record=self.database_cursor;self.database_cursor+=0x100;records.append(record)
     self.w(record+16,agent);self.w(record+24,controller)
     self.cpu.mem_write(record+32,struct.pack('<II',attr,1))
    self.ring(node+56,records)
   self.ring(head,nodes)
 def ring(self,head,nodes):
  self.w(head,nodes[0] if nodes else head);self.w(head+8,nodes[-1] if nodes else head)
  for i,node in enumerate(nodes):self.w(node,nodes[i+1] if i+1<len(nodes) else head);self.w(node+8,nodes[i-1] if i else head)
 def hook(self,cpu,address,size,data):
  if address==life.m.base+0x13e0:
   args=[cpu.reg_read(x) for x in (UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9)]
   self.stops.append({'binding':hex(args[0]),'controller':hex(args[1]),'children':args[2]})
  if core_base<=address<core_base+len(image):
   rva=address-core_base
   if 0x4a2c<=rva<=0x4e51:return
   args=[cpu.reg_read(x) for x in (UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9)]
   self.core_calls.append({'helper_rva':hex(rva),'args':[hex(v) for v in args]})
   if rva==0x32dc:self.ret(0 if args[0] in self.valid_handles else 0x8000000000000002)
   elif rva==0xcd58:self.lock_depth+=1;self.ret(0)
   elif rva==0xbe28:assert self.lock_depth==1;self.lock_depth-=1;self.ret(0)
   elif rva==0x3b8c:
    assert self.guid(args[1])==BINDING
    if args[0]==self.agent:self.w(args[2],life.m.base+0xc188);self.ret(0)
    else:self.ret(life.NF)
   elif rva==0xec54:self.ret(self.alloc(args[0]))
   elif rva==0xad70:
    assert args[0] in self.allocations and args[0] not in self.freed;self.freed.add(args[0]);self.ret(0)
   else:raise RuntimeError('Unreviewed core helper/instruction '+hex(rva))
   return
  before=self.next_handle
  super().hook(cpu,address,size,data)
  # Unlike opaque handle IDs in the driver-only fixture, DXE dereferences
  # IHANDLE structures. Reserve disjoint space for each synthesized handle.
  if self.next_handle!=before:self.next_handle=before+0x100
 def disconnect(self,handle,agent):
  self.database();self.w(self.stack,self.sentinel);self.cpu.reg_write(UC_X86_REG_RSP,self.stack)
  for register,value in zip((UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8),(handle,agent,0)):self.cpu.reg_write(register,value)
  try:self.cpu.emu_start(core_base+0x4a2c,self.sentinel,timeout=3000000,count=500000)
  except Exception as error:raise RuntimeError('Dispatcher replay stopped at '+hex(self.cpu.reg_read(UC_X86_REG_RIP))+'; helpers '+str(self.core_calls[-3:])) from error
  assert self.cpu.reg_read(UC_X86_REG_RIP)==self.sentinel and self.lock_depth==0
  return self.cpu.reg_read(UC_X86_REG_RAX)
 def preserved(self):
  return all(self.protocols.get(key)==value for key,value in self.initial_protocols.items()) and self.old_open in self.opens

def execute(target='owned_child',fail=None):
 machine=Machine(fail);binding=life.m.base+0xc188
 assert machine.call(0xf54,binding,machine.nic,0)==0
 assert machine.call(0xffc,binding,machine.nic,0)==0
 handle=machine.created[0] if target=='owned_child' else machine.nic if target=='original_nic' else machine.old_child if target=='existing_child' else 0
 matching=[x for x in machine.opens if x==((handle,life.DHCP),machine.agent,machine.nic,16)]
 approved=len(matching)==1 and handle not in {h for h,_ in machine.initial_protocols}
 status=machine.disconnect(handle,machine.agent)
 after=machine.state();after['owned_open_records']=len([x for x in machine.opens if x[1]==machine.agent])
 assert machine.preserved(), 'Existing firmware interfaces or DHCP child changed'
 assert all(s['controller']==hex(machine.created[0]) for s in machine.stops),machine.stops
 complete=(approved and status==0 and not after['remaining_protocols'] and
           not after['owned_open_records'] and not after['hii_packages'] and
           not after['dangling_protocols'] and machine.created==machine.destroyed)
 return {'target':target,'failure_fixture':fail,'preflight_approved':approved,
  'disconnect_status':hex(status),'stop_calls':machine.stops,'after':after,
  'existing_firmware_preserved':True,'postconditions_passed':complete,
  'management_handoff_allowed':complete,'reset_required_before_handoff':not complete,
  'core_helpers':machine.core_calls}

if __name__=='__main__':
 cases=[execute()]+[execute(t) for t in ['original_nic','existing_child','invalid_handle']]+[execute(fail=f) for f in ['http_uninstall','hii_uninstall','dhcp_destroy','hii_remove']]
 clean=cases[0]['after'];assert cases[0]['disconnect_status']=='0x0' and len(cases[0]['stop_calls'])==1
 assert not clean['remaining_protocols'] and not clean['owned_open_records'] and not clean['hii_packages'] and not clean['dangling_protocols']
 assert len(clean['outstanding_allocations'])==1 and clean['outstanding_allocations'][0]['size']==72
 for c in cases[1:4]:assert not c['stop_calls'] and c['disconnect_status']!='0x0' and c['after']['remaining_protocols']
 for c in cases[4:]:assert c['disconnect_status']=='0x0' and (c['after']['remaining_protocols'] or c['after']['hii_packages'])
 assert cases[0]['management_handoff_allowed'] and not cases[0]['reset_required_before_handoff']
 assert all(not c['preflight_approved'] for c in cases[1:4])
 assert all(not c['management_handoff_allowed'] and c['reset_required_before_handoff'] for c in cases[1:])
 report={'scope':__doc__,'cases_passed':len(cases),'dxe_core_sha256':core_hash,'http_driver_sha256':life.m.digest,'dispatcher_rva':'0x4a2c','cases':cases,'limits':'Synthetic handle database and core helper fixtures. Does not measure live DXE core code ownership or prove physical cleanup/reset. Only targeted DriverImageHandle and NULL ChildHandle are exercised. Vendor cleanup failure bugs remain. Postcondition decisions are offline assertions; no physical handoff/reset policy is deployed.'}
 destination=life.m.out/'targeted-disconnect-tests.json';destination.write_text(json.dumps(report,indent=2))
 print(json.dumps([{'target':c['target'],'failure':c['failure_fixture'],'status':c['disconnect_status'],'stop_calls':len(c['stop_calls']),'leftovers':len(c['after']['remaining_protocols']),'dangling':len(c['after']['dangling_protocols']),'handoff_allowed':c['management_handoff_allowed']} for c in cases],indent=2))

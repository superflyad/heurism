"""Replay real saved IPv4 Stop instructions over explicit attached-state fixtures.

This is not a real Start/Stop test. HII initialization is deliberately absent,
so its cleanup is not covered. Unknown EFI/interface calls fail closed.
"""
import importlib.util
import json
from pathlib import Path
import struct
import uuid
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RAX
path=Path(__file__).with_name('test-dell-http-entry.py')
spec=importlib.util.spec_from_file_location('entry_fixture',path)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
PRIVATE_GUID='ecebcb00-d9c8-11e4-af3d-8cdcd426c973'
DHCP='8a219718-4ef5-4761-91c8-c0f04bda9e56'
SB='9d9a39d8-bd42-4a73-a4d5-8ee94be11380'
LOAD='56ec3091-954c-11d2-8e3f-00a0c969723b'
DP='09576e91-6d3f-11d2-8e39-00a0c969723b'
def execute(controller):
 cpu=Uc(UC_ARCH_X86,UC_MODE_64);cpu.mem_map(m.base,len(m.mapped));cpu.mem_write(m.base,bytes(m.mapped))
 cpu.mem_map(0x200000,0x200000)
 table=0x202000;sentinel=0x200000;stack=0x3f8008;private=0x300000;ip4=0x310000
 nic=0x1111;dhcp=0x2222;http=0x3333;agent=0x4444
 def w(a,v):cpu.mem_write(a,struct.pack('<Q',v))
 def r(a):return struct.unpack('<Q',bytes(cpu.mem_read(a,8)))[0]
 def guid(a):return str(uuid.UUID(bytes_le=bytes(cpu.mem_read(a,16))))
 def ret(v):
  rsp=cpu.reg_read(UC_X86_REG_RSP);cpu.reg_write(UC_X86_REG_RAX,v);cpu.reg_write(UC_X86_REG_RIP,r(rsp));cpu.reg_write(UC_X86_REG_RSP,rsp+8)
 for offset in range(24,384,8):w(table+offset,0x210000+offset)
 w(m.base+0xd6a0,table);w(m.base+0xc188+40,agent)
 cpu.mem_write(private,struct.pack('<I',0x44504248));w(private+8,nic);w(private+16,ip4);w(private+40,dhcp)
 w(private+0x4d8,private+0x4d8);w(private+0x4e0,private+0x4d8)
 cpu.mem_write(ip4,struct.pack('<I',0x4e564248));w(ip4+8,http);w(ip4+16,agent);w(ip4+24,m.base+0x2324);w(ip4+32,0x320000);w(ip4+40,private)
 protocols={(nic,PRIVATE_GUID):private+0x150,(nic,SB):0x250000,(dhcp,DHCP):0x251000,(http,LOAD):ip4+24,(http,DP):0x320000}
 w(0x250000,0x280000);w(0x250008,0x280010)
 trace=[];freed=[];destroyed=[]
 def hook(uc,a,size,data):
  if m.base<=a<m.base+len(m.mapped):return
  args=[uc.reg_read(x) for x in (UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9)];rsp=uc.reg_read(UC_X86_REG_RSP)
  if 0x210000<=a<0x210180:
   offset=a-0x210000;record={'offset':hex(offset),'caller_rva':hex(r(rsp)-m.base)};trace.append(record)
   if offset==0x118:
    g=guid(args[1]);record.update(guid=g,handle=hex(args[0]),attributes=r(rsp+48));value=protocols.get((args[0],g))
    if value and args[2]:w(args[2],value)
    ret(0 if value else m.NOT_FOUND)
   elif offset==0x128:
    g=guid(args[1]);record.update(guid=g,handle=hex(args[0]))
    if args[0]==dhcp and g==DHCP:
     cpu.mem_write(0x260000,struct.pack('<QQII',agent,nic,0x10,1));w(args[2],0x260000);w(args[3],1);ret(0)
    else:ret(m.NOT_FOUND)
   elif offset==0x120:
    g=guid(args[1]);assert (args[0],g,args[2],args[3]) in [(dhcp,DHCP,agent,nic),(nic,PRIVATE_GUID,agent,http)];ret(0)
   elif offset==0x90:
    key=(args[0],guid(args[1]));assert protocols[key]==args[2];del protocols[key];ret(0)
   elif offset==0x150:
    pairs=args[1:]+[r(rsp+40+i*8) for i in range(5)]
    for i in range(0,len(pairs)-1,2):
     if not pairs[i]:break
     key=(args[0],guid(pairs[i]));assert protocols[key]==pairs[i+1];del protocols[key]
    ret(0)
   elif offset==0x48:
    assert args[0] in [private,ip4,0x260000];freed.append(args[0]);ret(0)
   else:raise RuntimeError('Unreviewed boot service '+hex(offset))
  elif a==0x280010:
   assert args[:2]==[0x250000,dhcp];destroyed.append(dhcp);del protocols[(dhcp,DHCP)];ret(0)
  else:raise RuntimeError('Unreviewed interface/instruction '+hex(a))
 cpu.hook_add(UC_HOOK_CODE,hook)
 w(stack,sentinel);cpu.reg_write(UC_X86_REG_RSP,stack);cpu.reg_write(UC_X86_REG_RCX,m.base+0xc188);cpu.reg_write(UC_X86_REG_RDX,nic if controller=='original_nic' else dhcp);cpu.reg_write(UC_X86_REG_R8,0);cpu.reg_write(UC_X86_REG_R9,0)
 cpu.emu_start(m.base+0x13e0,sentinel,timeout=1000000,count=30000)
 assert cpu.reg_read(UC_X86_REG_RIP)==sentinel and cpu.reg_read(UC_X86_REG_RAX)==0
 return {'controller':controller,'status':'0x0','private_present':(nic,PRIVATE_GUID) in protocols,'http_child_present':(http,LOAD) in protocols,'dhcp_child_destroyed':bool(destroyed),'private_freed':private in freed,'trace':trace}
cases=[execute('original_nic'),execute('dhcp_child')]
assert cases[0]['private_present'] and cases[0]['http_child_present'] and not cases[0]['private_freed']
assert not cases[1]['private_present'] and not cases[1]['http_child_present'] and cases[1]['private_freed'] and cases[1]['dhcp_child_destroyed']
report={'module_sha256':m.digest,'cases_passed':2,'scope':__doc__,'cases':cases,'limits':'Synthetic state, not full Start replay. Original NIC lacks MNP/DHCP protocol open records in this fixture; live records could differ. DHCP-child cleanup is not authorization to bypass UEFI driver call restrictions. No live attachment performed.'}
(m.out/'stop-tests.json').write_text(json.dumps(report,indent=2))
print(json.dumps([{k:v for k,v in c.items() if k!='trace'} for c in cases],indent=2))

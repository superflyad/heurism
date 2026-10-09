"""Replay bounded saved HttpDxe URI and duplicate-token gates offline.

These instruction slices do not perform a request, configure TLS, or access
hardware. They cannot attribute the prior physical EFI_ACCESS_DENIED result.
"""
import hashlib
import json
from pathlib import Path
import struct
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_RDI,UC_X86_REG_RSI,UC_X86_REG_RBP,
    UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_RSP,UC_X86_REG_RIP,UC_X86_REG_RAX)

repo=Path(__file__).resolve().parents[1]
out=repo/'artifacts/research/independent-bootstrap'
body=(out/'provider-HttpDxe.bin').read_bytes()
survey=json.loads((out/'provider-dump-survey.json').read_text())
records=[m for m in survey['matches'] if m['mz'] and ' HttpDxe/' in m['path']]
assert len(records)==1 and hashlib.sha256(body).hexdigest()==records[0]['sha256']
assert records[0]['sha256']=='9c83888402e904cd0866fd5b8415a73c6c3ce66a629be4dde992d7c806d87831'
assert unicorn.__version__=='2.1.4'
pe=struct.unpack_from('<I',body,60)[0]
section=pe+24+struct.unpack_from('<H',body,pe+20)[0]
mapped=bytearray(0x20000); base=0x1000000
for i in range(struct.unpack_from('<H',body,pe+6)[0]):
    _,virtual,rva,size,start=struct.unpack_from('<8sIIII',body,section+40*i)
    assert rva+max(virtual,size)<=len(mapped) and start+size<=len(body)
    mapped[rva:rva+size]=body[start:start+size]
assert bytes(mapped[0x9228:0x9231])==b'https://\0'
def machine(first,last):
    cpu=Uc(UC_ARCH_X86,UC_MODE_64)
    cpu.mem_map(base,len(mapped)); cpu.mem_write(base,bytes(mapped))
    cpu.mem_map(0x200000,0x100000)
    def guard(uc,address,size,data):
        if not base+first<=address<=base+last:
            raise RuntimeError('Unexpected instruction '+hex(address))
    cpu.hook_add(UC_HOOK_CODE,guard)
    return cpu

uri_cases=[]
for uri,expected in [('http://owner.example/companion.efi',0),
                     ('https://owner.example/companion.efi',1),
                     ('HTTPS://owner.example/companion.efi',1),
                     ('ftp://owner.example/companion.efi',0)]:
    cpu=machine(0x35fb,0x36ce)
    cpu.mem_write(0x250000,uri.encode()+b'\0')
    # An existing TLS child is a fixture to skip creation on the HTTPS branch.
    cpu.mem_write(0x240000+0x370,struct.pack('<Q',1))
    for register,value in [(UC_X86_REG_RDI,0x240000),(UC_X86_REG_RSI,0x250000),
                           (UC_X86_REG_RBP,0x260000),(UC_X86_REG_RSP,0x280008)]:
        cpu.reg_write(register,value)
    cpu.emu_start(base+0x35fb,base+0x36ce,timeout=1000000,count=20000)
    assert cpu.reg_read(UC_X86_REG_RIP)==base+0x36ce
    actual=bytes(cpu.mem_read(0x240000+0x360,1))[0]
    assert actual==expected
    uri_cases.append({'uri':uri,'use_https':actual,'reached_next_stage':True})

token_cases=[]
for mode,expected in [('new_pointer_and_event',0),('same_token',0x800000000000000f),
                      ('different_token_same_event',0x800000000000000f)]:
    cpu=machine(0x2638,0x2656)
    existing,current=0x240000,0x241000
    cpu.mem_write(0x230000+16,struct.pack('<Q',existing))
    cpu.mem_write(existing,struct.pack('<Q',0x5555))
    cpu.mem_write(current,struct.pack('<Q',0x5555 if mode=='different_token_same_event' else 0x6666))
    cpu.mem_write(0x280008,struct.pack('<Q',0x200000))
    cpu.reg_write(UC_X86_REG_RDX,0x230000)
    cpu.reg_write(UC_X86_REG_R8,existing if mode=='same_token' else current)
    cpu.reg_write(UC_X86_REG_RSP,0x280008)
    cpu.emu_start(base+0x2638,0x200000,timeout=1000000,count=1000)
    assert cpu.reg_read(UC_X86_REG_RIP)==0x200000
    status=cpu.reg_read(UC_X86_REG_RAX); assert status==expected
    token_cases.append({'fixture':mode,'status':hex(status)})
result={'scope':__doc__,'module_sha256':records[0]['sha256'],'cases_passed':7,
        'uri_detection_rva':'0x35fb..0x36ce','uri_cases':uri_cases,
        'token_duplicate_rva':'0x2638..0x2656','token_cases':token_cases,
        'limits':'URI classification slice is not full Request or URL validation. TLS-child presence is synthetic. Does not prove HTTP allowed on hardware or the cause of the earlier ACCESS_DENIED; token callback runs only over explicit fixtures.'}
(out/'request-gate-tests.json').write_text(json.dumps(result,indent=2))
print(json.dumps({'cases_passed':7,'uri_cases':uri_cases,'token_cases':token_cases},indent=2))

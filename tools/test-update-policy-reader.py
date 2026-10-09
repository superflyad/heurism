"""Execute the original Dell policy reader offline with physical-probe values.

Only the read routine at RVA ed34 executes; LocateProtocol is a fixture. No
signature verifier or firmware update method runs, on hardware or in emulation.
"""
import hashlib
import json
from pathlib import Path
import struct
import uuid
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_RSP, UC_X86_REG_RIP

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
body = (root / 'update-modules/dell-flash-update-dxe.bin').read_bytes()
digest = '4f40dce428d84aa1b8eb29a84d9f9698c811fd5610f338f0144fe9f0b94d2cc3'
if hashlib.sha256(body).hexdigest() != digest or unicorn.__version__ != '2.1.4':
    raise SystemExit('Pinned module/emulator changed')
live = json.loads((root / 'policy-probe-verification.json').read_text())
if 'signature_policy_byte=0x0000000000000001' not in live['policy_records'][0]:
    raise SystemExit('Live input differs')
pe = struct.unpack_from('<I', body, 60)[0]
count = struct.unpack_from('<H', body, pe+6)[0]
sections = pe+24+struct.unpack_from('<H', body, pe+20)[0]
policy = uuid.UUID('d67be471-df7c-4a3a-af56-ad9ac8fb7ff8')
fallback = uuid.UUID('ef48ffe8-9e24-4eb8-828d-2ec11a9df8dd')
module = 0x1000000
def execute(name, policy_value, version):
    cpu = Uc(UC_ARCH_X86, UC_MODE_64)
    cpu.mem_map(module, 0x50000)
    for i in range(count):
        _, virtual_bytes, rva, raw_bytes, raw_at = struct.unpack_from('<8sIIII', body, sections+40*i)
        if rva+max(virtual_bytes,raw_bytes)>0x50000 or raw_at+raw_bytes>len(body):
            raise RuntimeError('Invalid section bounds')
        if raw_bytes:cpu.mem_write(module+rva,body[raw_at:raw_at+raw_bytes])
    cpu.mem_map(0x200000,0x200000)
    table, locate, interface, output, stack, sentinel = 0x220000,0x230000,0x240000,0x250000,0x3f8008,0x200000
    cpu.mem_write(module+0x3d1a0,struct.pack('<Q',table))
    cpu.mem_write(table+0x140,struct.pack('<Q',locate))
    cpu.mem_write(output,b'\1') # Original caller's default: require signature.
    cpu.mem_write(stack,struct.pack('<Q',sentinel)+bytes(0x60))
    cpu.reg_write(UC_X86_REG_RSP,stack);cpu.reg_write(UC_X86_REG_RCX,output)
    calls=[]
    def trace(cpu,address,size,data):
        if address==locate:
            requested=uuid.UUID(bytes_le=bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_RCX),16)))
            if cpu.reg_read(UC_X86_REG_RDX)!=0:raise RuntimeError('Unexpected registration')
            out=cpu.reg_read(UC_X86_REG_R8);status=0x800000000000000e
            if requested==policy and policy_value is not None:
                cpu.mem_write(interface,bytes([0,policy_value]));status=0
            elif requested==fallback and version is not None:
                cpu.mem_write(interface,struct.pack('<II',0,version));status=0
            elif requested not in (policy,fallback):raise RuntimeError('Unexpected protocol')
            cpu.mem_write(out,struct.pack('<Q',interface if status==0 else 0))
            calls.append({'guid':str(requested),'status':hex(status)})
            sp=cpu.reg_read(UC_X86_REG_RSP);ret=struct.unpack('<Q',cpu.mem_read(sp,8))[0]
            cpu.reg_write(UC_X86_REG_RAX,status);cpu.reg_write(UC_X86_REG_RSP,sp+8);cpu.reg_write(UC_X86_REG_RIP,ret)
        elif not module+0xed34<=address<=module+0xedae:
            raise RuntimeError('Unexpected code or method '+hex(address))
    cpu.hook_add(UC_HOOK_CODE,trace)
    cpu.emu_start(module+0xed34,sentinel,timeout=1000000,count=1000)
    if cpu.reg_read(UC_X86_REG_RIP)!=sentinel:raise RuntimeError('No return')
    return {'name':name,'policy_fixture':policy_value,'fallback_version_fixture':version,
            'efi_status':hex(cpu.reg_read(UC_X86_REG_RAX)),
            'signature_flag':cpu.mem_read(output,1)[0],'locate_calls':calls}
fixtures=[('physical-values',1,5,0,1),('hypothetical-disabled-policy',0,5,0,0),
          ('older-fallback',None,4,0x800000000000000e,0),
          ('current-fallback',None,5,0x800000000000000e,1),
          ('both-absent',None,None,0x800000000000000e,1)]
cases=[]
for name,value,version,status,flag in fixtures:
    result=execute(name,value,version)
    if result['efi_status']!=hex(status) or result['signature_flag']!=flag:
        raise SystemExit('Original reader control failed: '+name)
    cases.append(result)
report={'scope':__doc__,'module_sha256':digest,'physical_report_sha256':live['report_sha256'],
        'cases':cases,'physical_policy_selects_signature_required':cases[0]['signature_flag']==1,
        'update_method_called':False}
(root/'update-policy-reader-controls.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({'controls_passed':len(cases),'physical_policy_selects_signature_required':True,
                  'hypothetical_disabled_value_is_not_live':True,'update_method_called':False},indent=2))

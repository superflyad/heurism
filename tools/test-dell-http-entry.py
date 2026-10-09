"""Replay the saved Dell HTTP driver entry with explicit offline EFI fixtures.

Real Dell entry and policy-lookup instructions execute in Unicorn. Other
firmware services use explicit fixtures, not a hardware safety assumption.
No real firmware or network is invoked.
Unknown services fail closed, and instruction/time budgets bound each replay.
"""
import hashlib
import json
from pathlib import Path
import struct
import uuid

import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8,
    UC_X86_REG_R9, UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RAX)

repo = Path(__file__).resolve().parents[1]
out = repo/'artifacts/research/independent-bootstrap'
body = (out/'http-boot-dxe.bin').read_bytes()
digest = hashlib.sha256(body).hexdigest()
assert digest == 'd992982aaef66ab249a4811d1a619eac3166e0cc66aef6a75e88014869020788'
assert unicorn.__version__ == '2.1.4'
pe = struct.unpack_from('<I', body, 60)[0]
optional = pe+24
section = optional+struct.unpack_from('<H', body, pe+20)[0]
base = 0x1000000
mapped = bytearray(0x20000)
for i in range(struct.unpack_from('<H', body, pe+6)[0]):
    _, virtual, rva, size, start = struct.unpack_from('<8sIIII', body, section+40*i)
    assert rva+max(virtual,size) <= len(mapped) and start+size <= len(body)
    mapped[rva:rva+size] = body[start:start+size]
assert struct.unpack_from('<Q', body, optional+24)[0] == 0
# Apply the same DIR64 relocation delta a loader would apply, rather than
# rewriting internal functions or bypassing any constructor.
reloc, length = struct.unpack_from('<II', body, optional+112+5*8)
at = reloc
while at < reloc+length:
    page, size = struct.unpack_from('<II', mapped, at)
    assert size >= 8 and size%2 == 0 and at+size <= reloc+length
    for n in range(at+8, at+size, 2):
        word = struct.unpack_from('<H', mapped, n)[0]
        kind, offset = word>>12, word&4095
        if kind == 0: continue
        assert kind == 10 and page+offset+8 <= len(mapped)
        value = struct.unpack_from('<Q', mapped, page+offset)[0]
        struct.pack_into('<Q', mapped, page+offset, value+base)
    at += size
assert at == reloc+length

services = {0x40:'AllocatePool', 0x48:'FreePool', 0x98:'HandleProtocol',
            0x140:'LocateProtocol', 0x148:'InstallMultipleProtocolInterfaces',
            0x150:'UninstallMultipleProtocolInterfaces'}
NOT_FOUND = 0x800000000000000e
policy_body=(out/'provider-DellBoardPolicyDxe.bin').read_bytes()
policy_digest=hashlib.sha256(policy_body).hexdigest()
assert policy_digest=='31cf5121722bbff0cc5096b7a87d628669a8700ee149787477c72b2d787226fa'
policy_base=0x1100000
policy_pe=struct.unpack_from('<I',policy_body,60)[0]
policy_optional=policy_pe+24
policy_section=policy_optional+struct.unpack_from('<H',policy_body,policy_pe+20)[0]
policy_mapped=bytearray(0x10000)
for i in range(struct.unpack_from('<H',policy_body,policy_pe+6)[0]):
    _,virtual,rva,size,start=struct.unpack_from('<8sIIII',policy_body,policy_section+40*i)
    assert rva+max(virtual,size)<=len(policy_mapped) and start+size<=len(policy_body)
    policy_mapped[rva:rva+size]=policy_body[start:start+size]
assert struct.unpack_from('<Q',policy_body,policy_optional+24)[0]==0
reloc,length=struct.unpack_from('<II',policy_body,policy_optional+112+5*8)
at=reloc
while at<reloc+length:
    page,size=struct.unpack_from('<II',policy_mapped,at)
    assert size>=8 and size%2==0 and at+size<=reloc+length
    for n in range(at+8,at+size,2):
        word=struct.unpack_from('<H',policy_mapped,n)[0]
        if not word>>12: continue
        assert word>>12==10 and page+(word&4095)+8<=len(policy_mapped)
        value=struct.unpack_from('<Q',policy_mapped,page+(word&4095))[0]
        struct.pack_into('<Q',policy_mapped,page+(word&4095),value+policy_base)
    at+=size
assert at==reloc+length
def execute(fail_install=None, vendor_absent=False):
    cpu = Uc(UC_ARCH_X86, UC_MODE_64)
    cpu.mem_map(base, len(mapped)); cpu.mem_write(base, bytes(mapped))
    cpu.mem_map(policy_base,len(policy_mapped)); cpu.mem_write(policy_base,bytes(policy_mapped))
    cpu.mem_map(0x200000, 0x200000)
    table, st, runtime, loaded = 0x202000, 0x203000, 0x204000, 0x205000
    sentinel, stack, heap = 0x200000, 0x3f8008, 0x300000
    def write64(address, value): cpu.mem_write(address, struct.pack('<Q', value))
    def read64(address): return struct.unpack('<Q', bytes(cpu.mem_read(address,8)))[0]
    def guid(address): return str(uuid.UUID(bytes_le=bytes(cpu.mem_read(address,16))))
    def return_value(value):
        rsp = cpu.reg_read(UC_X86_REG_RSP)
        destination = read64(rsp)
        cpu.reg_write(UC_X86_REG_RAX,value)
        cpu.reg_write(UC_X86_REG_RSP,rsp+8)
        cpu.reg_write(UC_X86_REG_RIP,destination)
    write64(st+96, table); write64(st+88, runtime)
    # Reproduce only the three linked callback records created by the provider's
    # constructor at RVA 0x7dc..0x877. No live registrations are assumed. Unknown
    # HTTP policy lookup executes the real provider function; callbacks may not
    # execute in this fixture and are caught by the instruction-range guard.
    head=policy_base+0xc50
    for i in range(3):
        node=0x260000+i*0x100
        source,callback=struct.unpack_from('<QQ',policy_mapped,0xc60+i*16)
        cpu.mem_write(node,struct.pack('<I',0x48504244))
        write64(node+8,head if i==2 else node+0x108)
        write64(node+16,head if i==0 else node-0xf8)
        cpu.mem_write(node+24,bytes(cpu.mem_read(source,16)))
        write64(node+40,callback)
    write64(head,0x260008); write64(head+8,0x260208)
    # Populate every EFI service slot with a trap; unreviewed calls never gain
    # an implicit successful stub. Runtime writes would likewise be rejected.
    for offset in range(24, 384, 8): write64(table+offset, 0x210000+offset)
    for offset in range(24, 136, 8): write64(runtime+offset, 0x220000+offset)
    write64(stack, sentinel)
    cpu.reg_write(UC_X86_REG_RCX,0x1234)
    cpu.reg_write(UC_X86_REG_RDX,st)
    cpu.reg_write(UC_X86_REG_RSP,stack)
    trace = []; interfaces = {}; install_count = 0
    def hook(uc,address,size,data):
        nonlocal heap, install_count
        if address==base+0xbb53 and trace and trace[-1]['service']=='InterfaceMethod':
            value=uc.reg_read(UC_X86_REG_RAX)
            assert value==NOT_FOUND
            trace[-1]['actual_provider_result']=hex(value)
        if base <= address < base+len(mapped): return
        if policy_base+0x894 <= address <= policy_base+0x9c7: return
        args = [uc.reg_read(r) for r in (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9)]
        rsp = uc.reg_read(UC_X86_REG_RSP)
        if 0x210000 <= address < 0x210180:
            offset = address-0x210000
            if offset not in services:
                raise RuntimeError('Unreviewed boot service '+hex(offset))
            name = services[offset]; record = {'service':name, 'caller_rva':hex(read64(rsp)-base)}
            trace.append(record)
            if name == 'AllocatePool':
                assert args[1] <= 0x10000 and heap+args[1]+16 < 0x3e0000
                write64(args[2], heap); heap += (args[1]+15)&~15
                return_value(0)
            elif name == 'FreePool': return_value(0)
            elif name == 'HandleProtocol':
                record['guid'] = guid(args[1])
                assert args[0] == 0x1234 and record['guid'] == '5b1b31a1-9562-11d2-8e3f-00a0c969723b'
                write64(args[2], loaded); return_value(0)
            elif name == 'LocateProtocol':
                target = guid(args[0]); record['guid'] = target
                if vendor_absent and target == '8f63ff6d-b7d4-474d-8c53-68258a224d58':
                    record['fixture_status']=hex(NOT_FOUND)
                    return_value(NOT_FOUND); return
                # Known read-only/passive constructor dependencies get opaque
                # interfaces; any actual interface method needs its own fixture.
                if target not in interfaces:
                    obj = 0x240000+len(interfaces)*0x1000
                    interfaces[target] = obj
                    for n in range(40): write64(obj+n*8, 0x280000+len(interfaces)*0x1000+n*16)
                    if target == '11d1ec21-e568-4eb0-8e1d-a0809772b606':
                        # This vendor interface uses a packed, unaligned method
                        # pointer. Its semantics remain explicitly unspecified.
                        write64(obj+9,0x2bf000)
                write64(args[2], interfaces[target]); return_value(0)
            elif name in ('InstallMultipleProtocolInterfaces','UninstallMultipleProtocolInterfaces'):
                pairs = args[1:]+[read64(rsp+40+i*8) for i in range(8)]
                listed = []
                for i in range(0,len(pairs)-1,2):
                    if pairs[i] == 0: break
                    listed.append({'guid': guid(pairs[i]), 'interface_rva':hex(pairs[i+1]-base)})
                else: raise RuntimeError('Unterminated protocol list')
                record['protocols'] = listed
                if name == 'InstallMultipleProtocolInterfaces':
                    install_count += 1
                    record['install_number'] = install_count
                    status = NOT_FOUND if install_count == fail_install else 0
                    if not status and not read64(args[0]): write64(args[0],0x4000+install_count)
                    return_value(status)
                else: return_value(0)
        elif 0x280000 <= address < 0x2c0000:
            match = None
            if address == 0x2bf000:
                trace.append({'service':'VendorCleanupFixture',
                    'guid':'11d1ec21-e568-4eb0-8e1d-a0809772b606','method_offset':9,
                    'caller_rva':hex(read64(rsp)-base), 'fixture_status':'0x0'})
                return_value(0); return
            for g, obj in interfaces.items():
                for n in range(40):
                    if read64(obj+n*8) == address: match = (g,n)
            record = {'service':'InterfaceMethod', 'guid': match[0] if match else None,
                      'slot': match[1] if match else None, 'args':[hex(v) for v in args],
                      'caller_rva':hex(read64(rsp)-base)}
            trace.append(record)
            if match == ('8f63ff6d-b7d4-474d-8c53-68258a224d58',0):
                descriptor = bytes(uc.mem_read(args[3],24))
                index, buffer, length = struct.unpack('<I4xQQ',descriptor)
                assert args[2] == 1 and index in (0,1) and length in (2,4)
                record['descriptor'] = {'index':index,'buffer_rva':hex(buffer-base),'bytes':length}
                record['provider']='actual saved DellBoardPolicyDxe lookup, default-table fixture'
                uc.reg_write(UC_X86_REG_RIP,policy_base+0x894)
            else: raise RuntimeError('Unreviewed interface '+str(match))
        else: raise RuntimeError('Unexpected instruction '+hex(address))
    cpu.hook_add(UC_HOOK_CODE,hook)
    error = None
    try: cpu.emu_start(base+0x9a8,sentinel,timeout=2000000,count=200000)
    except (unicorn.UcError,RuntimeError,AssertionError) as e: error = str(e)
    complete = cpu.reg_read(UC_X86_REG_RIP) == sentinel
    if complete:
        assert bytes(cpu.mem_read(base+0xc368,4))==bytes(mapped[0xc368:0xc36c])
        assert bytes(cpu.mem_read(base+0xd6f0,2))==bytes(mapped[0xd6f0:0xd6f2])
        assert read64(loaded+88)==base+0x808
    return {'fail_install':fail_install, 'vendor_absent':vendor_absent,
            'returned':complete, 'status':hex(cpu.reg_read(UC_X86_REG_RAX)),
            'stopped_rva':hex(cpu.reg_read(UC_X86_REG_RIP)-base), 'error':error, 'trace':trace}

results = [execute(),execute(1),execute(2),execute(vendor_absent=True)]
for result in results:
    assert result['returned'] and result['error'] is None, result
    assert result['status'] == hex(NOT_FOUND if result['fail_install'] else 0)
assert not any(t['service']=='InterfaceMethod' for t in results[3]['trace'])
assert len([t for t in results[2]['trace'] if t['service']=='UninstallMultipleProtocolInterfaces']) == 1
report = {'scope':__doc__, 'module_sha256':digest, 'policy_provider_sha256':policy_digest,
          'cases_passed':4,'cases':results,
          'limits':'Other firmware interfaces are synthetic; policy lookup runs real code over a reconstructed default table. Does not measure the live provider or additional registrations. No hardware execution, controller Start, scheduling, DHCP, transfer, or autonomous startup proof.'}
(out/'entry-tests.json').write_text(json.dumps(report,indent=2))
print(json.dumps([{'fail_install':r['fail_install'],'returned':r['returned'],
                  'status':r['status'],'error':r['error'],'calls':len(r['trace'])}
                 for r in results],indent=2))

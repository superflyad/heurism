"""Execute the copied capsule capability query offline, never an update entry.

Capability success is transport metadata, not signature acceptance or permission
to flash. PE sections are loaded at their virtual addresses, not file offsets.
"""
import hashlib
import json
from pathlib import Path
import struct
import uuid
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSP, UC_X86_REG_RIP

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
body = (root / 'update-modules/capsule-runtime-dxe.bin').read_bytes()
digest = '4db34759e75c0502ecb4057d4988d02ba5e5d713001ed838c86f41786f11e3aa'
if hashlib.sha256(body).hexdigest() != digest or unicorn.__version__ != '2.1.4':
    raise SystemExit('Pinned module/emulator changed')
pe = struct.unpack_from('<I', body, 60)[0]
count = struct.unpack_from('<H', body, pe+6)[0]
optional = pe+24
section_table = optional + struct.unpack_from('<H', body, pe+20)[0]
module = 0x1000000
fmp = uuid.UUID('6dcbd5ed-e82d-4c44-bda1-7194199ad92a')
system = uuid.UUID('417e8ca2-c8c6-4a7c-be26-e3bc703c2cbb')

def execute(name, capsule_guid, flags):
    cpu = Uc(UC_ARCH_X86, UC_MODE_64)
    cpu.mem_map(module, 0x10000)
    for index in range(count):
        _, virtual_bytes, rva, raw_bytes, raw_at = struct.unpack_from('<8sIIII', body, section_table+40*index)
        if rva + max(virtual_bytes, raw_bytes) > 0x10000 or raw_at+raw_bytes > len(body):
            raise RuntimeError('Invalid PE section')
        if raw_bytes:
            cpu.mem_write(module+rva, body[raw_at:raw_at+raw_bytes])
    cpu.mem_map(0x200000, 0x200000)
    array, capsule, maximum, reset, stack, sentinel = 0x240000, 0x241000, 0x242000, 0x242010, 0x3f8008, 0x200000
    cpu.mem_write(array, struct.pack('<Q', capsule))
    # Minimal header fixture has no update image or signature payload.
    cpu.mem_write(capsule, capsule_guid.bytes_le + struct.pack('<III', 28, flags, 28))
    cpu.mem_write(maximum, bytes([0xa5])*8)
    cpu.mem_write(reset, bytes([0xa5])*4)
    # Constructor constants and an initialized empty ESRT cache are fixtures.
    cpu.mem_write(module+0x41e0, struct.pack('<II', 0xa00000, 0x6400000))
    cpu.mem_write(module+0x4238, b'\1')
    cpu.mem_write(module+0x4240, struct.pack('<Q', 0x250000))
    cpu.mem_write(0x250000, bytes(16))
    visited = set()
    def trace(cpu, address, size, data):
        if not module <= address < module+0x10000:
            raise RuntimeError('Unexpected external service at '+hex(address))
        if address == module+0x1388:
            raise RuntimeError('UpdateCapsule must never execute')
        visited.add(address-module)
    cpu.hook_add(UC_HOOK_CODE, trace)
    cpu.mem_write(stack, struct.pack('<Q', sentinel)+bytes(0x50))
    for register, value in [(UC_X86_REG_RSP,stack),(UC_X86_REG_RCX,array),(UC_X86_REG_RDX,1),
                            (UC_X86_REG_R8,maximum),(UC_X86_REG_R9,reset)]:
        cpu.reg_write(register,value)
    cpu.emu_start(module+0x1628,sentinel,timeout=1000000,count=100000)
    if cpu.reg_read(UC_X86_REG_RIP) != sentinel:
        raise RuntimeError('Query did not return')
    status = cpu.reg_read(UC_X86_REG_RAX)
    return {'name':name,'capsule_guid':str(capsule_guid),'flags':hex(flags),'efi_status':hex(status),
            'maximum_capsule_bytes':struct.unpack('<Q',cpu.mem_read(maximum,8))[0] if status==0 else None,
            'reset_type':struct.unpack('<I',cpu.mem_read(reset,4))[0] if status==0 else None,
            'called_support_helper':0x2a00 in visited,'signature_payload_present':False}

fixtures = [('unknown-no-persist',uuid.UUID(int=0),0),
            ('unknown-persist',uuid.UUID(int=0),0x10000),
            ('unknown-persist-populate',uuid.UUID(int=0),0x30000),
            ('system-persist-populate',system,0x30000),
            ('populate-without-persist',system,0x20000),
            ('empty-fmp-no-persist',fmp,0),
            ('fmp-persist',fmp,0x10000),
            ('fmp-persist-populate',fmp,0x30000)]
cases = [execute(*fixture) for fixture in fixtures]
expected = ['0x8000000000000003','0x8000000000000003','0x0','0x0','0x8000000000000002','0x0','0x0','0x8000000000000002']
if [case['efi_status'] for case in cases] != expected:
    raise SystemExit('Unexpected capability results: '+json.dumps(cases))
report = {'scope':__doc__,'module_sha256':digest,'query_entry_rva':'0x1628',
          'forbidden_update_entry_rva':'0x1388','constructor_and_empty_esrt_cache_are_fixtures':True,
          'physical_update_or_signature_acceptance_tested':False,'cases':cases}
(root/'capsule-query-emulation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))

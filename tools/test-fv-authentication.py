"""Offline Dell FV notification and Security2 tests; never writes hardware.

PEI services and boot mode are fixtures. The original notification, BIOS-info
lookup, digest comparison, registration, and Security2 code run unmodified.
This does not test DXE dispatch or a complete physical boot.
"""
import hashlib
import json
from pathlib import Path
import struct
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
rom = (root / 'bios16-a.bin').read_bytes()
expected = '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
if unicorn.__version__ != '2.1.4' or hashlib.sha256(rom).hexdigest() != expected:
    raise SystemExit('Pinned emulator or ROM changed')
if rom[0xfb9460:0xfb9468] != b'$BIOSIF$' or rom[0xfb19f0:0xfb19f8] != b'__MHTS__':
    raise SystemExit('Original policy table changed')

def execute(offset, length, changed=False):
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0xff000000, 0x1000000)
    cpu.mem_write(0xff000000, rom)
    cpu.mem_map(0x200000, 0x200000)
    base = 0xff000000 + offset
    if changed:
        cpu.mem_write(base + 0x80, bytes([rom[offset + 0x80] ^ 1]))
    # Synthetic empty FV-auth cache; original routine populates it when eligible.
    cpu.mem_write(0xfffb183c, struct.pack('<I', 0x250000))
    cpu.mem_write(0xfffb16dc, struct.pack('<I', 0))
    cpu.mem_write(0xfffb16d8, struct.pack('<I', 8))
    cpu.mem_write(0xfffb1850, b'\x01')
    cpu.mem_write(0xfffb1858, struct.pack('<I', 0))
    # FV-info2 PPI and the core FV PPI returned by LocatePpi.
    info, ppi = 0x240000, 0x260000
    cpu.mem_write(info, rom[offset + 16:offset + 32] + struct.pack('<5I', base, length, 0, 0, 0))
    cpu.mem_write(ppi, struct.pack('<I', 0x210100))
    cpu.mem_write(ppi + 0x20, struct.pack('<II', 0x50564650, 0x10030))
    trace = []

    def mock(cpu, address, size, data):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', cpu.mem_read(sp, 4))[0]
        value = 0
        trace.append(hex(address))
        if address == 0xfffb0dc0:
            value = 0
        elif address == 0xfffb0f5d:
            out = struct.unpack('<I', cpu.mem_read(sp + 8, 4))[0]
            cpu.mem_write(out, struct.pack('<I', ppi))
        elif address == 0x210100:
            this, address_arg, length_arg, out = struct.unpack('<4I', cpu.mem_read(sp + 4, 16))
            if (this, address_arg, length_arg) != (ppi, base, length):
                raise RuntimeError('Unexpected FV handle query')
            cpu.mem_write(out, struct.pack('<I', base))
        elif address == 0xfffb0023:
            value = 0xfffb19f0
        elif address == 0xfffb0156:
            value = 0xfffb9460
        elif address == 0xfffb0efd:
            if cpu.reg_read(UC_X86_REG_ECX) != 0x70:
                raise RuntimeError('Unexpected hash allocation')
            value = 0x280000
        else:
            raise RuntimeError('Unexpected service')
        cpu.reg_write(UC_X86_REG_EAX, value)
        cpu.reg_write(UC_X86_REG_ESP, sp + 4)
        cpu.reg_write(UC_X86_REG_EIP, ret)

    for address in (0xfffb0dc0, 0xfffb0f5d, 0x210100, 0xfffb0023, 0xfffb0156, 0xfffb0efd):
        cpu.hook_add(UC_HOOK_CODE, mock, begin=address, end=address)
    stack, sentinel = 0x3f8000, 0x200000
    def run(entry, args):
        cpu.mem_write(stack, struct.pack('<' + 'I' * (len(args) + 1), sentinel, *args))
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.emu_start(entry, sentinel, timeout=30000000, count=500000000)
        if cpu.reg_read(UC_X86_REG_EIP) != sentinel:
            raise RuntimeError('Original callback did not return')
        return cpu.reg_read(UC_X86_REG_EAX)

    notification_status = run(0xfffb061f, [0, 0xfffb16b4, info])
    authentication = struct.unpack('<I', cpu.mem_read(info + 32, 4))[0]
    count = struct.unpack('<I', cpu.mem_read(0xfffb16dc, 4))[0]
    defer = 0x270000
    cpu.mem_write(defer, b'\xff')
    security_status = run(0xfffb095c, [0, 0, authentication, base, 0, defer])
    return {'bios_offset': hex(offset), 'bytes': length, 'changed_byte_fixture': changed,
            'notification_status': hex(notification_status), 'authentication_status': hex(authentication),
            'cache_records': count, 'security2_status': hex(security_status),
            'defer_execution': bool(cpu.mem_read(defer, 1)[0]), 'mocked_calls': trace}

cases = [execute(0x110000, 0x20000), execute(0x680000, 0x100000), execute(0x680000, 0x100000, True)]
outcomes = [(c['authentication_status'], c['cache_records'], c['security2_status'], c['defer_execution']) for c in cases]
if outcomes != [('0x0', 0, '0x8000001a', True), ('0x2', 1, '0x0', False), ('0xa', 1, '0x8000001a', True)]:
    raise RuntimeError('Unexpected authentication results: ' + json.dumps(cases))
report = {'scope': __doc__, 'bios_sha256': expected, 'bios_info_table': '0xfffb9460',
          'digest_table': '0xfffb19f0', 'notification_callback': '0xfffb061f',
          'security2_callback': '0xfffb095c', 'emulator': 'unicorn==2.1.4', 'cases': cases}
(root / 'fv-authentication-emulation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))

"""Reconstruct BGPDT with original ROM code and explicitly mocked PEI services.

No hardware MSR or flash access. The constructed policy must be compared with
hardware separately; emulation alone does not prove live write permissions.
"""
import hashlib
import json
from pathlib import Path
import struct
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                              UC_X86_REG_ESP, UC_X86_REG_EIP)

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
rom = (root / 'bios16-a.bin').read_bytes()
expected = '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
if unicorn.__version__ != '2.1.4' or hashlib.sha256(rom).hexdigest() != expected:
    raise SystemExit('Pinned emulator/ROM changed')
spi_reports = sorted(root.glob('spi-inspection-*.json'))
if not spi_reports:
    raise SystemExit('Missing physical SPI evidence')
spi_path = spi_reports[-1]
spi = json.loads(spi_path.read_text())
region_register = int(spi['registers']['FREG1'], 16)
bios_base = (region_register & 0x7fff) << 12
bios_end = (((region_register >> 16) & 0x7fff) << 12) | 0xfff
bios_size = bios_end - bios_base + 1
if (bios_base, bios_size) != (0x800000, len(rom)):
    raise SystemExit('Physical BIOS region differs from pinned ROM layout')
cases = []
for ec_present in (False, True):
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0xff000000, 0x1000000)
    cpu.mem_write(0xff000000, rom)
    cpu.mem_map(0x200000, 0x200000)
    stack, sentinel = 0x3f8000, 0x200000
    hob, pool = 0x300000, 0x310000
    flash_interface, flash_method = 0x220000, 0x230000
    ec_interface, ec_method = 0x240000, 0x250000
    cpu.mem_write(flash_interface + 0x20, struct.pack('<I', flash_method))
    cpu.mem_write(ec_interface + 0x18, struct.pack('<I', ec_method))
    cpu.mem_write(stack, struct.pack('<I', sentinel))
    cpu.reg_write(UC_X86_REG_ESP, stack)
    calls = []

    def mock(cpu, address, size, data):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', cpu.mem_read(sp, 4))[0]
        cx, dx = cpu.reg_read(UC_X86_REG_ECX), cpu.reg_read(UC_X86_REG_EDX)
        value = 0
        if address == 0xfffcbf97:
            out = struct.unpack('<I', cpu.mem_read(sp + 8, 4))[0]
            if cx == 0xfffd7220:
                interface = flash_interface
            elif cx == 0xfffd7a20:
                interface = 0xfff94f50  # Actual static ROM registry.
            else:
                raise RuntimeError('Unexpected LocatePpi GUID ' + hex(cx))
            cpu.mem_write(out, struct.pack('<I', interface))
        elif address == flash_method:
            interface, region, base_out, size_out = struct.unpack('<IIII', cpu.mem_read(sp + 4, 16))
            if interface != flash_interface or region != 1:
                raise RuntimeError('Unexpected BIOS region query')
            cpu.mem_write(base_out, struct.pack('<I', bios_base))
            cpu.mem_write(size_out, struct.pack('<I', bios_size))
        elif address == 0xfffcc00f:
            if dx != 0x328:
                raise RuntimeError('Unexpected HOB allocation')
            out = struct.unpack('<I', cpu.mem_read(sp + 4, 4))[0]
            cpu.mem_write(out, struct.pack('<I', hob))
        elif address == 0xfffcc065:
            allocations = sum(c['address'] == hex(address) for c in calls)
            target = pool + 0x1000 * allocations
            if cx > 0x1000 or not 0x200000 <= dx < 0x400000:
                raise RuntimeError('Unexpected pool allocation')
            cpu.mem_write(dx, struct.pack('<I', target))
        elif address == 0xfffcda72:
            value = ec_interface
        elif address == ec_method:
            argument = struct.unpack('<I', cpu.mem_read(sp + 4, 4))[0]
            if argument != 0x38:
                raise RuntimeError('Unexpected EC feature query')
            value = int(ec_present)
        elif address == 0xfffd2bb5:
            cpu.mem_write(cx, struct.pack('<I', 0xffc80090))  # Actual RAW BIOS Guard ACM.
            cpu.mem_write(dx, struct.pack('<I', 0x8600))
        else:
            raise RuntimeError('Unrecognized mocked helper')
        calls.append({'address': hex(address), 'ecx': hex(cx), 'edx': hex(dx)})
        cpu.reg_write(UC_X86_REG_EAX, value)
        cpu.reg_write(UC_X86_REG_ESP, sp + 4)
        cpu.reg_write(UC_X86_REG_EIP, ret)

    for address in (0xfffcbf97, flash_method, 0xfffcc00f, 0xfffcc065, 0xfffcda72, ec_method, 0xfffd2bb5):
        cpu.hook_add(UC_HOOK_CODE, mock, begin=address, end=address)

    def reject_privileged(cpu, address, size, data):
        instruction = bytes(cpu.mem_read(address, size))
        if instruction[:2] in (b'\x0f\x30', b'\x0f\x32') or instruction[0] in range(0xe4, 0xe8) or instruction[0] in range(0xec, 0xf0):
            raise RuntimeError('Unexpected privileged/I/O instruction in offline constructor')

    cpu.hook_add(UC_HOOK_CODE, reject_privileged)
    cpu.emu_start(0xfffd2c4b, sentinel, timeout=10000000, count=10000000)
    if cpu.reg_read(UC_X86_REG_EIP) != sentinel or cpu.reg_read(UC_X86_REG_EAX) != 0:
        raise RuntimeError('Original constructor failed/timeout')
    table_size = struct.unpack('<I', cpu.mem_read(hob + 0x18, 4))[0]
    if table_size != 176:
        raise RuntimeError('Unexpected BGPDT size')
    table = bytes(cpu.mem_read(hob + 0x18, table_size))
    if table[8:24] != b'DellAgsX11\0\0\0\0\0\0':
        raise RuntimeError('Unexpected platform identifier')
    low, high = struct.unpack_from('<II', table, 0xa8)
    if (low, high) != (0x940000, 0x17fffff):
        raise RuntimeError('Unexpected signed flash range')
    key = rom[0xf94950:0xf94b50]
    key_hash = hashlib.sha256(key[:256][::-1] + int.from_bytes(key[256:], 'big').to_bytes(4, 'little')).digest()
    if table[24:56] != key_hash:
        raise RuntimeError('Original key encoding/hash differs')
    path = root / ('bgpdt-ec-present.bin' if ec_present else 'bgpdt-ec-absent.bin')
    path.write_bytes(table)
    cases.append({'ec_present_fixture': ec_present, 'table_bytes': table_size,
                  'sha256': hashlib.sha256(table).hexdigest(),
                  'platform': table[8:24].rstrip(b'\0').decode(),
                  'attributes': hex(struct.unpack_from('<I', table, 0x84)[0]),
                  'bios_guard_svn': hex(struct.unpack_from('<I', table, 0x78)[0]),
                  'signed_flash_range': [hex(low), hex(high)],
                  'unsigned_bios_region': ['0x800000', '0x93ffff'],
                  'key_hashes': [table[24 + i * 32:56 + i * 32].hex() for i in range(3)],
                  'mocked_calls': calls})
report = {'scope': 'Offline original BGPDT constructor and original key SHA-256; '
                   'mocked PEI allocations/region query/EC presence/ACM lookup. '
                   'Not live policy validation or proof of executable unsigned writes.',
          'rom_sha256': expected, 'spi_evidence': spi_path.name,
          'constructor': '0xfffd2c4b', 'cases': cases}
(root / 'bios-guard-policy-emulation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({**report, 'cases': [{k: v for k, v in c.items() if k != 'mocked_calls'} for c in cases]}, indent=2))

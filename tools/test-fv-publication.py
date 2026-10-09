"""Run original FV publisher with mocked PEI services; no firmware modification."""
import hashlib
import json
from pathlib import Path
import struct
import uuid
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_IDTR

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
rom = (root / 'bios16-a.bin').read_bytes()
expected = '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
body = (root / 'update-modules/report-fv-pei.bin').read_bytes()
if unicorn.__version__ != '2.1.4' or hashlib.sha256(rom).hexdigest() != expected or hashlib.sha256(body).hexdigest() != '09e24f7309c0b0919b8ff68cde182420eba77a0f40092d7a0f5a14b900fdd9d0':
    raise SystemExit('Pinned emulator/ROM/module changed')
fv_guid = uuid.UUID(bytes_le=rom[0xfc0630:0xfc0640])
cases = []
for mode, extra in [(0, False), (0, True), (1, False), (0x11, False), (0x20, False)]:
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0xff000000, 0x1000000)
    cpu.mem_write(0xff000000, rom)
    cpu.mem_map(0x200000, 0x200000)
    services, table, pool, hob_end = 0x270000, 0x280000, 0x300000, 0x290000
    cpu.mem_write(0x260000, struct.pack('<I', services))
    cpu.mem_write(services, struct.pack('<I', table))
    cpu.mem_write(hob_end, struct.pack('<HHI', 0xffff, 8, 0))
    cpu.reg_write(UC_X86_REG_IDTR, (0, 0x260004, 0x100, 0))
    records, allocations, resource_hobs, notifications = [], [], [], []
    for offset in (0x18, 0x24, 0x28, 0x30, 0x34, 0x4c):
        cpu.mem_write(table + offset, struct.pack('<I', 0x210000 + offset))

    def mock(cpu, address, size, data):
        sp = cpu.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack('<I', cpu.mem_read(sp, 4))[0]
        value = 0
        if address == 0xfffc0236:
            cpu.mem_write(cpu.reg_read(UC_X86_REG_ECX), bytes([int(extra)]))
        elif address == 0x210018:
            _, descriptor = struct.unpack('<II', cpu.mem_read(sp + 4, 8))
            flags, guid_at, interface = struct.unpack('<III', cpu.mem_read(descriptor, 12))
            guid = uuid.UUID(bytes_le=bytes(cpu.mem_read(guid_at, 16)))
            if guid == fv_guid:
                raw = bytes(cpu.mem_read(interface, 36))
                base, length = struct.unpack_from('<II', raw, 16)
                records.append({'base': hex(base), 'bios_offset': hex(base - 0xff000000),
                                'bytes': length, 'format_guid': str(uuid.UUID(bytes_le=raw[:16]))})
        elif address == 0x210024:
            _, descriptor = struct.unpack('<II', cpu.mem_read(sp + 4, 8))
            notifications.append(hex(descriptor))
        elif address == 0x210028:
            _, out = struct.unpack('<II', cpu.mem_read(sp + 4, 8))
            cpu.mem_write(out, struct.pack('<I', mode))
        elif address == 0x210030:
            _, out = struct.unpack('<II', cpu.mem_read(sp + 4, 8))
            cpu.mem_write(out, struct.pack('<I', hob_end))
        elif address == 0x210034:
            _, kind, length, out = struct.unpack('<IIII', cpu.mem_read(sp + 4, 16))
            target = pool + len(allocations) * 0x1000
            allocations.append(target)
            cpu.mem_write(target, struct.pack('<HHI', kind, length, 0))
            cpu.mem_write(out, struct.pack('<I', target))
            resource_hobs.append({'type': kind, 'bytes': length, 'address': target})
        elif address == 0x21004c:
            _, length, out = struct.unpack('<III', cpu.mem_read(sp + 4, 12))
            if length > 0x1000:
                raise RuntimeError('Unexpected allocation')
            target = pool + len(allocations) * 0x1000
            allocations.append(target)
            cpu.mem_write(out, struct.pack('<I', target))
        else:
            raise RuntimeError('Unexpected synthetic service')
        cpu.reg_write(UC_X86_REG_EAX, value)
        cpu.reg_write(UC_X86_REG_ESP, sp + 4)
        cpu.reg_write(UC_X86_REG_EIP, ret)

    for address in [0xfffc0236] + [0x210000 + offset for offset in (0x18, 0x24, 0x28, 0x30, 0x34, 0x4c)]:
        cpu.hook_add(UC_HOOK_CODE, mock, begin=address, end=address)
    for entry in (0xfffc009d, 0xfffc010c):
        stack, sentinel = 0x3f8000, 0x200000
        cpu.mem_write(stack, struct.pack('<III', sentinel, 0, services))
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.emu_start(entry, sentinel, timeout=1000000, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != sentinel or cpu.reg_read(UC_X86_REG_EAX) != 0:
            raise RuntimeError('Original FV publisher failed')
    unsigned = [r for r in records if int(r['bios_offset'], 16) < 0x140000]
    if bool(unsigned) != (mode != 0x20) or (unsigned and unsigned != [
            {'base': '0xff110000', 'bios_offset': '0x110000', 'bytes': 0x20000,
             'format_guid': '8c8ce578-8a3d-4f1c-9935-896185c32dd3'}]):
            raise RuntimeError('Unexpected unsigned-volume publication: ' + json.dumps(unsigned))
    cases.append({'boot_mode_fixture': hex(mode), 'extra_volume_fixture': extra,
                  'published_fv_ppis': records, 'unsigned_volume_ppis': unsigned,
                  'notification_descriptors': notifications,
                  'resource_hobs': [{k: v for k, v in h.items() if k != 'address'} for h in resource_hobs]})
report = {'scope': 'Offline original entry and notification callback with synthetic PEI services/HOBs/boot mode. '
                   'Actual FV-info PPI construction executes, but downstream dispatcher/authentication and live flash writes do not.',
          'rom_sha256': expected, 'fv_info_ppi_guid': str(fv_guid), 'cases': cases}
(root / 'fv-publication-emulation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'scope': report['scope'], 'cases': [{'boot_mode': c['boot_mode_fixture'],
                  'fv_count': len(c['published_fv_ppis']), 'unsigned_fvs': c['unsigned_volume_ppis']} for c in cases]}, indent=2))

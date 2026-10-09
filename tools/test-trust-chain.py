"""Emulate one Dell SHA-256 comparison path on copied bytes, never on hardware.

Requires unicorn==2.1.4. This is a function test with synthetic firmware state,
not a full boot simulation or proof of which path the physical boot selects.
"""
import hashlib
import json
from pathlib import Path
import struct
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP

if unicorn.__version__ != '2.1.4':
    raise SystemExit('Requires reviewed emulator version unicorn==2.1.4')
root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
image = (root / 'bios16-a.bin').read_bytes()
analysis = json.loads((root / 'uefi-volume-analysis.json').read_text())
if hashlib.sha256(image).hexdigest() != analysis['bios_sha256']:
    raise SystemExit('Firmware bytes differ from analysis')
reference = next(entry for entry in analysis['digest_references']
                 if entry['volume_offset'] == '0x680000' and entry['algorithm'] == 'sha256'
                 and entry['table_inside_ibb'] and entry['header_bytes_skipped'] == 0)
expected_record = 0xff000000 + int(reference['digest_offset'], 16) - 20
data = image[0x680000:0x780000]

def execute(payload):
    emulator = Uc(UC_ARCH_X86, UC_MODE_32)
    emulator.mem_map(0xff000000, 0x1000000)
    emulator.mem_write(0xff000000, image)
    emulator.mem_map(0x200000, 0x400000)
    emulator.mem_map(0x700000, 0x10000)
    emulator.mem_write(0x300000, payload)
    # The selected fallback uses [descriptor+4]=length, [descriptor+8]=address.
    emulator.mem_write(0x210000, struct.pack('<III', 0, len(payload), 0x300000))
    emulator.mem_write(0xfffb1850, b'\x01')
    emulator.mem_write(0xfffb1858, struct.pack('<I', 0))
    stack, sentinel = 0x708000, 0x220000
    emulator.mem_write(stack, struct.pack('<I', sentinel))
    emulator.reg_write(UC_X86_REG_ESP, stack)
    emulator.reg_write(UC_X86_REG_ECX, 0x210000)
    emulator.reg_write(UC_X86_REG_EDX, expected_record)
    calls = []

    def hook(cpu, address, size, user_data):
        if address == 0xfffb0efd:
            # Supply a private heap buffer instead of calling EFI allocation.
            requested = cpu.reg_read(UC_X86_REG_ECX)
            if requested != 0x70:
                raise RuntimeError('Unexpected allocation size')
            calls.append({'function': hex(address), 'bytes': requested})
            sp = cpu.reg_read(UC_X86_REG_ESP)
            return_address = struct.unpack('<I', cpu.mem_read(sp, 4))[0]
            cpu.reg_write(UC_X86_REG_EAX, 0x200000)
            cpu.reg_write(UC_X86_REG_ESP, sp + 4)
            cpu.reg_write(UC_X86_REG_EIP, return_address)

    emulator.hook_add(UC_HOOK_CODE, hook, begin=0xfffb0efd, end=0xfffb0efd)
    emulator.emu_start(0xfffb0b73, sentinel, timeout=20000000, count=500000000)
    if emulator.reg_read(UC_X86_REG_EIP) != sentinel or len(calls) != 1:
        raise RuntimeError('Comparison did not return through the expected path: '
                           + hex(emulator.reg_read(UC_X86_REG_EIP)))
    return {'comparison_matched': bool(emulator.reg_read(UC_X86_REG_EAX) & 0xff),
            'mocked_calls': calls}

original = execute(data)
changed = bytearray(data)
changed[0x80] ^= 1
modified = execute(bytes(changed))
if not original['comparison_matched'] or modified['comparison_matched']:
    raise SystemExit('Dell comparison negative control failed')
result = {'scope': 'Offline execution of original Dell SHA-256 fallback comparison; '
                   'synthetic globals and EFI allocation; not a physical boot acceptance test',
          'emulator': 'unicorn==2.1.4', 'bios_sha256': analysis['bios_sha256'],
          'function': '0xfffb0b73', 'volume': '0x680000..0x77ffff',
          'expected_digest_record': hex(expected_record),
          'original_volume': original, 'one_byte_changed_volume': modified,
          'test_volume_change_inside_ibb': False,
          'firmware_backup_unchanged': True}
(root / 'trust-chain-emulation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))

"""Bounded offline tests of Dell's Security2 callback; synthetic boot mode.

This does not test recovery capsule acceptance, Boot Guard, or flash permission.
"""
import hashlib
import json
from pathlib import Path
import struct
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP

if unicorn.__version__ != '2.1.4':
    raise SystemExit('Requires unicorn==2.1.4')
root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
image = (root / 'bios16-a.bin').read_bytes()
expected = json.loads((root / 'uefi-volume-analysis.json').read_text())['bios_sha256']
if hashlib.sha256(image).hexdigest() != expected:
    raise SystemExit('Firmware backup changed')

def execute(mode, auth):
    emulator = Uc(UC_ARCH_X86, UC_MODE_32)
    emulator.mem_map(0xff000000, 0x1000000)
    emulator.mem_write(0xff000000, image)
    emulator.mem_map(0x200000, 0x100000)
    emulator.mem_map(0x700000, 0x10000)
    stack, sentinel, defer = 0x708000, 0x220000, 0x230000
    emulator.mem_write(defer, b'\xff')
    # Security2: PeiServices, This, AuthStatus, FvHandle, FileHandle, Defer.
    emulator.mem_write(stack, struct.pack('<7I', sentinel, 0, 0, auth, 0, 0, defer))
    emulator.reg_write(UC_X86_REG_ESP, stack)
    calls = []

    def get_boot_mode(cpu, address, size, user_data):
        calls.append(hex(address))
        sp = cpu.reg_read(UC_X86_REG_ESP)
        destination = struct.unpack('<I', cpu.mem_read(sp, 4))[0]
        cpu.reg_write(UC_X86_REG_EAX, mode)
        cpu.reg_write(UC_X86_REG_ESP, sp + 4)
        cpu.reg_write(UC_X86_REG_EIP, destination)

    emulator.hook_add(UC_HOOK_CODE, get_boot_mode, begin=0xfffb0dc0, end=0xfffb0dc0)
    emulator.emu_start(0xfffb095c, sentinel, timeout=2000000, count=100000)
    if emulator.reg_read(UC_X86_REG_EIP) != sentinel or len(calls) != 1:
        raise RuntimeError('Unexpected callback path')
    return {'boot_mode': hex(mode), 'authentication_status': hex(auth),
            'efi_status': hex(emulator.reg_read(UC_X86_REG_EAX)),
            'defer_execution': bool(emulator.mem_read(defer, 1)[0])}

cases = [execute(0, 2), execute(0, 10), execute(0x11, 10), execute(0x20, 10)]
if [(case['efi_status'], case['defer_execution']) for case in cases] != [
        ('0x0', False), ('0x8000001a', True), ('0x0', False), ('0x8000001a', True)]:
    raise SystemExit('Callback expectation failed')
result = {'scope': 'Original Dell Security2 callback with mocked GetBootMode only; '
                   'not proof of accepting a replacement ROM or recovery image',
          'bios_sha256': expected, 'callback': '0xfffb095c',
          'emulator': 'unicorn==2.1.4', 'cases': cases}
(root / 'trust-chain-policy-emulation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))

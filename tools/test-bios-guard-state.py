"""Execute the original BIOS Guard enable-bit branch on copied PE code only."""
import hashlib
import json
from pathlib import Path
import struct
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RDX, UC_X86_REG_RCX, UC_X86_REG_RIP

if unicorn.__version__ != '2.1.4':
    raise SystemExit('Requires unicorn==2.1.4')
root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
body = (root / 'update-modules/bios-guard-services.bin').read_bytes()
metadata = json.loads((root / 'update-module-analysis.json').read_text())
expected = next(module['sha256'] for module in metadata['modules'] if module['name'] == 'bios-guard-services')
if hashlib.sha256(body).hexdigest() != expected:
    raise SystemExit('Module changed')
msr = json.loads((root / 'bios-guard-msr.json').read_text())
controls = {int(cpu['bios_guard_control'], 16) for cpu in msr['cpus'].values()}
if len(controls) != 1:
    raise SystemExit('CPUs disagree; investigate before testing')
actual = controls.pop()
pe = struct.unpack_from('<I', body, 0x3c)[0]
if body[pe:pe + 4] != b'PE\0\0' or struct.unpack_from('<H', body, pe + 4)[0] != 0x8664:
    raise SystemExit('Expected x64 PE')
count = struct.unpack_from('<H', body, pe + 6)[0]
optional_size = struct.unpack_from('<H', body, pe + 20)[0]
table = pe + 24 + optional_size

def execute(control):
    emulator = Uc(UC_ARCH_X86, UC_MODE_64)
    emulator.mem_map(0, 0x20000)
    for index in range(count):
        offset = table + 40 * index
        virtual_size, rva, size, raw_offset = struct.unpack_from('<IIII', body, offset + 8)
        if raw_offset + size > len(body) or rva + max(size, virtual_size) > 0x20000:
            raise ValueError('Invalid PE section mapping')
        emulator.mem_write(rva, body[raw_offset:raw_offset + size])
    calls = []

    def read_msr(cpu, address, size, user_data):
        if cpu.reg_read(UC_X86_REG_RCX) != 0x110:
            raise RuntimeError('Unexpected MSR')
        calls.append(address)
        cpu.reg_write(UC_X86_REG_RAX, control & 0xffffffff)
        cpu.reg_write(UC_X86_REG_RDX, control >> 32)
        cpu.reg_write(UC_X86_REG_RIP, address + size)

    def stop(cpu, address, size, user_data):
        cpu.emu_stop()

    emulator.hook_add(UC_HOOK_CODE, read_msr, begin=0x1686, end=0x1686)
    for destination in (0x1693, 0x16cd):
        emulator.hook_add(UC_HOOK_CODE, stop, begin=destination, end=destination)
    emulator.emu_start(0x1681, 0x20000, timeout=1000000, count=100)
    target = emulator.reg_read(UC_X86_REG_RIP)
    if len(calls) != 1 or target not in (0x1693, 0x16cd):
        raise RuntimeError('Unexpected enable check path')
    return {'control': hex(control), 'enabled_branch_taken': target == 0x1693}

cases = [execute(actual), execute(0), execute(1)]
if [case['enabled_branch_taken'] for case in cases] != [True, False, False]:
    raise SystemExit('Enable branch controls failed')
result = {'scope': 'Offline original enable check only, mocked RDMSR; '
                   'no execution of BIOS Guard update service',
          'module_sha256': expected, 'boot_id': msr['boot_id'], 'cases': cases}
(root / 'bios-guard-state-emulation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))

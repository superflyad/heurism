"""Run copied BIOS Guard dispatch/result code with MSRs mocked inside Unicorn."""
import hashlib
import json
from pathlib import Path
import struct
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RBX, UC_X86_REG_RCX,
                              UC_X86_REG_RDX, UC_X86_REG_RSI, UC_X86_REG_RDI,
                              UC_X86_REG_R12, UC_X86_REG_R15, UC_X86_REG_RIP)

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
body = (root / 'update-modules/bios-guard-services.bin').read_bytes()
expected = 'aed7c89f3093a1a90f6f9b8ab8e76661200966e39f931030eef80c9cdae7939c'
if unicorn.__version__ != '2.1.4' or hashlib.sha256(body).hexdigest() != expected:
    raise SystemExit('Pinned emulator/module changed')
pe = struct.unpack_from('<I', body, 0x3c)[0]
count = struct.unpack_from('<H', body, pe + 6)[0]
table = pe + 24 + struct.unpack_from('<H', body, pe + 20)[0]

def new_cpu():
    cpu = Uc(UC_ARCH_X86, UC_MODE_64)
    cpu.mem_map(0, 0x20000)
    for index in range(count):
        offset = table + 40 * index
        virtual_size, rva, size, raw = struct.unpack_from('<IIII', body, offset + 8)
        if raw + size > len(body) or rva + max(size, virtual_size) > 0x20000:
            raise ValueError('Invalid section')
        cpu.mem_write(rva, body[raw:raw + size])
    cpu.mem_map(0x123450000, 0x1000)
    return cpu

def stop(cpu, address, size, data):
    cpu.emu_stop()

launches = []
context = 0x123450000
for result in [0, 0x8001, 0x1234567887658002]:
    cpu = new_cpu()
    cpu.reg_write(UC_X86_REG_RSI, context)
    accesses = []

    def mocked_msr(cpu, address, size, data):
        msr = cpu.reg_read(UC_X86_REG_RCX) & 0xffffffff
        if address in (0x1c08, 0x1c13):
            value = ((cpu.reg_read(UC_X86_REG_RDX) & 0xffffffff) << 32) | (cpu.reg_read(UC_X86_REG_RAX) & 0xffffffff)
            accesses.append({'operation': 'mock_write', 'msr': hex(msr), 'value': hex(value)})
        elif address == 0x1c1a and msr == 0x115:
            accesses.append({'operation': 'mock_read', 'msr': hex(msr), 'value': hex(result)})
            cpu.reg_write(UC_X86_REG_RAX, result & 0xffffffff)
            cpu.reg_write(UC_X86_REG_RDX, result >> 32)
        else:
            raise RuntimeError('Unexpected privileged instruction')
        cpu.reg_write(UC_X86_REG_RIP, address + size)

    for address in (0x1c08, 0x1c13, 0x1c1a):
        cpu.hook_add(UC_HOOK_CODE, mocked_msr, begin=address, end=address)
    cpu.hook_add(UC_HOOK_CODE, stop, begin=0x1c2e, end=0x1c2e)
    cpu.emu_start(0x1bf1, 0x20000, timeout=1000000, count=100)
    stored = struct.unpack('<Q', cpu.mem_read(context + 0x58, 8))[0]
    if accesses != [{'operation': 'mock_write', 'msr': '0x115', 'value': hex(context + 0x28)},
                    {'operation': 'mock_write', 'msr': '0x116', 'value': '0x0'},
                    {'operation': 'mock_read', 'msr': '0x115', 'value': hex(result)}] or stored != result:
        raise RuntimeError('Unexpected dispatch argument/result handling')
    launches.append({'accesses': accesses, 'stored_result': hex(stored)})

cases = []
statuses = [0, 1, 2, 5, 6, 7, 0x7fff, 0x8000, 0x8001, 0x8002, 0x8003, 0x8004, 0x8005, 0x8006, 0xfffe, 0xffff]
for status in statuses:
    cpu = new_cpu()
    cpu.mem_write(context + 0x58, struct.pack('<Q', status))
    cpu.reg_write(UC_X86_REG_RDI, context)
    cpu.reg_write(UC_X86_REG_R12, 1)
    cpu.reg_write(UC_X86_REG_R15, 0)
    cpu.hook_add(UC_HOOK_CODE, stop, begin=0x1e97, end=0x1e97)
    cpu.emu_start(0x1e42, 0x20000, timeout=1000000, count=100)
    actual = cpu.reg_read(UC_X86_REG_RBX)
    wanted = (0 if status == 0 else 0x8000000000000003 if status in (1, 6, 0xffff)
              else 0x8000000000000007 if status in (0x8001, 0x8002, 0x8004, 0x8005)
              else 0x8000000000000002)
    if actual != wanted or cpu.reg_read(UC_X86_REG_RIP) != 0x1e97:
        raise RuntimeError('Unexpected result mapping')
    cases.append({'bios_guard_status': hex(status), 'efi_status': hex(actual)})
report = {'scope': 'Offline original BSP dispatch and return mapping; synthetic context and mocked MSRs. '
                   'No CPU BIOS Guard execution, AP rendezvous, I/O, firmware update, or hardware success proof.',
          'module_sha256': expected, 'dispatch_cases': launches, 'result_cases': cases}
(root / 'bios-guard-dispatch-emulation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'dispatch_cases': len(launches), 'result_cases': len(cases), 'scope': report['scope']}, indent=2))

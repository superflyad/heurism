"""Test original DXE verifier's FV-origin branch with synthetic EFI protocols.

This does NOT establish that the unsigned FV becomes an EFI FV2 protocol or
that the DXE dispatcher accepts its drivers. No physical firmware is modified.
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
body = (root / 'update-modules/security-stub-dxe.bin').read_bytes()
expected = '73401dd92e416dc062cccd54128bd40555c1105511e07e03323637a2eee6e728'
if unicorn.__version__ != '2.1.4' or hashlib.sha256(body).hexdigest() != expected:
    raise SystemExit('Pinned module or emulator changed')
module = 0x1000000
fv_guid = uuid.UUID('220e73b6-6bdb-4413-8405-b974b108619a')

def execute(fv_protocol, secure_boot):
    cpu = Uc(UC_ARCH_X86, UC_MODE_64)
    cpu.mem_map(module, 0x100000)
    cpu.mem_write(module, body)
    cpu.mem_map(0x200000, 0x200000)
    services, stack, sentinel = 0x250000, 0x3f8008, 0x200000
    cpu.mem_write(module + 0x729c0, struct.pack('<Q', services))
    for offset in (0x48, 0xb8, 0x118):
        cpu.mem_write(services + offset, struct.pack('<Q', 0x210000 + offset))
    path = 0x240000
    cpu.mem_write(path, b'\x04\x06\x14\x00' + bytes(16) + b'\x7f\xff\x04\x00')
    trace = []

    def mock(cpu, address, size, data):
        sp = cpu.reg_read(UC_X86_REG_RSP)
        ret = struct.unpack('<Q', cpu.mem_read(sp, 8))[0]
        value = 0
        if address == module + 0x2de4:
            name_at, out = cpu.reg_read(UC_X86_REG_RCX), cpu.reg_read(UC_X86_REG_RDX)
            name = bytes(cpu.mem_read(name_at, 32)).decode('utf-16-le').split('\0')[0]
            if name not in ('AuditMode', 'SecureBoot'):
                raise RuntimeError('Unexpected variable')
            cpu.mem_write(0x260000, bytes([secure_boot if name == 'SecureBoot' else 0]))
            cpu.mem_write(out, struct.pack('<Q', 0x260000))
            trace.append(name)
        elif address == 0x2100b8:
            guid = uuid.UUID(bytes_le=bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_RCX), 16)))
            trace.append('LocateDevicePath:' + str(guid))
            if fv_protocol and guid == fv_guid:
                cpu.mem_write(cpu.reg_read(UC_X86_REG_R8), struct.pack('<Q', 0x270000))
            else:
                value = 0x800000000000000e
        elif address == 0x210118:
            trace.append('OpenProtocol:FV2')
        elif address in (module + 0x10cc, module + 0x8044):
            trace.append('Error-reporting fixture:' + hex(address - module))
        elif address != 0x210048:
            raise RuntimeError('Unexpected service')
        cpu.reg_write(UC_X86_REG_RAX, value)
        cpu.reg_write(UC_X86_REG_RSP, sp + 8)
        cpu.reg_write(UC_X86_REG_RIP, ret)

    for address in (module + 0x2de4, module + 0x10cc, module + 0x8044, 0x2100b8, 0x210118, 0x210048):
        cpu.hook_add(UC_HOOK_CODE, mock, begin=address, end=address)
    cpu.mem_write(stack, struct.pack('<Q', sentinel) + bytes(0x40))
    cpu.reg_write(UC_X86_REG_RSP, stack)
    # Security handler: AuthStatus, DevicePath, ImageBuffer, ImageSize.
    cpu.reg_write(UC_X86_REG_RCX, 0)
    cpu.reg_write(UC_X86_REG_RDX, path)
    cpu.reg_write(UC_X86_REG_R8, 0)
    cpu.reg_write(UC_X86_REG_R9, 0)
    try:
        cpu.emu_start(module + 0x4e30, sentinel, timeout=2000000, count=100000)
    except unicorn.UcError as error:
        raise RuntimeError('Unexpected instruction/service at ' + hex(cpu.reg_read(UC_X86_REG_RIP)) + ': ' + json.dumps(trace)) from error
    if cpu.reg_read(UC_X86_REG_RIP) != sentinel:
        raise RuntimeError('Original verifier did not return')
    return {'fv2_protocol_fixture': fv_protocol, 'secure_boot_fixture': secure_boot,
            'image_buffer_fixture': 'NULL', 'efi_status': hex(cpu.reg_read(UC_X86_REG_RAX)),
            'mocked_calls': trace}

cases = [execute(True, 1), execute(False, 1), execute(False, 0)]
if [c['efi_status'] for c in cases] != ['0x0', '0x8000000000000002', '0x0']:
    raise RuntimeError('Unexpected policy result: ' + json.dumps(cases))
report = {'scope': __doc__, 'module_sha256': expected, 'handler_rva': '0x4e30',
          'origin_classifier_rva': '0x32e8', 'fv2_protocol_guid': str(fv_guid), 'cases': cases}
(root / 'dxe-origin-policy-emulation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))

"""Execute recovery policy/key receivers against the actual ROM PPI descriptors.

PEI services are mocked; this tests copied code and static providers, not a live
recovery invocation or acceptance of owner firmware.
"""
import hashlib
import json
from pathlib import Path
import struct
import uuid
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
rom = (root / 'bios16-a.bin').read_bytes()
expected = '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
if unicorn.__version__ != '2.1.4' or hashlib.sha256(rom).hexdigest() != expected:
    raise SystemExit('Pinned emulator/ROM changed')
descriptors = []
for offset in range(0xf955b8, 0xf95648, 12):
    flags, guid_pointer, interface = struct.unpack_from('<III', rom, offset)
    guid = rom[guid_pointer - 0xff000000:guid_pointer - 0xff000000 + 16]
    descriptors.append({'offset': offset, 'flags': flags, 'guid': guid, 'interface': interface})
    if flags & 0x80000000:
        break
policy_guid = uuid.UUID('8251115f-c0e5-49de-9936-1846f238206f').bytes_le
registry_guid = uuid.UUID('acdc2cf7-5524-4d14-a42a-01992ac613be').bytes_le
policy = next(d for d in descriptors if d['guid'] == policy_guid)
registry = next(d for d in descriptors if d['guid'] == registry_guid)
if policy['interface'] != 0xfff957dc or registry['interface'] != 0xfff94f50:
    raise SystemExit('Static provider changed')
if rom[0xf957dc:0xf957de] != b'\x01\x01' or struct.unpack_from('<III', rom, 0xf94f50) != (1, 11, 0xfff95c20):
    raise SystemExit('Policy/registry contents changed')

def new_cpu(missing=()):
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0xff000000, 0x1000000)
    cpu.mem_write(0xff000000, rom)
    cpu.mem_map(0x200000, 0x100000)
    services, vtable, locate = 0x210000, 0x211000, 0x212000
    cpu.mem_write(services, struct.pack('<I', vtable))
    cpu.mem_write(vtable + 0x20, struct.pack('<I', locate))

    def mock(cpu, address, size, data):
        stack = cpu.reg_read(UC_X86_REG_ESP)
        return_at = struct.unpack('<I', cpu.mem_read(stack, 4))[0]
        if address == 0xfff96c14:
            value = services
        elif address == locate:
            args = struct.unpack('<IIIII', cpu.mem_read(stack + 4, 20))
            _, guid_at, instance, descriptor_out, interface_out = args
            guid = bytes(cpu.mem_read(guid_at, 16))
            matches = [d for d in descriptors if d['guid'] == guid and guid not in missing]
            if instance != 0:
                raise RuntimeError('Unexpected PPI instance')
            if matches:
                chosen = matches[0]
                if descriptor_out:
                    cpu.mem_write(descriptor_out, struct.pack('<I', 0xff000000 + chosen['offset']))
                if interface_out:
                    cpu.mem_write(interface_out, struct.pack('<I', chosen['interface']))
                value = 0
            else:
                value = 0x8000000e
        else:
            raise RuntimeError('Unexpected mock service')
        cpu.reg_write(UC_X86_REG_EAX, value)
        cpu.reg_write(UC_X86_REG_ESP, stack + 4)
        cpu.reg_write(UC_X86_REG_EIP, return_at)

    for address in (0xfff96c14, locate):
        cpu.hook_add(UC_HOOK_CODE, mock, begin=address, end=address)
    return cpu

def call(cpu, address, args):
    stack, stop = 0x2f8000, 0x220000
    cpu.mem_write(stack, struct.pack('<' + 'I' * (len(args) + 1), stop, *args))
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.emu_start(address, stop, timeout=1000000, count=100000)
    if cpu.reg_read(UC_X86_REG_EIP) != stop:
        raise RuntimeError('Function did not return')
    return cpu.reg_read(UC_X86_REG_EAX)

cases = []
for name, missing, wanted_auth, wanted_registry in [
        ('actual_static_providers', (), 1, 0xfff94f50),
        ('policy_absent_registry_present', (policy_guid,), 1, 0xfff94f50),
        ('both_absent', (policy_guid, registry_guid), 0, 0)]:
    cpu = new_cpu(missing)
    out = 0x230000
    cpu.mem_write(out, b'\x01\0\0\0')  # Caller's original default auth flag.
    auth_status = call(cpu, 0xfff9aef6, [out])
    auth = cpu.mem_read(out, 1)[0]
    cpu.mem_write(out, b'\0' * 4)
    registry_status = call(cpu, 0xfff9af51, [out])
    registry_at = struct.unpack('<I', cpu.mem_read(out, 4))[0]
    if auth != wanted_auth or registry_at != wanted_registry:
        raise RuntimeError('Unexpected receiver policy')
    cases.append({'name': name, 'authentication_required': bool(auth),
                  'authentication_helper_status': hex(auth_status),
                  'registry_helper_status': hex(registry_status), 'registry_address': hex(registry_at)})
lookups = []
for index in range(12):
    cpu = new_cpu()
    if index < 11:
        record = struct.unpack_from('<I', rom, 0xf95c20 + index * 8)[0] - 0xff000000
        guid = rom[record + 12:record + 28]
    else:
        guid = b'\x55' * 16
    cpu.mem_write(0x230000, guid)
    cpu.mem_write(0x240000, struct.pack('<I', 0xffffffff))
    found = bool(call(cpu, 0xfff9b3d7, [registry['interface'], 0x230000, 0x240000]) & 0xff)
    selected = struct.unpack('<I', cpu.mem_read(0x240000, 4))[0]
    if found != (index < 11) or (found and selected != index):
        raise RuntimeError('Actual registry lookup differs')
    lookups.append({'guid': str(uuid.UUID(bytes_le=guid)), 'found': found, 'index': selected if found else None})
report = {'scope': 'Offline original receivers with actual static ROM PPI interfaces; mocked PEI services. '
                   'Not live PPI lifetime, complete recovery, BIOS Guard authorization, or flash/boot proof.',
          'rom_sha256': expected, 'bindings_install_ppi_call': '0xfff92ee1',
          'bindings_descriptor_list': '0xfff955b8', 'policy_interface': hex(policy['interface']),
          'registry_interface': hex(registry['interface']), 'cases': cases, 'lookup_cases': lookups}
(root / 'recovery-registry-emulation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'receiver_cases': cases, 'actual_registry_lookup_cases': len(lookups)}, indent=2))

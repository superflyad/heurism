"""Test saved Dell Supported() code offline, with explicit protocol fixtures.

Only two bounded leaf functions execute in Unicorn. OpenProtocol TEST_PROTOCOL
is simulated: no vendor entry point, Start(), firmware API or network runs.
"""
import hashlib
import json
from pathlib import Path
import struct
import uuid

import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8,
                              UC_X86_REG_R9, UC_X86_REG_RSP, UC_X86_REG_RIP,
                              UC_X86_REG_RAX)

repo = Path(__file__).resolve().parents[1]
out = repo / 'artifacts/research/independent-bootstrap'
body = (out / 'http-boot-dxe.bin').read_bytes()
digest = hashlib.sha256(body).hexdigest()
assert digest == 'd992982aaef66ab249a4811d1a619eac3166e0cc66aef6a75e88014869020788'
assert unicorn.__version__ == '2.1.4'
pe = struct.unpack_from('<I', body, 60)[0]
section = pe + 24 + struct.unpack_from('<H', body, pe + 20)[0]
mapped = bytearray(0x20000)
for i in range(struct.unpack_from('<H', body, pe + 6)[0]):
    _, virtual, rva, size, start = struct.unpack_from('<8sIIII', body, section + 40*i)
    assert rva + max(virtual, size) <= len(mapped) and start + size <= len(body)
    mapped[rva:rva+size] = body[start:start+size]

names = {
    '9d9a39d8-bd42-4a73-a4d5-8ee94be11380': 'DHCP4_SERVICE_BINDING',
    '9fb9a8a1-2f4a-43a6-889c-d0f7b6c47ad5': 'DHCP6_SERVICE_BINDING',
    'bdc8e6af-d9bc-4379-a72a-e0c4e75dae1c': 'HTTP_SERVICE_BINDING',
    '09576e91-6d3f-11d2-8e39-00a0c969723b': 'DEVICE_PATH',
}
results = []
for family, binding_rva, expected_rva, last in [
    ('IPv4', 0xc188, 0xf54, 0xff9), ('IPv6', 0xc158, 0x1510, 0x15b5)
]:
    supported, start, stop, version, image, binding = struct.unpack_from('<QQQI4xQQ', mapped, binding_rva)
    assert supported == expected_rva and version == 10 and image == binding == 0
    family_results = []
    # Every prerequisite succeeds, then fail each one in turn. Both common
    # missing-interface and access-denied statuses must propagate unchanged.
    for fail_at, failure in [(None, 0)] + [(n, s) for n in range(3) for s in
                                       (0x8000000000000003, 0x800000000000000f)]:
        cpu = Uc(UC_ARCH_X86, UC_MODE_64)
        base, arena, table, external, sentinel = 0x1000000, 0x200000, 0x202000, 0x200100, 0x200000
        cpu.mem_map(base, len(mapped)); cpu.mem_write(base, bytes(mapped))
        cpu.mem_map(arena, 0x10000)
        cpu.mem_write(base+0xd6a0, struct.pack('<Q', table))
        cpu.mem_write(table+0x118, struct.pack('<Q', external))
        cpu.mem_write(external, b'\xc3')
        cpu.mem_write(base+binding_rva+40, struct.pack('<Q', 0x1234))
        stack = 0x208008
        cpu.mem_write(stack, struct.pack('<Q', sentinel))
        cpu.reg_write(UC_X86_REG_RCX, base+binding_rva)
        cpu.reg_write(UC_X86_REG_RDX, 0x5678)
        cpu.reg_write(UC_X86_REG_R8, 0)
        cpu.reg_write(UC_X86_REG_RSP, stack)
        calls = []
        def trace(uc, address, length, data):
            if address == external:
                rsp = uc.reg_read(UC_X86_REG_RSP)
                assert uc.reg_read(UC_X86_REG_RCX) == 0x5678
                assert uc.reg_read(UC_X86_REG_R8) == 0
                assert uc.reg_read(UC_X86_REG_R9) == 0x1234
                controller, attrs = struct.unpack('<QQ', bytes(uc.mem_read(rsp+40, 16)))
                assert controller == 0x5678 and attrs == 4
                guid = str(uuid.UUID(bytes_le=bytes(uc.mem_read(uc.reg_read(UC_X86_REG_RDX), 16))))
                status = failure if len(calls) == fail_at else 0
                calls.append({'protocol': names.get(guid, guid), 'guid': guid, 'status': hex(status)})
                uc.reg_write(UC_X86_REG_RAX, status)
            elif not base+supported <= address <= base+last:
                raise RuntimeError('Unexpected instruction ' + hex(address))
        cpu.hook_add(UC_HOOK_CODE, trace)
        cpu.emu_start(base+supported, sentinel, timeout=1000000, count=2000)
        assert cpu.reg_read(UC_X86_REG_RIP) == sentinel
        status = cpu.reg_read(UC_X86_REG_RAX)
        assert status == failure
        assert len(calls) == (3 if fail_at is None else fail_at+1)
        family_results.append({'fail_at': fail_at, 'result': hex(status), 'calls': calls})
    assert [x['protocol'] for x in family_results[0]['calls']] == [
        'DHCP4_SERVICE_BINDING' if family == 'IPv4' else 'DHCP6_SERVICE_BINDING',
        'HTTP_SERVICE_BINDING', 'DEVICE_PATH']
    results.append({'family': family, 'binding_rva': hex(binding_rva),
                    'supported_rva': hex(supported), 'start_rva': hex(start),
                    'stop_rva': hex(stop), 'cases': family_results})
report = {'scope': __doc__, 'module_sha256': digest, 'cases_passed': 14,
          'bindings': results,
          'limits': 'Mocked protocol presence only. Does not prove physical protocol co-location, driver dispatch, Start(), download, TLS trust or SSD-independent startup.'}
(out / 'binding-tests.json').write_text(json.dumps(report, indent=2))
print(json.dumps({'cases_passed': 14, 'bindings': [{k: v for k, v in b.items() if k != 'cases'} for b in results]}, indent=2))

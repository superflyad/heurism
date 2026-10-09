"""Run only Dell's saved UpdateCapsule early FMP branch in an offline emulator.

No firmware service or physical UpdateCapsule entry is called. This tests the
actual machine-code branch; it does not test another capsule delivery route.
"""

import hashlib
import json
from pathlib import Path
import struct
import uuid

import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_HOOK_CODE, UC_MODE_64
from unicorn.x86_const import (
    UC_X86_REG_R8,
    UC_X86_REG_RAX,
    UC_X86_REG_RCX,
    UC_X86_REG_RDX,
    UC_X86_REG_RIP,
    UC_X86_REG_RSP,
)


root = Path(__file__).resolve().parents[1]
body = (root / "artifacts/firmware/update-modules/capsule-runtime-dxe.bin").read_bytes()
expected = "4db34759e75c0502ecb4057d4988d02ba5e5d713001ed838c86f41786f11e3aa"
assert hashlib.sha256(body).hexdigest() == expected
assert unicorn.__version__ == "2.1.4"

pe = struct.unpack_from("<I", body, 60)[0]
count = struct.unpack_from("<H", body, pe + 6)[0]
section_table = pe + 24 + struct.unpack_from("<H", body, pe + 20)[0]
base = 0x1000000
cpu = Uc(UC_ARCH_X86, UC_MODE_64)
cpu.mem_map(base, 0x10000)
for index in range(count):
    _, virtual_size, rva, raw_size, raw_at = struct.unpack_from(
        "<8sIIII", body, section_table + 40 * index
    )
    assert rva + max(virtual_size, raw_size) <= 0x10000
    assert raw_at + raw_size <= len(body)
    if raw_size:
        cpu.mem_write(base + rva, body[raw_at : raw_at + raw_size])

cpu.mem_map(0x200000, 0x200000)
array = 0x240000
capsule = 0x241000
stack = 0x3f8008
sentinel = 0x200000
fmp_guid = uuid.UUID("6dcbd5ed-e82d-4c44-bda1-7194199ad92a")
cpu.mem_write(array, struct.pack("<Q", capsule))
cpu.mem_write(capsule, fmp_guid.bytes_le + struct.pack("<III", 28, 0x10000, 28))
cpu.mem_write(stack, struct.pack("<Q", sentinel) + bytes(0x100))

visited = []


def trace(uc, address, size, unused):
    if address == sentinel:
        return
    if not base <= address < base + 0x10000:
        raise RuntimeError("Unexpected external call " + hex(address))
    visited.append(address - base)


cpu.hook_add(UC_HOOK_CODE, trace)
for register, value in (
    (UC_X86_REG_RSP, stack),
    (UC_X86_REG_RCX, array),
    (UC_X86_REG_RDX, 1),
    (UC_X86_REG_R8, 0),
):
    cpu.reg_write(register, value)
cpu.emu_start(base + 0x1388, sentinel, timeout=1000000, count=100000)
assert cpu.reg_read(UC_X86_REG_RIP) == sentinel
status = cpu.reg_read(UC_X86_REG_RAX)
assert status == 0x8000000000000002, hex(status)  # EFI_INVALID_PARAMETER
assert 0x13F6 in visited and 0x13B9 in visited and 0x160A in visited
assert not any(rva >= 0x140B and rva < 0x1603 for rva in visited)
report = {
    "scope": __doc__,
    "module_sha256": expected,
    "capsule_guid": str(fmp_guid),
    "initial_flags": "0x10000",
    "returned_status": hex(status),
    "final_flags": hex(struct.unpack("<I", cpu.mem_read(capsule + 20, 4))[0]),
    "early_fmp_rejection_branch_taken": True,
    "external_firmware_services_called": False,
    "physical_capsule_delivery_tested": False,
}
destination = root / "artifacts/firmware/fmp-capsule-branch.json"
destination.write_text(json.dumps(report, indent=2), encoding="utf-8")
print(json.dumps(report, indent=2))

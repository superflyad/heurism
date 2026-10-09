"""Execute the saved Dell BDS SysPrep decision branch with bounded offline stubs.

This is not a whole-firmware boot and never contacts the physical Dell.
"""

import hashlib
from pathlib import Path
import struct

from unicorn import Uc, UC_ARCH_X86, UC_HOOK_CODE, UC_MODE_64
from unicorn.x86_const import (
    UC_X86_REG_R13, UC_X86_REG_RAX, UC_X86_REG_RBP, UC_X86_REG_RBX,
    UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_RIP, UC_X86_REG_RSP,
)

root = Path(__file__).resolve().parents[1]
body = (root / "artifacts/research/independent-bootstrap/bds-dxe.bin").read_bytes()
expected = "827dfbe11c71f67c1a3a37a594145e21680866e8deb8b8f5be0df3f6638fb32b"
assert hashlib.sha256(body).hexdigest() == expected
base, stack, options = 0x1000000, 0x2000000, 0x2010000

def target(call_rva):
    assert body[call_rva] == 0xE8
    return call_rva + 5 + struct.unpack_from("<i", body, call_rva + 1)[0]

assert body[0x1646:0x1648] == bytes.fromhex("33 d2")  # Driver type 0
assert body[0x10A1:0x10A6] == bytes.fromhex("45 8d 6c 24 01")  # r13 = 1
assert body[0x192D:0x1930] == bytes.fromhex("41 8b d5")  # SysPrep type 1
assert [target(x) for x in (0x164F, 0x1672, 0x1937, 0x195A)] == [
    0x62DC, 0xE84, 0x62DC, 0xE84
]
pointer = lambda at: struct.unpack_from("<Q", body, at)[0]
assert [pointer(0x23180 + 8*i) for i in range(3)] == [0x23120, 0x23138, 0x23158]
assert [pointer(0x231A0 + 8*i) for i in range(3)] == [0x217F8, 0x21808, 0x21818]
assert body[0x23138:0x23138+24] == "SysPrepOrder".encode("utf-16le")
assert body[0x21808:0x21808+14] == "SysPrep".encode("utf-16le")


def run(platform_recovery):
    cpu = Uc(UC_ARCH_X86, UC_MODE_64)
    cpu.mem_map(base, 0x30000)
    cpu.mem_write(base, body)
    cpu.mem_map(stack, 0x20000)
    calls = []

    def hook(uc, address, size, unused):
        rva = address - base
        if rva not in (0x62DC, 0xD19C, 0xE84, 0x64B4):
            if not 0x1929 <= rva < 0x197F:
                raise RuntimeError(f"unexpected Dell code {rva:#x}")
            return
        if rva == 0x62DC:
            calls.append(("get_options", uc.reg_read(UC_X86_REG_RDX)))
            uc.mem_write(uc.reg_read(UC_X86_REG_RCX), struct.pack("<Q", 1))
            uc.reg_write(UC_X86_REG_RAX, options)
        elif rva == 0xE84:
            calls.append(("process", uc.reg_read(UC_X86_REG_RDX)))
            assert uc.reg_read(UC_X86_REG_RCX) == options
        elif rva == 0x64B4:
            calls.append(("free", uc.reg_read(UC_X86_REG_RDX)))
        rsp = uc.reg_read(UC_X86_REG_RSP)
        uc.reg_write(UC_X86_REG_RIP, struct.unpack("<Q", uc.mem_read(rsp, 8))[0])
        uc.reg_write(UC_X86_REG_RSP, rsp + 8)

    cpu.hook_add(UC_HOOK_CODE, hook)
    cpu.reg_write(UC_X86_REG_RSP, stack + 0x8000)
    cpu.reg_write(UC_X86_REG_RBP, stack + 0x9000)
    cpu.reg_write(UC_X86_REG_R13, 1)
    cpu.reg_write(UC_X86_REG_RBX, 0x40 if platform_recovery else 0)
    end = 0x1997 if platform_recovery else 0x197F
    cpu.emu_start(base + 0x1929, base + end, timeout=1000000, count=1000)
    assert cpu.reg_read(UC_X86_REG_RIP) == base + end
    return calls


normal = run(False)
recovery = run(True)
assert normal == [("get_options", 1), ("process", 1), ("free", 1)], normal
assert recovery == [], recovery
print("Dell BdsDxe: Driver type 0 precedes SysPrep type 1; normal boot processes SysPrep, platform recovery skips it")

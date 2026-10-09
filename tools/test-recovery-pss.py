"""Original Dell recovery key lookup and PSS verifier with an actual EC package.

Synthetic memory/arguments, real ROM list and crypto. Not the complete EFI path.
"""
import hashlib
import json
from pathlib import Path
import struct
import uuid
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
rom = (root / 'bios16-a.bin').read_bytes()
report = json.loads((root / 'recovery-signature-verification.json').read_text())
manifest = json.loads((root / 'recovery-package-analysis.json').read_text())
expected = '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
if unicorn.__version__ != '2.1.4' or hashlib.sha256(rom).hexdigest() != expected:
    raise SystemExit('Emulator or ROM changed')
item = next(x for x in manifest['files'] if ' -- 7 Embedded Controller ' in x['path'] and x['path'].endswith('.data.bin'))
path = root / 'vendor/recovery-1.35.0-extracted' / item['path']
message, signature = path.read_bytes(), path.with_suffix('.sig').read_bytes()
if hashlib.sha256(message).hexdigest() != item['sha256']:
    raise SystemExit('Actual EC image changed')
sig_item = next(x for x in manifest['files'] if x['path'] == path.with_suffix('.sig').relative_to(root / 'vendor/recovery-1.35.0-extracted').as_posix())
if hashlib.sha256(signature).hexdigest() != sig_item['sha256']:
    raise SystemExit('Actual EC signature changed')
if not any(x['sha256'] == item['sha256'] and x['signature_verified'] for x in report['checks']):
    raise SystemExit('Missing independent package verification')
key = rom[0xf94458:0xf94658]
guid = uuid.UUID('540b32b0-808d-47e1-a188-76aa7cf1bb93').bytes_le

def new_cpu():
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0xff000000, 0x1000000)
    cpu.mem_write(0xff000000, rom)
    cpu.mem_map(0x200000, 0x400000)
    cpu.mem_map(0x700000, 0x10000)
    return cpu

def call(cpu, address, arguments):
    stack, sentinel = 0x708000, 0x220000
    cpu.mem_write(stack, struct.pack('<' + 'I' * (1 + len(arguments)), sentinel, *arguments))
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.emu_start(address, sentinel, timeout=10000000, count=300000000)
    if cpu.reg_read(UC_X86_REG_EIP) != sentinel:
        raise RuntimeError('Original code did not return: ' + hex(cpu.reg_read(UC_X86_REG_EIP)))
    return bool(cpu.reg_read(UC_X86_REG_EAX) & 0xff)

lookups = []
lookup_inputs = []
for index in range(11):
    record = struct.unpack_from('<I', rom, 0xf95c20 + index * 8)[0] - 0xff000000
    pointer, size, flags = struct.unpack_from('<III', rom, record)
    if size != 512 or flags != 0x221 or not (0xfff90000 <= pointer < 0xfff96900):
        raise SystemExit('Static ROM key list differs')
    lookup_inputs.append((str(uuid.UUID(bytes_le=rom[record + 12:record + 28])),
                          rom[record + 12:record + 28], index))
lookup_inputs.append(('unknown_guid', b'\x55' * 16, None))
for name, lookup_guid, expected_index in lookup_inputs:
    cpu = new_cpu()
    # Actual static list has eleven pairs; only its receiving descriptor is synthetic.
    cpu.mem_write(0x210000, struct.pack('<III', 0, 11, 0xfff95c20))
    cpu.mem_write(0x230000, lookup_guid)
    cpu.mem_write(0x240000, struct.pack('<I', 0xffffffff))
    found = call(cpu, 0xfff9b3d7, [0x210000, 0x230000, 0x240000])
    index = struct.unpack('<I', cpu.mem_read(0x240000, 4))[0]
    if found != (expected_index is not None) or (found and index != expected_index):
        raise RuntimeError('Unexpected GUID lookup')
    lookups.append({'name': name, 'found': found, 'index': index if found else None})

cases = []
for name in ['vendor_original', 'changed_payload', 'changed_signature']:
    cpu = new_cpu()
    msg, sig = bytearray(message), bytearray(signature)
    if name == 'changed_payload':
        msg[192] ^= 1
    if name == 'changed_signature':
        sig[-1] ^= 1
    context, msg_at, key_at, sig_at = 0x210000, 0x230000, 0x290000, 0x2a0000
    cpu.mem_write(msg_at, bytes(msg))
    cpu.mem_write(key_at, key)
    cpu.mem_write(sig_at, bytes(sig))
    if not call(cpu, 0xfff97c50, [context, 0x10000, 0x221, len(key)]):
        raise RuntimeError('PSS initialization failed')
    if not call(cpu, 0xfff97d34, [context, msg_at, len(msg)]):
        raise RuntimeError('Original SHA256 update failed')
    accepted = call(cpu, 0xfff97d59, [context, key_at, len(key), sig_at, len(sig)])
    if accepted != (name == 'vendor_original'):
        raise RuntimeError('Unexpected original Dell PSS result: ' + name)
    cases.append({'name': name, 'accepted': accepted})
result = {'scope': 'Original ROM key lookup and actual vendor EC signature primitive; '
                   'synthetic descriptor, no complete recovery or write authorization',
          'rom_sha256': expected, 'ec_image_sha256': item['sha256'],
          'signature_sha256': sig_item['sha256'], 'key_guid': str(uuid.UUID(bytes_le=guid)),
          'crypto_helpers_replaced': False, 'lookup_cases': lookups, 'signature_cases': cases}
(root / 'recovery-pss-emulation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))

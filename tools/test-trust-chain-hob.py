"""Test original Dell digest-HOB comparison using actual boot event bytes.

Synthetic HOB list/globals and GetHobList substitute; not a complete boot test.
No hardware writes, persistent candidate ROM, or authentication bypass.
"""
import hashlib
import json
from pathlib import Path
import struct
import uuid
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP

if unicorn.__version__ != '2.1.4':
    raise SystemExit('Requires unicorn==2.1.4')
root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
image = (root / 'bios16-a.bin').read_bytes()
analysis = json.loads((root / 'uefi-volume-analysis.json').read_text())
log_analysis = json.loads((root / 'tpm-log-analysis.json').read_text())
log = (root / ('tpm-log-' + log_analysis['boot_id'] + '.bin')).read_bytes()
if hashlib.sha256(image).hexdigest() != analysis['bios_sha256'] or hashlib.sha256(log).hexdigest() != log_analysis['log_sha256']:
    raise SystemExit('Verified inputs differ')
guid = image[0xfb1694:0xfb16a4]

def execute(event, record):
    emulator = Uc(UC_ARCH_X86, UC_MODE_32)
    emulator.mem_map(0xff000000, 0x1000000)
    emulator.mem_write(0xff000000, image)
    emulator.mem_map(0x200000, 0x400000)
    emulator.mem_map(0x700000, 0x10000)
    # EFI_HOB_GUID_TYPE, packed event bytes and 8-byte-aligned HOB length.
    length = (24 + len(event) + 7) & ~7
    hob = struct.pack('<HHI', 4, length, 0) + guid + event
    hob += b'\0' * (length - len(hob)) + struct.pack('<HHI', 0xffff, 8, 0)
    emulator.mem_write(0x230000, hob)
    emulator.mem_write(0xfffb1850, b'\x01')
    emulator.mem_write(0xfffb1854, struct.pack('<I', 0xfffb1694))
    emulator.mem_write(0xfffb1858, struct.pack('<I', 2))
    stack, sentinel = 0x708000, 0x220000
    emulator.mem_write(stack, struct.pack('<I', sentinel))
    emulator.reg_write(UC_X86_REG_ESP, stack)
    emulator.reg_write(UC_X86_REG_ECX, 0x210000)
    emulator.reg_write(UC_X86_REG_EDX, record)
    calls = []

    def get_hob_list(cpu, address, size, user_data):
        calls.append(hex(address))
        sp = cpu.reg_read(UC_X86_REG_ESP)
        destination = struct.unpack('<I', cpu.mem_read(sp, 4))[0]
        cpu.reg_write(UC_X86_REG_EAX, 0x230000)
        cpu.reg_write(UC_X86_REG_ESP, sp + 4)
        cpu.reg_write(UC_X86_REG_EIP, destination)

    emulator.hook_add(UC_HOOK_CODE, get_hob_list, begin=0xfffb0d48, end=0xfffb0d48)
    emulator.emu_start(0xfffb0b73, sentinel, timeout=2000000, count=1000000)
    if emulator.reg_read(UC_X86_REG_EIP) != sentinel or len(calls) != 2:
        raise RuntimeError('Unexpected return/path')
    return bool(emulator.reg_read(UC_X86_REG_EAX) & 0xff)

results = []
for event in log_analysis['events']:
    if event['pcr'] != 0 or event['event_type'] not in ('0x1', '0x80000008'):
        continue
    for match in event.get('rom_volume_digest_matches', []):
        reference = next((item for item in analysis['digest_references']
                          if item['volume_offset'] == match['volume_offset']
                          and item['algorithm'] == 'sha256' and item['table_inside_ibb']
                          and item['header_bytes_skipped'] == 0), None)
        if not reference:
            continue
        record = 0xff000000 + int(reference['digest_offset'], 16) - 20
        raw = log[event['offset']:event['offset'] + event['bytes']]
        altered = bytearray(raw)
        altered[14] ^= 1  # SHA256 digest after 12-byte header and 2-byte algorithm ID.
        wrong_pcr = bytearray(raw)
        wrong_pcr[:4] = struct.pack('<I', 1)
        wrong_type = bytearray(raw)
        wrong_type[4:8] = struct.pack('<I', 4)
        outcome = {'volume_offset': match['volume_offset'], 'event_offset': event['offset'],
                   'original_event_matched': execute(raw, record),
                   'changed_digest_matched': execute(bytes(altered), record),
                   'wrong_pcr_matched': execute(bytes(wrong_pcr), record),
                   'wrong_event_type_matched': execute(bytes(wrong_type), record)}
        if not outcome['original_event_matched'] or any(outcome[key] for key in
                ('changed_digest_matched', 'wrong_pcr_matched', 'wrong_event_type_matched')):
            raise SystemExit('Negative control failed: ' + json.dumps(outcome))
        results.append(outcome)
if not results:
    raise SystemExit('No protected-table/event matches tested')
result = {'scope': 'Original Dell mode-2 HOB comparison with actual boot event bytes; '
                   'synthetic globals/list and GetHobList; no proof of physical path selection',
          'bios_sha256': analysis['bios_sha256'], 'boot_id': log_analysis['boot_id'],
          'log_sha256': log_analysis['log_sha256'], 'hob_guid': str(uuid.UUID(bytes_le=guid)),
          'emulator': 'unicorn==2.1.4', 'cases': results}
(root / 'trust-chain-hob-emulation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))

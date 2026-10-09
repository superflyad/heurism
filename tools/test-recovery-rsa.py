"""Run original Dell recovery SHA-256/RSA verifier on synthetic public fixtures.

No emulated crypto substitution, EFI update, private Dell key, or hardware access.
This tests a primitive, not the recovery policy or caller's trusted-key selection.
"""
import hashlib
import json
from pathlib import Path
import struct
import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP

if unicorn.__version__ != '2.1.4':
    raise SystemExit('Requires unicorn 2.1.4')
root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
rom = (root / 'bios16-a.bin').read_bytes()
expected = '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c'
if hashlib.sha256(rom).hexdigest() != expected:
    raise SystemExit('ROM reference changed')
fixture = json.loads((root / 'recovery-rsa-fixture.json').read_text())
message = bytes.fromhex(fixture['message_hex'])
signature = bytes.fromhex(fixture['signature_hex'])
key = bytes.fromhex(fixture['modulus_hex']) + fixture['exponent'].to_bytes(256, 'big')
modulus = int(fixture['modulus_hex'], 16)
digest_info = bytes.fromhex('3031300d060960864801650304020105000420') + hashlib.sha256(message).digest()
encoded = b'\x00\x01' + b'\xff' * (256 - len(digest_info) - 3) + b'\x00' + digest_info
if modulus.bit_length() != 2048 or fixture['exponent'] != 65537 or len(signature) != 256:
    raise SystemExit('Unexpected synthetic RSA fixture')
if pow(int.from_bytes(signature, 'big'), fixture['exponent'], modulus).to_bytes(256, 'big') != encoded:
    raise SystemExit('Independent Python RSA fixture verification failed')
cases = []
for name in ['original', 'changed_message', 'changed_signature', 'changed_key']:
    cpu = Uc(UC_ARCH_X86, UC_MODE_32)
    cpu.mem_map(0xff000000, 0x1000000)
    cpu.mem_write(0xff000000, rom)
    cpu.mem_map(0x200000, 0x400000)
    cpu.mem_map(0x700000, 0x10000)
    context, msg_at, sig_at, key_at = 0x210000, 0x230000, 0x240000, 0x250000
    msg, sig, pub = bytearray(message), bytearray(signature), bytearray(key)
    if name == 'changed_message':
        msg[0] ^= 1
    if name == 'changed_signature':
        sig[-1] ^= 1
    if name == 'changed_key':
        pub[-1] ^= 2
    cpu.mem_write(msg_at, bytes(msg))
    cpu.mem_write(sig_at, bytes(sig))
    cpu.mem_write(key_at, bytes(pub))

    def call(address, arguments):
        stack, sentinel = 0x708000, 0x220000
        cpu.mem_write(stack, struct.pack('<' + 'I' * (1 + len(arguments)), sentinel, *arguments))
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.emu_start(address, sentinel, timeout=10000000, count=300000000)
        if cpu.reg_read(UC_X86_REG_EIP) != sentinel:
            raise RuntimeError('Verifier did not return normally at ' + hex(cpu.reg_read(UC_X86_REG_EIP)))
        return bool(cpu.reg_read(UC_X86_REG_EAX) & 0xff)

    if not call(0xfff97c50, [context, 0x10000, 0x121, len(key)]):
        raise RuntimeError('Initialization failed')
    if not call(0xfff97d34, [context, msg_at, len(msg)]):
        raise RuntimeError('Hash update failed')
    accepted = call(0xfff97d59, [context, key_at, len(pub), sig_at, len(sig)])
    if accepted != (name == 'original'):
        raise RuntimeError('Unexpected verification result: ' + name)
    cases.append({'name': name, 'accepted': accepted})
report = {'scope': 'Original recovery crypto primitive with synthetic RSA key; '
                   'not Dell trust policy or firmware acceptance',
          'bios_sha256': expected, 'unicorn_version': unicorn.__version__,
          'fixture_sha256': hashlib.sha256((root / 'recovery-rsa-fixture.json').read_bytes()).hexdigest(),
          'independent_python_fixture_verification': True,
          'flags': '0x121: inferred RSA/SHA256/PKCS1 v1.5 from original code and controls',
          'crypto_helpers_replaced': False, 'cases': cases}
(root / 'recovery-rsa-emulation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))

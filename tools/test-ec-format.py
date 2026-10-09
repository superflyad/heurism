"""Offline Microchip version-1 image generator controls; never accesses the EC.

Runs a pinned vendor file generator in a dedicated scratch directory using
public vendor demo keys and synthetic data. Outputs are not boot/install images.
"""
import hashlib
import itertools
import json
from pathlib import Path
import struct
import subprocess

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
vendor = root / 'microchip'
generator = vendor / 'everglades_spi_gen.exe'
expected = 'bf6a5f07d5e373173beed53215e06c32e4395f60d5cec1233835a892a8354512'
if hashlib.sha256(generator.read_bytes()).hexdigest() != expected:
    raise SystemExit('Vendor generator differs from pinned reference')
commit = 'e98e827058ac3653a2368550d27bb112112fe948'
if json.loads((vendor / 'source-tree.json').read_text())['commit'] != commit:
    raise SystemExit('Reference revision changed')
keys = json.loads((vendor / 'public-fixture-keys.json').read_text())
expected_keys = {
    'ecprivkey001.pem': 'fe5645d99950e9f55e62a857c54e49e07a8ee1c4a4538e0e38cc09043494ae55',
    'ecpubkey002_crt.pem': '81cd28c809b21ae2561f7b0058863862f2b51fafb99d62644538693a0213a851',
}
if len(keys) != 2 or {item['name'] for item in keys} != set(expected_keys):
    raise SystemExit('Unexpected demo key inventory')
for item in keys:
    if hashlib.sha256((vendor / item['name']).read_bytes()).hexdigest() != expected_keys[item['name']]:
        raise SystemExit('Public demo key fixture changed')
work = vendor / 'format-controls-v1'
work.mkdir(exist_ok=True)
payload = bytes(range(256)) * 16
(work / 'fixture.bin').write_bytes(payload)
for item in keys:
    (work / item['name']).write_bytes((vendor / item['name']).read_bytes())
cases = []
for authenticate, encrypt, sign in itertools.product([False, True], repeat=3):
    name = f'a{int(authenticate)}e{int(encrypt)}s{int(sign)}'
    config = f'''[SPI]
SPISizeMegabits = 8
Flashmap = false
[IMAGE "0"]
ImageLocation = 0x1000
SpiFreqMHz = 48
SpiReadCommand = Quad
SpiDriveStrength = 4
SpiSlewFast = false
SpiSignalControl = 0x00
FwBinFile = fixture.bin
FwOffset = 0
FwLoadAddress = 0xE0000
FwEntryAddress = 0xE0001
UseECDSA = {str(sign).lower()}
FwAuthtic = {str(authenticate).lower()}
FwEncrypt = {str(encrypt).lower()}
ECDSAPrivKeyFile = ecprivkey001.pem
ECDSAPrivKeyPassword = ECPRIVKEY001
AesGenECPubKeyFile = ecpubkey002_crt.pem
'''
    (work / (name + '.cfg')).write_text(config, encoding='ascii')
    result = subprocess.run([str(generator.resolve()), '-i', name + '.cfg', '-o', name + '.bin'],
                            cwd=work, capture_output=True, timeout=20,
                            creationflags=0x08000000)
    (work / (name + '.log')).write_bytes(result.stdout + result.stderr)
    if result.returncode != 0:
        raise SystemExit('Generator failed: ' + name)
    data = (work / (name + '.bin')).read_bytes()
    header = data[0x1000:0x10c0]
    flags = (0x40 if authenticate else 0) | (0x80 if encrypt else 0)
    if len(data) != 1048576 or header[:5] != b'PHCM\x01' or header[6] != flags:
        raise SystemExit('Unexpected image format: ' + name)
    length = struct.unpack_from('<H', header, 16)[0] * 64
    offset = struct.unpack_from('<I', header, 20)[0]
    body = data[0x1000 + offset:0x1000 + offset + length]
    header_hash = hashlib.sha256(header[:64]).digest() == header[64:96]
    if offset != 192 or length != len(payload) or header_hash == sign:
        raise SystemExit('Unexpected signature/header convention: ' + name)
    if (body == payload) == encrypt:
        raise SystemExit('Unexpected payload transform: ' + name)
    cases.append({'name': name, 'authentication_requested': authenticate,
                  'encryption_requested': encrypt, 'ecdsa_signature_requested': sign,
                  'flags_at_6': hex(header[6]), 'header_sha256_matches': header_hash,
                  'header_tail_96_128_hex': header[96:128].hex(),
                  'payload_is_plain_fixture': body == payload,
                  'payload_length': length, 'payload_offset': offset,
                  'post_payload_192_hex': data[0x1000 + offset + length:
                                               0x1000 + offset + length + 192].hex(),
                  'output_sha256': hashlib.sha256(data).hexdigest()})
report = {'scope': 'Synthetic offline generator behavior; not physical EC OTP state or write acceptance',
          'source_commit': commit, 'generator_sha256': expected,
          'public_demo_keys_only': True, 'cases': cases}
(root / 'ec-format-controls.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'cases_passed': len(cases), 'flags': sorted({c['flags_at_6'] for c in cases}),
                  'scope': report['scope']}, indent=2))

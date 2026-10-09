"""Verify original Dell PFS signed bytes against a key in the verified ROM.

Uses raw decompressed containers, never the reconstructed flat BIOS. File only;
not proof of the full hardware update flow or authority to sign our firmware.
"""
import hashlib
import json
from pathlib import Path
import re
import struct
import uuid
import zlib
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
rom = (root / 'bios16-a.bin').read_bytes()
package = (root / 'vendor/BIOS_IMG-1.35.0.rcv').read_bytes()
rom_hash = hashlib.sha256(rom).hexdigest()
package_hash = hashlib.sha256(package).hexdigest()
if rom_hash != '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c':
    raise SystemExit('ROM changed')
if package_hash != 'e649ae3fc5684a7bc7790bc3f2dca7f4235986d80af469ec60b926fc0e7fa9ed':
    raise SystemExit('Published package changed')
record_offset, key_offset = 0xf957c0, 0xf94458
pointer, length, flags = struct.unpack_from('<III', rom, record_offset)
key_guid = str(uuid.UUID(bytes_le=rom[record_offset + 12:record_offset + 28]))
if (pointer, length, flags) != (0xff000000 + key_offset, 512, 0x221):
    raise SystemExit('ROM key record differs')
key_bytes = rom[key_offset:key_offset + 512]
public = rsa.RSAPublicNumbers(int.from_bytes(key_bytes[256:], 'big'),
                             int.from_bytes(key_bytes[:256], 'big')).public_key()
pss = padding.PSS(mgf=padding.MGF1(hashes.SHA256()), salt_length=20)
sections, checks = [], []
pattern = re.compile(br'\xEE\xAA\x76\x1B\xEC\xBB\x20\xF1\xE6\x51.\x78\x9C', re.DOTALL)
for match in pattern.finditer(package):
    start = match.start()
    header = package[start - 5:start + 11]
    if len(header) != 16 or start < 5:
        raise SystemExit('Truncated compression header')
    checksum = 0
    for byte in header[:15]:
        checksum ^= byte
    compressed_size = struct.unpack_from('<I', header)[0]
    if checksum != header[15] or compressed_size > len(package) - start - 27:
        raise SystemExit('Invalid compression header/bounds')
    compressed = package[start + 11:start + 11 + compressed_size]
    decompressor = zlib.decompressobj()
    data = decompressor.decompress(compressed, 100 * 1024 * 1024)
    if not decompressor.eof or decompressor.unused_data or decompressor.unconsumed_tail:
        raise SystemExit('Compressed section did not finish within bounds')
    if data[:8] != b'PFS.HDR.' or len(data) < 32:
        raise SystemExit('Missing PFS header')
    version, payload_size = struct.unpack_from('<II', data, 8)
    if version not in (1, 2) or len(data) != payload_size + 32 or data[-8:] != b'PFS.FTR.':
        raise SystemExit('Unexpected PFS size/footer')
    section = 'Firmware' if package[start - 1] == 0xaa else 'Utilities'
    sections.append({'section': section, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    cursor, index, end = 16, 0, 16 + payload_size
    while cursor < end:
        if cursor + 56 > end:
            raise SystemExit('Truncated PFS entry')
        entry_version = struct.unpack_from('<I', data, cursor + 16)[0]
        header_size = {1: 72, 2: 88}.get(entry_version)
        if header_size is None:
            raise SystemExit('Unknown PFS entry revision')
        guid = str(uuid.UUID(bytes_le=data[cursor:cursor + 16]))
        sizes = struct.unpack_from('<IIII', data, cursor + 40)
        if cursor + header_size + sum(sizes) > end:
            raise SystemExit('PFS entry exceeds container')
        at, values = cursor + header_size, []
        for size in sizes:
            values.append(data[at:at + size])
            at += size
        index += 1
        for kind, value, signature in [('data', values[0], values[1]), ('metadata', values[2], values[3])]:
            if not signature:
                continue
            if not value or len(signature) != 256:
                raise SystemExit('Unexpected signed-entry size')
            public.verify(signature, value, pss, hashes.SHA256())
            changed = bytearray(value)
            changed[0] ^= 1
            rejected = False
            try:
                public.verify(signature, bytes(changed), pss, hashes.SHA256())
            except InvalidSignature:
                rejected = True
            if not rejected:
                raise SystemExit('Changed payload was accepted')
            checks.append({'section': section, 'entry_index': index, 'entry_guid': guid,
                           'kind': kind, 'bytes': len(value), 'sha256': hashlib.sha256(value).hexdigest(),
                           'signature_verified': True, 'changed_payload_rejected': True})
            if section == 'Firmware' and index == 1 and kind == 'data':
                (root / 'vendor/original-bios-signed-container.bin').write_bytes(value)
                (root / 'vendor/original-bios-signed-container.sig').write_bytes(signature)
        cursor = at
    if cursor != end:
        raise SystemExit('PFS parsing did not reach boundary')
if len(sections) != 2 or len(checks) != 18:
    raise SystemExit('Expected two sections and eighteen signatures')
report = {'scope': 'Original package signature verification with a public key from verified ROM; '
                   'not full recovery-policy execution or custom-firmware acceptance',
          'rom_sha256': rom_hash, 'package_sha256': package_hash,
          'key_record_offset': hex(record_offset), 'key_offset': hex(key_offset),
          'key_guid': key_guid, 'key_sha256': hashlib.sha256(key_bytes).hexdigest(),
          'scheme': 'RSA-2048 PSS SHA256, MGF1 SHA256, 20-byte salt',
          'sections': sections, 'checks': checks}
(root / 'recovery-signature-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'signatures_verified': len(checks), 'negative_controls_passed': len(checks),
                  'key_guid': key_guid, 'scope': report['scope']}, indent=2))

"""Verify and disassemble vendor BIOS Guard blocks; never execute their scripts."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import uuid
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import padding, rsa

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
container = (root / 'vendor/original-bios-signed-container.bin').read_bytes()
rom = (root / 'bios16-a.bin').read_bytes()
if hashlib.sha256(container).hexdigest() != 'fa9d307295f8e000ab5d4be9c857bb1ed12829ceae4514829f53edb2c164c236':
    raise SystemExit('Original signed container changed')
if hashlib.sha256(rom).hexdigest() != '09bc04700d0047b2317f865eafffaf77e9de500d3746e1ca0da9344d4690c46c':
    raise SystemExit('ROM changed')
decoder_path = root / 'vendor/bgscripttool/big_script_tool.py'
if hashlib.sha256(decoder_path.read_bytes()).hexdigest() != 'a50942d65c8eac05dec5b4826429affb9aaeb52b0e08c95744e2e6b25f6abf1e':
    raise SystemExit('Pinned decoder changed')
spec = importlib.util.spec_from_file_location('bgscript_decoder', decoder_path)
decoder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(decoder)
version, size = struct.unpack_from('<II', container, 8)
if container[:8] != b'PFS.HDR.' or version != 1 or size + 32 != len(container) or container[-8:] != b'PFS.FTR.':
    raise SystemExit('Container bounds/header mismatch')
cursor, end = 16, 16 + size
blocks, texts, ranges, keys = [], [], [], set()
reconstructed = bytearray(16 * 1024 * 1024)
outer_sig = (root / 'vendor/original-bios-signed-container.sig').read_bytes()
outer_key_bytes = rom[0xf94458:0xf94658]
outer_key = rsa.RSAPublicNumbers(int.from_bytes(outer_key_bytes[256:], 'big'),
                                 int.from_bytes(outer_key_bytes[:256], 'big')).public_key()
outer_padding = padding.PSS(mgf=padding.MGF1(hashes.SHA256()), salt_length=20)
outer_key.verify(outer_sig, container, outer_padding, hashes.SHA256())
while cursor < end:
    if cursor + 72 > end:
        raise SystemExit('Truncated PFS entry')
    entry_version = struct.unpack_from('<I', container, cursor + 16)[0]
    header_size = {1: 72, 2: 88}.get(entry_version)
    if header_size is None:
        raise SystemExit('Unknown PFS entry revision')
    sizes = struct.unpack_from('<IIII', container, cursor + 40)
    finish = cursor + header_size + sum(sizes)
    if finish > end:
        raise SystemExit('PFS entry exceeds container')
    fields, at = [], cursor + header_size
    for length in sizes:
        fields.append(container[at:at + length])
        at += length
    data, signature, metadata, metadata_signature = fields
    if len(data) < 48 or len(signature) not in (0, 524) or len(metadata) != 25 or metadata_signature:
        raise SystemExit('Unexpected block layout')
    major, minor, platform, attributes, script_major, script_minor, script_size, data_size, bios_svn, ec_svn, vendor = struct.unpack_from('<HH16sIHHIIIII', data)
    if (major, minor, platform, script_major, script_minor) != (2, 0, b'DellAgsX11\0\0\0\0\0\0', 2, 0) or attributes not in (0, 13):
        raise SystemExit('Unexpected BIOS Guard policy header')
    if len(data) != 48 + script_size + data_size or script_size % 8 or data_size != 65536:
        raise SystemExit('BIOS Guard size mismatch')
    if bool(signature) != bool(attributes & 1):
        raise SystemExit('Signature presence disagrees with SFAM flag')
    if signature:
        if signature[:8] != struct.pack('<II', 1, 1):
            raise SystemExit('Unknown signature header')
        modulus = int.from_bytes(signature[8:264], 'little')
        exponent = struct.unpack_from('<I', signature, 264)[0]
        public = rsa.RSAPublicNumbers(exponent, modulus).public_key()
        raw_signature = signature[268:]
        public.verify(raw_signature, data, padding.PKCS1v15(), hashes.SHA256())
    controls = []
    for name, offset in [('header', 36), ('script', 48), ('payload', 48 + script_size)]:
        changed = bytearray(data)
        changed[offset] ^= 1
        try:
            if signature:
                public.verify(raw_signature, changed, padding.PKCS1v15(), hashes.SHA256())
            else:
                changed_container = bytearray(container)
                changed_container[cursor + header_size + offset] ^= 1
                outer_key.verify(outer_sig, changed_container, outer_padding, hashes.SHA256())
        except InvalidSignature:
            controls.append(name)
        else:
            raise SystemExit('Changed signed bytes accepted')
    changed_sig = bytearray(raw_signature if signature else outer_sig)
    changed_sig[-1] ^= 1
    try:
        if signature:
            public.verify(changed_sig, data, padding.PKCS1v15(), hashes.SHA256())
        else:
            outer_key.verify(changed_sig, container, outer_padding, hashes.SHA256())
    except InvalidSignature:
        controls.append('signature')
    else:
        raise SystemExit('Changed signature accepted')
    script = data[48:48 + script_size]
    if script[8:12] != b'\x51\0\0\0' or script[24:28] != b'\x51\0\x02\0':
        raise SystemExit('Unexpected offset/length setup instructions')
    offset = struct.unpack_from('<I', script, 12)[0]
    length = struct.unpack_from('<I', script, 28)[0]
    address, unknown, meta_offset, meta_length = struct.unpack_from('<IIII', metadata)
    if offset != meta_offset or length != meta_length or length != data_size or offset + length > len(reconstructed):
        raise SystemExit('Script/metadata/image-range mismatch')
    if any(offset < old_end and old_start < offset + length for old_start, old_end in ranges):
        raise SystemExit('Overlapping payload ranges')
    ranges.append((offset, offset + length))
    reconstructed[offset:offset + length] = data[48 + script_size:]
    text = decoder.BigScript(code_bytes=script).to_string()
    if 'eraseblk F0' not in text or 'write F0 B0 I1' not in text or 'compare B0 B1 I1' not in text:
        raise SystemExit('Unexpected update-script behavior; inspect before proceeding')
    texts.append(f'Block {len(blocks)}: BIOS offset {offset:#x}, metadata address {address:#x}\n{text}\n')
    if signature:
        key_bytes = modulus.to_bytes(256, 'big') + exponent.to_bytes(256, 'big')
        keys.add(key_bytes)
    blocks.append({'index': len(blocks), 'bios_offset': hex(offset), 'metadata_address': hex(address),
                   'bytes': length, 'script_bytes': script_size, 'bios_svn': bios_svn, 'ec_svn': ec_svn,
                   'sha256': hashlib.sha256(data).hexdigest(), 'attributes': hex(attributes),
                   'block_signature_verified': bool(signature), 'outer_container_signature_verified': True,
                   'control_signature_layer': 'block' if signature else 'outer_container',
                   'rejected_changes': controls})
    cursor = finish
if cursor != end or len(blocks) != 256 or sorted(ranges) != [(i * 65536, (i + 1) * 65536) for i in range(256)] or len(keys) != 1:
    raise SystemExit('Incomplete/ambiguous BIOS coverage or signer')
flat_files = list((root / 'vendor/recovery-1.35.0-extracted/Firmware').glob('*1 System BIOS with BiosGuard*.data.bin'))
if len(flat_files) != 1 or flat_files[0].read_bytes() != reconstructed:
    raise SystemExit('Independent block reconstruction differs from extracted BIOS')
key_bytes = keys.pop()
matches = []
for index in range(11):
    record = struct.unpack_from('<I', rom, 0xf95c20 + index * 8)[0] - 0xff000000
    pointer, key_size, flags = struct.unpack_from('<III', rom, record)
    offset = pointer - 0xff000000
    if key_size == 512 and rom[offset:offset + key_size] == key_bytes:
        matches.append({'index': index, 'rom_offset': hex(offset), 'registry_flags': hex(flags),
                        'guid': str(uuid.UUID(bytes_le=rom[record + 12:record + 28])),
                        'inside_signed_ibb': 0xf70000 <= offset and offset + key_size <= 0x1000000})
if len(matches) != 1:
    raise SystemExit('Signer does not match exactly one installed recovery registry key')
report = {'scope': 'Offline signature verification and script disassembly. Scripts never executed; '
                   'no hardware flash cycle, full BIOS Guard policy validation, or owner-key installation proof.',
          'scheme': 'RSA-2048 PKCS1v1.5 SHA-256', 'block_count': len(blocks),
          'signed_block_count': sum(b['block_signature_verified'] for b in blocks),
          'blocks_without_inner_signature': [b['bios_offset'] for b in blocks if not b['block_signature_verified']],
          'rejected_mutations': sum(len(b['rejected_changes']) for b in blocks),
          'reconstructed_sha256': hashlib.sha256(reconstructed).hexdigest(),
          'key_sha256': hashlib.sha256(key_bytes).hexdigest(), 'rom_registry_matches': matches,
          'blocks': blocks}
(root / 'bios-guard-block-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
(root / 'bios-guard-scripts.txt').write_text('\n'.join(texts), encoding='utf-8')
print(json.dumps({k: v for k, v in report.items() if k != 'blocks'}, indent=2))

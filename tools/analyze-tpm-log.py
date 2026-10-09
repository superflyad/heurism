"""Bounds-check a TCG2 boot log and compare firmware events with verified ROM."""
import hashlib
import json
from pathlib import Path
import struct
import sys

root = Path(__file__).resolve().parents[1] / 'artifacts/firmware'
algorithms = {4: ('sha1', 20), 11: ('sha256', 32), 12: ('sha384', 48),
              13: ('sha512', 64), 18: ('sm3', 32)}

class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def take(self, length):
        if length < 0 or self.pos + length > len(self.data):
            raise ValueError('Truncated event at ' + hex(self.pos))
        value = self.data[self.pos:self.pos + length]
        self.pos += length
        return value

    def number(self, format):
        return struct.unpack(format, self.take(struct.calcsize(format)))[0]

def parse(data):
    reader = Reader(data)
    pcr, event_type = struct.unpack('<II', reader.take(8))
    reader.take(20)
    spec = Reader(reader.take(reader.number('<I')))
    if pcr != 0 or event_type != 3 or spec.take(16) != b'Spec ID Event03\0':
        raise ValueError('Expected TCG2 Spec ID event')
    spec.take(8)  # Platform class and specification version fields.
    count = spec.number('<I')
    if not 1 <= count <= 16:
        raise ValueError('Invalid algorithm count')
    sizes = {}
    for _ in range(count):
        alg, size = struct.unpack('<HH', spec.take(4))
        if alg not in algorithms or algorithms[alg][1] != size or alg in sizes:
            raise ValueError('Unsupported or inconsistent algorithm')
        sizes[alg] = size
    spec.take(spec.number('<B'))
    if spec.pos != len(spec.data):
        raise ValueError('Unexpected Spec ID trailing bytes')
    events = []
    while reader.pos < len(data):
        start = reader.pos
        pcr, event_type, count = struct.unpack('<III', reader.take(12))
        if pcr > 23 or count > len(sizes) or count == 0:
            raise ValueError('Invalid PCR/digest count')
        digests = {}
        for _ in range(count):
            alg = reader.number('<H')
            if alg not in sizes or alg in digests:
                raise ValueError('Invalid event algorithm')
            digests[algorithms[alg][0]] = reader.take(sizes[alg]).hex()
        payload = reader.take(reader.number('<I'))
        event = {'offset': start, 'bytes': reader.pos - start, 'pcr': pcr,
                 'event_type': hex(event_type), 'digests': digests,
                 'event_data_bytes': len(payload)}
        if event_type == 0x80000008 and len(payload) == 16:
            base, length = struct.unpack('<QQ', payload)
            event['blob'] = {'base': hex(base), 'bytes': length}
        elif event_type == 0x8000000a and payload:
            description_size = payload[0]
            if len(payload) != 1 + description_size + 16:
                raise ValueError('Invalid firmware blob2 payload')
            base, length = struct.unpack('<QQ', payload[1 + description_size:])
            event['blob'] = {'base': hex(base), 'bytes': length}
        events.append(event)
    return sizes, events

def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else next(root.glob('tpm-log-*.bin'))
    data = path.read_bytes()
    metadata = json.loads(path.with_suffix('.json').read_text())
    if len(data) != metadata['bytes'] or hashlib.sha256(data).hexdigest() != metadata['sha256']:
        raise ValueError('TPM log metadata mismatch')
    bios = (root / 'bios16-a.bin').read_bytes()
    analysis = json.loads((root / 'uefi-volume-analysis.json').read_text())
    if hashlib.sha256(bios).hexdigest() != analysis['bios_sha256']:
        raise ValueError('BIOS metadata mismatch')
    sizes, events = parse(data)
    # TPM2_PCR_Read response has big-endian TPML_PCR_SELECTION and TPML_DIGEST.
    response = path.with_suffix('.pcr-read.bin').read_bytes()
    if hashlib.sha256(response).hexdigest() != metadata['pcr_read_response_sha256']:
        raise ValueError('PCR read metadata mismatch')
    pcr_reader = Reader(response)
    tag = pcr_reader.number('>H')
    total = pcr_reader.number('>I')
    code = pcr_reader.number('>I')
    if tag != 0x8001 or total != len(response) or code != 0:
        raise ValueError('Invalid TPM response')
    update_counter = pcr_reader.number('>I')
    if pcr_reader.number('>I') != 1 or pcr_reader.number('>H') != 11:
        raise ValueError('Unexpected PCR selection')
    if pcr_reader.take(pcr_reader.number('>B')) != b'\x01\x00\x00':
        raise ValueError('Unexpected PCR bitmap')
    if pcr_reader.number('>I') != 1 or pcr_reader.number('>H') != 32:
        raise ValueError('Unexpected digest count/size')
    actual_pcr = pcr_reader.take(32)
    if pcr_reader.pos != len(response):
        raise ValueError('PCR response trailing bytes')
    locality = 0
    for event in events:
        if event['pcr'] == 0 and event['event_type'] == '0x3':
            payload = data[event['offset'] + event['bytes'] - event['event_data_bytes']:
                           event['offset'] + event['bytes']]
            if payload.startswith(b'StartupLocality\0') and len(payload) == 17:
                locality = payload[-1]
    if locality not in (0, 3):
        raise ValueError('Unexpected startup locality')
    replay = b'\0' * 31 + bytes([locality])
    for event in events:
        if event['pcr'] == 0 and event['event_type'] != '0x3':
            replay = hashlib.sha256(replay + bytes.fromhex(event['digests']['sha256'])).digest()
    if replay != actual_pcr:
        raise ValueError('Event log replay differs from physical TPM PCR0')
    for event in events:
        matches = []
        for volume in analysis['volumes']:
            offset, length = int(volume['bios_offset'], 16), volume['bytes']
            matching = [alg for alg, digest in event['digests'].items()
                        if hashlib.new(alg, bios[offset:offset + length]).hexdigest() == digest]
            if matching:
                matches.append({'volume_offset': hex(offset), 'bytes': length,
                                'algorithms': matching})
        if matches:
            event['rom_volume_digest_matches'] = matches
    result = {'scope': 'Reported actual boot log, offline digest comparison; '
                       'measurement alone does not prove enforcement',
              'boot_id': metadata['boot_id'], 'log_sha256': metadata['sha256'],
              'bios_sha256': analysis['bios_sha256'],
              'algorithms': {algorithms[alg][0]: size for alg, size in sizes.items()},
              'pcr0': {'startup_locality': locality, 'sha256': actual_pcr.hex(),
                       'replayed_log_matches': True, 'update_counter': update_counter},
              'event_count': len(events), 'events': events}
    (root / 'tpm-log-analysis.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps({key: value for key, value in result.items() if key != 'events'}, indent=2))
    print(json.dumps({'firmware_events_matching_rom': [
        {'event_offset': event['offset'], 'event_type': event['event_type'],
         'volume_offsets': [match['volume_offset'] for match in event['rom_volume_digest_matches']]}
        for event in events if event.get('rom_volume_digest_matches')]}, indent=2))

if __name__ == '__main__':
    main()

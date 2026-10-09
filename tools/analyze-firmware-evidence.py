"""Decode captured CSME 15 status and validate board evidence, entirely offline."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path, help='Completed evidence snapshot directory')
args = parser.parse_args()
evidence = json.loads((args.directory / 'evidence.json').read_text())
for name, entry in evidence['files'].items():
    data = (args.directory / entry['local_path']).read_bytes()
    if len(data) != entry['bytes'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
        raise SystemExit('Evidence integrity mismatch: ' + name)
audit = json.loads(evidence['commands']['companion-firmware-audit']['stdout'])
version = audit['mei_firmware_version']
if not version or not version.splitlines()[0].startswith('0:15.'):
    raise SystemExit('This decoder is restricted to the observed CSME 15 platform')
config = args.directory / 'sys/bus/pci/devices/0000_00_16.0/config'
data = config.read_bytes()
if len(data) < 0x70 or data[:4] != bytes.fromhex('8680e0a0'):
    raise SystemExit('Unexpected CSME PCI identity or short configuration capture')
registers = {f'HFSTS{i}': struct.unpack_from('<I', data, offset)[0]
             for i, offset in enumerate([0x40, 0x48, 0x60, 0x64, 0x68, 0x6c], 1)}
h1, h3, h6 = (registers[key] for key in ['HFSTS1', 'HFSTS3', 'HFSTS6'])
support = json.loads((args.directory.parent / 'coreboot-support.json').read_text())
commit = support['coreboot_commit']
source = 'https://github.com/coreboot/coreboot/blob/' + commit + '/'
tables = {}
for name, entry in evidence['files'].items():
    if not name.startswith('/sys/firmware/acpi/tables/'):
        continue
    raw = (args.directory / entry['local_path']).read_bytes()
    if len(raw) < 8:
        raise SystemExit('Short ACPI table: ' + name)
    declared = struct.unpack_from('<I', raw, 4)[0]
    signature = raw[:4].decode('ascii', errors='replace')
    length_ok = declared == len(raw)
    checksum_ok = None if signature == 'FACS' else sum(raw) % 256 == 0
    if not length_ok or checksum_ok is False:
        raise SystemExit('Invalid ACPI table: ' + name)
    tables[name.rsplit('/', 1)[-1]] = {'signature': signature, 'bytes': len(raw),
                                       'length_ok': length_ok, 'checksum_ok': checksum_ok}
report = {
    'snapshot_utc': evidence['collected_utc'], 'boot_id': audit['boot_id'],
    'platform': audit['platform'], 'csme_version': version,
    'raw_status_registers': {key: f'0x{value:08x}' for key, value in registers.items()},
    'csme15_decode': {
        'manufacturing_mode_bit': bool(h1 & (1 << 4)),
        'fpf_soc_lock': bool(h6 & (1 << 30)),
        'manufacturing_mode_per_coreboot': bool(h1 & (1 << 4)) or not bool(h6 & (1 << 30)),
        'fpf_committed_per_coreboot': bool(h6 & (1 << 30)),
        'fw_sku': (h3 >> 4) & 7,
        'fw_sku_name': {2: 'consumer', 3: 'corporate', 5: 'lite'}.get((h3 >> 4) & 7, 'unknown'),
        'cpu_debug_disable_bit': bool(h6 & 2)},
    'boot_guard': audit['boot_guard_msr_0x13a'],
    'upstream_support': {'commit': commit, 'target_name_matches': support['target_name_matches'],
                         'tigerlake_soc_present': support['tigerlake_soc_present']},
    'acpi_tables': tables,
    'sources': [source + 'src/soc/intel/common/block/include/intelblocks/me_15.h',
                source + 'src/soc/intel/common/block/cse/cse_spec.c',
                source + 'src/soc/intel/common/block/include/intelblocks/cse.h',
                source + 'src/security/intel/cbnt/logging.c'],
    'limitations': ['No flash image, protected-region map or OEM key hash acquired.',
        'Committed-fuse status is not a dump of every fuse or enforcement-policy field.',
        'A clear CPU debug-disable bit does not establish usable debug access or a Boot Guard bypass.',
        'No independent BIOS display/power channel or physical flash recovery established.',
        'No firmware written; firmware replacement requirement remains unmet.']}
destination = args.directory / 'analysis.json'
destination.write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'report': str(destination), 'csme15_decode': report['csme15_decode'],
                  'verified_files': len(evidence['files']), 'valid_acpi_tables': len(tables)}, indent=2))

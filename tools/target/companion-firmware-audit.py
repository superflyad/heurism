#!/usr/bin/python3
"""Read firmware access facts without probing flash chips or writing registers (Python 3)."""
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import struct
import subprocess

def read(path):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return None
subprocess.run(['modprobe', 'msr'], capture_output=True)
registers = {}
for device in sorted(Path('/dev/cpu').glob('*/msr')):
    try:
        with device.open('rb', buffering=0) as stream:
            stream.seek(0x13a)
            raw = struct.unpack('<Q', stream.read(8))[0]
        registers[device.parent.name] = {
            'raw': f'0x{raw:016x}', 'measured_boot': bool(raw & (1 << 5)),
            'verified_boot': bool(raw & (1 << 6)), 'boot_guard_capable': bool(raw & (1 << 32))}
    except OSError as error:
        registers[device.parent.name] = {'error': str(error)}
base = Path('/sys/class/firmware-attributes/dell-wmi-sysman')
attributes = {p.name: {field: read(p / field) for field in
             ['display_name', 'type', 'current_value', 'default_value', 'possible_values']}
             for p in sorted((base / 'attributes').iterdir())
             if p.is_dir() and (p / 'current_value').exists()}
report = {
    'utc': datetime.now(timezone.utc).isoformat(),
    'boot_id': read('/proc/sys/kernel/random/boot_id'),
    'platform': {p: read('/sys/class/dmi/id/' + p) for p in
                 ['sys_vendor', 'product_name', 'board_name', 'bios_version', 'bios_date']},
    'boot_guard_msr_0x13a': registers,
    'bios_attributes': attributes,
    'bios_pending_reboot': read(base / 'attributes/pending_reboot'),
    'bios_passwords_enabled': {p.name: read(p / 'is_enabled')
                              for p in (base / 'authentication').iterdir() if p.is_dir()},
    'mei_clients': sorted(p.name for p in Path('/sys/bus/mei/devices').iterdir()),
    'mei_firmware_version': read('/sys/class/mei/mei0/fw_ver'),
    'flash_mtd_devices': sorted(str(p) for p in Path('/sys/class/mtd').glob('mtd*')),
    'kvm_available': Path('/dev/kvm').exists(),
    'sources': ['https://github.com/coreboot/coreboot/blob/main/src/security/intel/cbnt/logging.c'],
    'limitations': 'MSR flags report boot policy, not a complete fuse/flash-region audit. No firmware writes.'}
destination = Path('/var/lib/companion/firmware-audit.json')
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(json.dumps(report, indent=2))
os.chmod(destination, 0o600)
print(json.dumps(report, indent=2))

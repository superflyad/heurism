#!/usr/bin/python3
"""Inspect Intel 8086:a0a4 SPI protection registers using read-only mappings.

Does not enable PCI decoding, bind a driver, issue a flash cycle, or write MMIO.
Register layout: Linux v6.18 drivers/spi/spi-intel.c and spi-intel-pci.c.
"""
from datetime import datetime, timezone
import json
import mmap
import os
from pathlib import Path
import struct

base = Path('/sys/bus/pci/devices/0000:00:1f.5')
if os.getuid() != 0 or base.joinpath('vendor').read_text().strip() != '0x8086' or base.joinpath('device').read_text().strip() != '0xa0a4':
    raise SystemExit('Requires root and the known Tiger Lake SPI controller')
with (base / 'config').open('rb') as stream:
    config = stream.read(256)
command = struct.unpack_from('<H', config, 4)[0]
bcr = struct.unpack_from('<I', config, 0xdc)[0]
resource = (base / 'resource').read_text().splitlines()[0].split()
start, end, flags = (int(field, 16) for field in resource)
report = {'utc': datetime.now(timezone.utc).isoformat(),
    'boot_id': Path('/proc/sys/kernel/random/boot_id').read_text().strip(),
    'pci_command': f'0x{command:04x}', 'bios_control_raw': f'0x{bcr:08x}',
    'write_protect_disable_bit': bool(bcr & 1),
    'bios_write_controls': {'write_protect_disable': bool(bcr & 1),
        'lock_enable': bool(bcr & (1 << 1)),
        'smm_required_for_bios_write': bool(bcr & (1 << 5)),
        'boot_bios_destination': 'eSPI' if bcr & (1 << 6) else 'SPI',
        'interface_lock_down': bool(bcr & (1 << 7)),
        'async_smi_on_blocked_bios_write': bool(bcr & (1 << 11)),
        'interpretation': 'WPD=1 and InSMM.STS=1 required if EISS is set; '
                          'LE locks EISS until platform reset'},
    'bar0': {'start': hex(start), 'end': hex(end), 'flags': hex(flags)},
    'driver': str((base / 'driver').resolve()) if (base / 'driver').exists() else None,
    'sources': ['https://github.com/torvalds/linux/blob/v6.18/drivers/spi/spi-intel.c',
                'https://github.com/torvalds/linux/blob/v6.18/drivers/spi/spi-intel-pci.c',
                'https://cdrdv2-public.intel.com/631120/631120-002.pdf#page=320'],
    'scope': 'Read-only PCI config and status/protection MMIO; no flash transactions'}
if not command & 2:
    report['mmio_error'] = 'PCI memory decoding disabled; left unchanged'
elif not start or end - start + 1 != 4096:
    report['mmio_error'] = 'Unexpected BAR; not mapped'
else:
    offsets = {'BFPREG': 0, 'HSFSTS_CTL': 4, 'DLOCK': 0x0c, 'FRACC': 0x50}
    offsets.update({f'FREG{i}': 0x54 + i*4 for i in range(6)})
    offsets.update({f'PR{i}': 0x84 + i*4 for i in range(5)})
    try:
        with (base / 'resource0').open('rb', buffering=0) as stream:
            with mmap.mmap(stream.fileno(), 4096, flags=mmap.MAP_SHARED, prot=mmap.PROT_READ) as registers:
                values = {name: struct.unpack('<I', registers[offset:offset+4])[0]
                          for name, offset in offsets.items()}
        if values['HSFSTS_CTL'] == 0xffffffff:
            raise ValueError('Unresponsive MMIO mapping')
        report['registers'] = {name: f'0x{value:08x}' for name, value in values.items()}
        status = values['HSFSTS_CTL']
        report['status'] = {'descriptor_valid': bool(status & (1 << 14)),
                            'configuration_locked': bool(status & (1 << 15)),
                            'descriptor_override_strap_active': not bool(status & (1 << 13)),
                            'write_status_disabled': bool(status & (1 << 11)),
                            'cycle_in_progress': bool(status & (1 << 5))}
        report['regions'] = []
        for i in range(6):
            value = values[f'FREG{i}']
            begin = (value & 0x7fff) << 12
            limit = ((value >> 16) & 0x7fff) << 12 | 0xfff
            report['regions'].append({'index': i, 'start': hex(begin), 'end': hex(limit),
                                       'enabled': begin <= limit})
        report['protected_ranges'] = []
        for i in range(5):
            value = values[f'PR{i}']
            report['protected_ranges'].append({'index': i,
                'start': hex((value & 0x7fff) << 12),
                'end': hex(((value >> 16) & 0x7fff) << 12 | 0xfff),
                'read_protected': bool(value & (1 << 15)),
                'write_protected': bool(value & (1 << 31))})
    except (OSError, ValueError) as error:
        report['mmio_error'] = str(error)
print(json.dumps(report, indent=2))

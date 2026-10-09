"""Verify known unsafe selections are rejected without contacting the Dell."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from boot_safety import validate_command
for command in ['efibootmgr -n 0002; reboot','efibootmgr -n0001','efibootmgr --bootnext=0003',
                'efibootmgr -o 0005,0000,0001,0003','efibootmgr -r -o 0000,0001; efibootmgr -n 0002']:
    try:validate_command(command)
    except ValueError:pass
    else:raise AssertionError(command)
for command in ['id; efibootmgr; efibootmgr -r','efibootmgr -r -o 0000,0001; efibootmgr -o 0005,0000',
                'efibootmgr --driver --bootorder=0000,0001','efibootmgr -n 0006']:
    validate_command(command)
print('PASS: NIC BootNext/order blocked; inspection, SSD recovery and driver restoration allowed')

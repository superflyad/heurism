"""Verify known unsafe selections are rejected without contacting the Dell."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from boot_safety import validate_command, validate_remote_power
for command in ['efibootmgr -n 0002; reboot','efibootmgr -n0001','efibootmgr --bootnext=0003',
                'efibootmgr -o 0005,0000,0001,0003','efibootmgr -r -o 0000,0001; efibootmgr -n 0002']:
    try:validate_command(command)
    except ValueError:pass
    else:raise AssertionError(command)
for command in ['id; efibootmgr; efibootmgr -r','efibootmgr -r -o 0000,0001; efibootmgr -o 0005,0000',
                'efibootmgr --driver --bootorder=0000,0001','efibootmgr -n 0006']:
    validate_command(command)
print('PASS: NIC BootNext/order blocked; inspection, SSD recovery and driver restoration allowed')

for command in ['heurismctl power "{\\"operation\\":\\"reboot\\",\\"confirm\\":true}"',
                '/sbin/reboot', 'poweroff', 'shutdown -h now',
                'companion-next-boot rescue --reboot']:
    try:validate_remote_power(command)
    except ValueError:pass
    else:raise AssertionError(command)
    validate_remote_power(command, local_recovery_ready=True)
for command in ['heurismctl status', 'heurismctl power-check',
                'cat /proc/sys/kernel/random/boot_id']:
    validate_remote_power(command)
print('PASS: remote power requires a local recovery assertion')

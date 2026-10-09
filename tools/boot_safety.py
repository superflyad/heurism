"""Operator-error guards for physical Dell commands; not a shell sandbox."""
import re

def validate_remote_power(command, local_recovery_ready=False):
    """Require a local recovery assertion for a remote Dell power operation."""
    power = re.search(
        r'\b(?:reboot|poweroff|shutdown|halt)\b|'
        r'\b(?:heurismctl|companionctl)\s+power(?=\s|$|[;&|])|'
        r'\b(?:heurism|companion)-next-boot\b[^\n;&|]*--reboot\b',
        command, re.I)
    if power and not local_recovery_ready:
        raise ValueError('Remote Dell power action blocked: an earlier checked reboot '
                         'did not return to SSH. Use --local-recovery-ready only while '
                         'a person can inspect the Dell and restore it physically.')

def validate_command(command):
    if not re.search(r'\befibootmgr\b', command):
        return
    # Firmware added a duplicate NIC entry 0001 on recovery. Match all current
    # NIC entries anywhere in BootOrder; DriverOrder uses a separate namespace.
    # This guard is not a general shell parser or security boundary.
    for part in re.split(r'[;\n&|]',command):
        if not re.search(r'\befibootmgr\b',part):continue
        next_entry=re.search(r'(?:--bootnext(?:\s+|=)|(?<!\w)-n\s*)(?:0x)?0*([123])\b',part,re.I)
        order=re.search(r'(?:--bootorder(?:\s+|=)|(?<!\w)-o\s*)([0-9a-fA-F,]+)',part)
        driver=bool(re.search(r'(?:--driver\b|(?<!\w)-r\b)',part))
        unsafe_order=order and not driver and any(int(value,16) in [1,2,3] for value in order[1].split(',') if value)
        if next_entry or unsafe_order:
            raise ValueError('Native Dell PXE boot is blocked: failed PXE stopped at SupportAssist without restoring SSH. Keep SSD startup and use a bounded Companion loader experiment.')

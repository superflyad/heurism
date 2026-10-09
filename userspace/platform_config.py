"""Explicit platform selection. Existing installations keep the Dell policy."""
import json
from pathlib import Path
import stat

CONFIG = Path('/etc/companion/platform.json')
VENDOR = Path('/sys/class/dmi/id/sys_vendor')
MODEL = Path('/sys/class/dmi/id/product_name')
MANIFEST = Path('/etc/companion/vm-protected.sha256')
BOOT_ID = Path('/proc/sys/kernel/random/boot_id')
HEALTH_ID = Path('/var/lib/companion/healthy-boot-id')
EFI = Path('/sys/firmware/efi')


def profile():
    try:
        info = CONFIG.lstat()
    except FileNotFoundError:
        return 'dell'
    if not stat.S_ISREG(info.st_mode) or info.st_uid != 0 or info.st_mode & 0o022:
        raise ValueError('Platform configuration must be a protected root-owned file')
    value = json.loads(CONFIG.read_text())
    if value != {'platform': 'hyperv-dev'}:
        raise ValueError('Unknown platform configuration')
    vendor = VENDOR.read_text().strip()
    model = MODEL.read_text().strip()
    if vendor != 'Microsoft Corporation' or model != 'Virtual Machine':
        raise ValueError('Hyper-V profile requires the actual virtual platform')
    return 'hyperv-dev'


def verify_vm_boot(run):
    manifest = MANIFEST
    info = manifest.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_uid != 0 or info.st_mode & 0o022:
        raise ValueError('VM boot manifest is not protected')
    run(['sha256sum', '-c', str(manifest)])
    boot = BOOT_ID.read_text().strip()
    if HEALTH_ID.read_text().strip() != boot:
        raise ValueError('VM management has not confirmed this boot')
    run(['rc-service', 'sshd', 'status'])
    run(['rc-service', 'companion-watch', 'status'])
    if not EFI.is_dir():
        raise ValueError('Expected a UEFI VM boot')
    if 'BootNext:' in run(['efibootmgr']):
        raise ValueError('One-time VM firmware selection needs review')

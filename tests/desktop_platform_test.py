"""Root-owned platform/boot gate fixtures, with real checksum verification."""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, '/opt/companion/desktop')
import platform_config as platform


class PlatformTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = Path(self.temp.name)
        self.patches = []
        for name in ('CONFIG', 'VENDOR', 'MODEL', 'MANIFEST', 'BOOT_ID', 'HEALTH_ID', 'EFI'):
            p = patch.object(platform, name, self.base/name)
            p.start()
            self.patches.append(p)
        platform.VENDOR.write_text('Microsoft Corporation')
        platform.MODEL.write_text('Virtual Machine')
        platform.BOOT_ID.write_text('fixture-boot')
        platform.HEALTH_ID.write_text('fixture-boot')
        platform.EFI.mkdir()
        self.boot_file = self.base/'loader'
        self.boot_file.write_text('public fixture loader')
        platform.MANIFEST.write_text(hashlib.sha256(self.boot_file.read_bytes()).hexdigest()+'  '+str(self.boot_file)+'\n')
        self.calls = []

    def tearDown(self):
        for p in reversed(self.patches):
            p.stop()
        self.temp.cleanup()

    def fake_run(self, argv):
        self.calls.append(argv)
        if argv[0] == 'sha256sum':
            result = subprocess.run(argv, capture_output=True, text=True)
            if result.returncode:
                raise ValueError('Checksum rejected fixture')
            return result.stdout
        return ''

    def test_existing_system_defaults_to_dell(self):
        self.assertEqual(platform.profile(), 'dell')

    def test_vm_profile_requires_real_platform(self):
        platform.CONFIG.write_text('{"platform":"hyperv-dev"}')
        self.assertEqual(platform.profile(), 'hyperv-dev')
        platform.VENDOR.write_text('Dell Inc.')
        with self.assertRaises(ValueError):
            platform.profile()

    def test_unsafe_config_rejected(self):
        platform.CONFIG.write_text('{"platform":"hyperv-dev"}')
        platform.CONFIG.chmod(0o666)
        with self.assertRaises(ValueError):
            platform.profile()

    def test_unknown_profile_and_symlink_rejected(self):
        platform.CONFIG.write_text('{"platform":"anything"}')
        with self.assertRaises(ValueError):
            platform.profile()
        platform.CONFIG.unlink()
        platform.CONFIG.symlink_to(platform.VENDOR)
        with self.assertRaises(ValueError):
            platform.profile()

    def test_vm_boot_has_no_firmware_writes(self):
        platform.verify_vm_boot(self.fake_run)
        self.assertFalse(any('-o' in argv or '--bootnext' in argv for argv in self.calls))

    def test_corrupt_boot_file_rejected(self):
        self.boot_file.write_text('changed')
        with self.assertRaises(ValueError):
            platform.verify_vm_boot(self.fake_run)

    def test_stale_health_rejected(self):
        platform.HEALTH_ID.write_text('old-boot')
        with self.assertRaises(ValueError):
            platform.verify_vm_boot(self.fake_run)

    def test_bootnext_rejected(self):
        def run(argv):
            return 'BootNext: 0001\n' if argv == ['efibootmgr'] else self.fake_run(argv)
        with self.assertRaises(ValueError):
            platform.verify_vm_boot(run)


if __name__ == '__main__':
    unittest.main()

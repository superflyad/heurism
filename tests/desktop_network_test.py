"""Wireless transaction failures without touching real interfaces or credentials."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch, Mock

source = Path(__file__).resolve().parent
sys.path.insert(0, str(source if (source/'system_actions.py').exists() else Path('/opt/companion/desktop')))
import system_actions as actions


class WirelessTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = Path(self.temp.name)
        self.path = self.base/'wifi.conf'
        self.calls = []
        self.patches = [patch.object(actions, 'WIFI_CONFIG', self.path),
                        patch.object(actions.platform_config, 'profile', return_value='dell'),
                        patch.object(actions, 'RUNTIME', self.base),
                        patch.object(actions, 'wireless', return_value='wlan-fixture'),
                        patch.object(actions, 'managed_wireless', return_value=None),
                        patch.object(actions, 'wireless_dhcp', return_value=None),
                        patch.object(actions, 'run', side_effect=self.fake_run),
                        patch.object(actions.subprocess, 'Popen', return_value=Mock())]
        for item in self.patches:
            item.start()
        self.value = {'ssid': 'Fixture SSID', 'password': 'public-test-fixture'}

    def tearDown(self):
        for item in reversed(self.patches):
            item.stop()
        self.temp.cleanup()

    def fake_run(self, argv, **kwargs):
        self.calls.append((argv, kwargs))
        if argv[0] == 'wpa_passphrase':
            return 'network={\n ssid="Fixture SSID"\n #psk="public-test-fixture"\n psk='+'0'*64+'\n}\n'
        if argv[0] == 'wpa_cli':
            return 'OK\n'
        return ''

    def test_new_wireless_private_config_no_password_in_argv_or_reply(self):
        reply = actions.connect_wifi(self.value)
        self.assertEqual(reply['interface'], 'wlan-fixture')
        self.assertNotIn(self.value['password'], self.path.read_text())
        self.assertNotIn(self.value['password'], json.dumps(reply))
        self.assertEqual(self.path.stat().st_mode & 0o777, 0o600)
        for argv, kwargs in self.calls:
            self.assertNotIn(self.value['password'], argv)
            self.assertNotIn('eth0', argv)
        self.assertEqual(self.calls[0][1]['input'], self.value['password']+'\n')

    def test_existing_owned_daemons_reconfigure_and_renew_without_duplicates(self):
        with patch.object(actions, 'managed_wireless', return_value=123), \
             patch.object(actions, 'wireless_dhcp', return_value=456), patch.object(actions.os, 'kill') as kill:
            actions.connect_wifi(self.value)
            kill.assert_called_once_with(456, actions.signal.SIGUSR1)
            actions.subprocess.Popen.assert_not_called()
        self.assertFalse(any(argv[0] == 'wpa_supplicant' for argv, _ in self.calls))

    def test_start_failure_restores_previous_private_file(self):
        self.path.write_text('old fixture configuration')
        def fail(argv, **kwargs):
            if argv[0] == 'wpa_supplicant':
                raise subprocess.TimeoutExpired(argv, 5)
            return self.fake_run(argv, **kwargs)
        with patch.object(actions, 'run', side_effect=fail):
            with self.assertRaises(ValueError):
                actions.connect_wifi(self.value)
        self.assertEqual(self.path.read_text(), 'old fixture configuration')
        self.assertEqual(self.path.stat().st_mode & 0o777, 0o600)
        actions.subprocess.Popen.assert_not_called()

    def test_other_owner_rejected_before_config_change(self):
        with patch.object(actions, 'managed_wireless', side_effect=ValueError('Other owner')):
            with self.assertRaises(ValueError):
                actions.connect_wifi(self.value)
        self.assertFalse(self.path.exists())
        self.assertEqual(self.calls, [])

    def test_late_failure_reloads_restored_config(self):
        self.path.write_text('old fixture configuration')
        with patch.object(actions, 'managed_wireless', return_value=123), \
             patch.object(actions.subprocess, 'Popen', side_effect=OSError('Fixture failure')):
            with self.assertRaises(ValueError):
                actions.connect_wifi(self.value)
        self.assertEqual(self.path.read_text(), 'old fixture configuration')
        self.assertEqual(sum(argv[0] == 'wpa_cli' for argv, _ in self.calls), 2)

    def boot_run(self, order='0005,0000', extra='', driver='0000,0001', next_boot=''):
        def fake(argv, **kwargs):
            self.calls.append((argv, kwargs))
            if argv == ['efibootmgr', '--driver']:
                return 'DriverOrder: '+driver+'\n'
            if argv == ['efibootmgr', '-v']:
                return ('BootOrder: '+order+'\n'+next_boot+
                        'Boot0005* Companion Management\tHD()/\\EFI\\alpine\\grubx64.efi\n'
                        'Boot0000* SSD Fallback\tHD()/\\EFI\\Boot\\BootX64.efi\n'+extra)
            if argv == ['efibootmgr']:
                return 'BootOrder: 0005,0000\n'
            return ''
        return fake

    def test_power_boot_default_order_needs_no_firmware_write(self):
        with patch.object(actions, 'run', side_effect=self.boot_run()):
            actions.verify_boot()
        self.assertFalse(any('-o' in argv for argv, _ in self.calls))

    def test_power_boot_restores_only_verified_appended_network_entries(self):
        extra = 'Boot00AB* NIC\tPciRoot()/MAC(abcdef)/IPv4(){auto_created_boot_option}\n'
        with patch.object(actions, 'run', side_effect=self.boot_run('0005,0000,00AB', extra)):
            actions.verify_boot()
        self.assertEqual([argv for argv, _ in self.calls if '-o' in argv], [['efibootmgr', '-o', '0005,0000']])
        self.assertTrue((self.base/'power-boot-before.txt').exists())

    def test_power_boot_rejects_unexpected_orders_or_pending_bootnext(self):
        for kwargs in ({'order':'00AB,0005,0000'}, {'order':'0005,0000,00AB'},
                       {'driver':'0001,0000'}, {'next_boot':'BootNext: 00AB\n'}):
            self.calls.clear()
            with patch.object(actions, 'run', side_effect=self.boot_run(**kwargs)):
                with self.assertRaises(ValueError):
                    actions.verify_boot()
            self.assertFalse(any('-o' in argv for argv, _ in self.calls))


if __name__ == '__main__':
    unittest.main()

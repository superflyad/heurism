"""Linux-side control boundary tests; no physical hardware writes."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import sys

source = Path(__file__).resolve().parent / 'control.py'
if not source.exists():
    source = Path(__file__).resolve().parents[1] / 'userspace/control.py'
if not source.exists():
    source = Path('/opt/companion/desktop/control.py')
spec = importlib.util.spec_from_file_location('control', source)
sys.path.insert(0, str(source.parent))
control = importlib.util.module_from_spec(spec)
spec.loader.exec_module(control)


class ControlTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = Path(self.temp.name)
        light = self.base / 'sys/class/backlight/fixture'
        light.mkdir(parents=True)
        (light / 'max_brightness').write_text('1000')
        (light / 'brightness').write_text('500')
        self.light = light
        self.api = control.Control(self.base/'sys', self.base/'proc', self.base/'state')

    def tearDown(self):
        self.temp.cleanup()

    def test_rejects_unrecognized_operations(self):
        for action in ('reboot', 'command', '../../brightness', None):
            with self.assertRaises(ValueError):
                self.api.dispatch({'version': 1, 'action': action, 'value': 'rm'})

    def test_system_actions_reject_before_side_effects(self):
        for action, value in [('network-connect', {'ssid':'test', 'password':'short'}),
                              ('network-connect', {'ssid':'a\nnetwork', 'password':'not-a-real-secret'}),
                              ('bios-set', {'name':'SecureBoot', 'value':'Disabled'}),
                              ('admin-console', 'arbitrary-command'),
                              ('power', {'operation':'reboot','confirm':False})]:
            with self.assertRaises(ValueError):
                self.api.dispatch({'version':1, 'action':action, 'value':value})

    def test_rejects_malformed_frames(self):
        for req in (None, [], {}, {'version': 2, 'action': 'status'}):
            with self.assertRaises(ValueError):
                self.api.dispatch(req)

    def test_brightness_bounds_no_writes_on_rejection(self):
        for value in (True, 0, 4, 101, '50', 1.5, None):
            with self.assertRaises(ValueError):
                self.api.dispatch({'version': 1, 'action': 'brightness', 'value': value})
            self.assertEqual((self.light/'brightness').read_text(), '500')
        self.assertEqual(self.api.dispatch({'version': 1, 'action': 'brightness', 'value': 25}),
                         {'brightness': 25})
        self.assertEqual((self.light/'brightness').read_text(), '250')

    def test_preference_survives_service_recreation(self):
        self.api.dispatch({'version': 1, 'action': 'theme', 'value': 'light'})
        fresh = control.Control(self.base/'sys', self.base/'proc', self.base/'state')
        self.assertEqual(fresh.prefs['theme'], 'light')
        self.assertEqual(fresh.prefs['input']['tap'], True)
        with self.assertRaises(ValueError):
            fresh.dispatch({'version': 1, 'action': 'theme', 'value': {'path': '/etc/shadow'}})
        self.assertEqual(json.loads((self.base/'state/preferences.json').read_text())['theme'], 'light')

    def test_input_rejects_unsafe_values_without_x_commands(self):
        for values in (None, {}, {'device': '/dev/input/event0'}, {'tap': 1},
                       {'speed': True}, {'speed': float('nan')}, {'speed': 2}):
            with self.assertRaises(ValueError):
                self.api.dispatch({'version': 1, 'action': 'input-settings', 'value': values})


if __name__ == '__main__':
    unittest.main()

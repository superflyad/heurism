#!/usr/bin/env python3
"""Local Companion control API. No network listener or arbitrary commands."""
import argparse
import json
import os
from pathlib import Path
import pwd
import socket
import socketserver
import struct
import subprocess
import time
import input_settings
import system_actions
import platform_config


def read(path, default=None):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return default


def integer(path):
    try:
        return int(read(path))
    except (ValueError, TypeError):
        return None


class Control:
    def __init__(self, sys_root='/sys', proc_root='/proc', state_root='/var/lib/companion/desktop'):
        self.sys = Path(sys_root)
        self.proc = Path(proc_root)
        self.state = Path(state_root)
        self.prefs = {'theme': 'night', 'input': dict(input_settings.DEFAULTS)}
        try:
            stored = json.loads((self.state / 'preferences.json').read_text())
            if stored.get('theme') in ('night', 'light'):
                self.prefs['theme'] = stored['theme']
            if stored.get('input'):
                self.prefs['input'].update(input_settings.validate(stored['input']))
        except (OSError, ValueError, AttributeError):
            pass

    def backlight(self):
        return next(iter(sorted((self.sys / 'class/backlight').glob('*'))), None)

    def status(self):
        mem = {}
        for line in (read(self.proc / 'meminfo', '') or '').splitlines():
            key, _, value = line.partition(':')
            if key in ('MemTotal', 'MemAvailable'):
                mem[key] = int(value.split()[0]) * 1024
        network = []
        try:
            addresses = subprocess.check_output(
                ['ip', '-4', '-o', 'addr', 'show', 'scope', 'global'], timeout=2,
                stderr=subprocess.DEVNULL, text=True)
            for line in addresses.splitlines():
                fields = line.split()
                if len(fields) >= 4 and fields[2] == 'inet':
                    address, prefix = fields[3].split('/')
                    network.append({'interface': fields[1], 'address': address, 'prefix': int(prefix)})
        except (OSError, ValueError, subprocess.SubprocessError):
            pass
        batteries, ac, thermal = [], [], []
        for device in sorted((self.sys / 'class/power_supply').glob('*')):
            kind = read(device / 'type')
            if kind == 'Battery':
                batteries.append({'name': device.name, 'capacity': integer(device / 'capacity'),
                                  'status': read(device / 'status', 'Unknown')})
            elif kind in ('Mains', 'USB', 'USB_C'):
                ac.append({'name': device.name, 'online': integer(device / 'online')})
        for zone in sorted((self.sys / 'class/thermal').glob('thermal_zone*')):
            value = integer(zone / 'temp')
            if value is not None and -20000 <= value <= 150000:
                thermal.append({'name': read(zone / 'type', zone.name), 'celsius': value / 1000})
        light = self.backlight()
        maximum = integer(light / 'max_brightness') if light else None
        current = integer(light / 'brightness') if light else None
        boot = read(self.proc / 'sys/kernel/random/boot_id')
        health = read('/var/lib/companion/healthy-boot-id')
        def running(service):
            try:
                return subprocess.run(['rc-service', service, 'status'], stdout=subprocess.DEVNULL,
                                      stderr=subprocess.DEVNULL, timeout=2).returncode == 0
            except (OSError, subprocess.SubprocessError):
                return False
        return {'version': 1, 'platform': platform_config.profile(), 'hostname': socket.gethostname(), 'kernel': os.uname().release,
                'boot_id': boot, 'uptime_seconds': float(read(self.proc / 'uptime', '0').split()[0]),
                'memory': mem, 'network': network, 'batteries': batteries, 'power': ac,
                'thermal': thermal, 'brightness': round(100 * current / maximum) if
                current is not None and maximum else None,
                'management': {'ssh': running('sshd'), 'watch': running('companion-watch'),
                               'boot_healthy': bool(boot and boot == health)},
                'bios': {'vendor': read(self.sys / 'class/dmi/id/bios_vendor'),
                         'version': read(self.sys / 'class/dmi/id/bios_version'),
                         'model': read(self.sys / 'class/dmi/id/product_name')},
                'preferences': dict(self.prefs)}

    def save(self, preferences):
        self.state.mkdir(parents=True, exist_ok=True)
        path = self.state / 'preferences.json'
        temporary = path.with_suffix('.tmp')
        with temporary.open('w') as stream:
            json.dump(preferences, stream)
            stream.flush()
            os.fsync(stream.fileno())
        temporary.replace(path)
        self.prefs = preferences

    def dispatch(self, request):
        if not isinstance(request, dict) or request.get('version') != 1:
            raise ValueError('Unsupported request')
        action = request.get('action')
        operations = {'network-scan': lambda: system_actions.scan_wifi(),
                      'network-connect': lambda: system_actions.connect_wifi(request.get('value')),
                      'bios-list': lambda: system_actions.bios_list(),
                      'bios-set': lambda: system_actions.bios_set(request.get('value')),
                      'admin-console': lambda: system_actions.admin_console(request.get('value')),
                      'power': lambda: system_actions.power(request.get('value'))}
        if action in operations:
            return operations[action]()
        if action == 'status':
            return self.status()
        if action == 'brightness':
            value = request.get('value')
            if type(value) is not int or not 5 <= value <= 100:
                raise ValueError('Brightness must be an integer from 5 to 100')
            light = self.backlight()
            maximum = integer(light / 'max_brightness') if light else None
            if not maximum:
                raise ValueError('No supported backlight')
            (light / 'brightness').write_text(str(max(1, round(maximum * value / 100))))
            return {'brightness': round(100 * integer(light / 'brightness') / maximum)}
        if action == 'theme':
            value = request.get('value')
            if value not in ('night', 'light'):
                raise ValueError('Unsupported theme')
            self.save(dict(self.prefs, theme=value))
            return dict(self.prefs)
        if action == 'input-status':
            return input_settings.effective()
        if action == 'input-settings':
            value = input_settings.validate(request.get('value'))
            previous = dict(self.prefs['input'])
            current = input_settings.apply(value)
            try:
                self.save(dict(self.prefs, input=dict(previous, **value)))
            except OSError:
                input_settings.apply(previous)
                raise
            return current
        raise ValueError('Unknown action')


class Handler(socketserver.StreamRequestHandler):
    def handle(self):
        self.connection.settimeout(3)
        try:
            _, uid, _ = struct.unpack('3i', self.connection.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
            if uid not in self.server.allowed_uids:
                raise ValueError('Unauthorized local caller')
            raw = self.rfile.readline(4097)
            if len(raw) > 4096 or not raw.endswith(b'\n'):
                raise ValueError('Invalid request frame')
            result = {'ok': True, 'data': self.server.control.dispatch(json.loads(raw))}
        except (ValueError, TypeError, KeyError, OSError, TimeoutError, subprocess.SubprocessError) as error:
            result = {'ok': False, 'error': str(error)}
        try:
            self.wfile.write(json.dumps(result).encode() + b'\n')
        except OSError:
            pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--socket', default='/run/companion-desktop/control.sock')
    args = parser.parse_args()
    account = pwd.getpwnam('companion-ui')
    path = Path(args.socket)
    path.parent.mkdir(parents=True, exist_ok=True)
    os.chown(path.parent, 0, account.pw_gid)
    os.chmod(path.parent, 0o750)
    path.unlink(missing_ok=True)
    with socketserver.UnixStreamServer(str(path), Handler) as server:
        os.chown(path, 0, account.pw_gid)
        os.chmod(path, 0o660)
        server.allowed_uids = {0, account.pw_uid}
        server.control = Control()
        print('Companion control ready', flush=True)
        server.serve_forever()


if __name__ == '__main__':
    main()

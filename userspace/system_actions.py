"""Fixed Companion system operations. Network changes affect only wireless."""
import json
import os
from pathlib import Path
import re
import subprocess
import threading
import signal
import platform_config

WIFI_CONFIG = Path('/etc/companion/wifi.conf')
RUNTIME = Path('/run/companion')


def managed_wireless(interface):
    """Find the daemon actually owning this wireless interface before reconfiguration."""
    for process in Path('/proc').glob('[0-9]*'):
        try:
            args = (process/'cmdline').read_bytes().split(b'\0')
        except OSError:
            continue
        if not args[0] or Path(os.fsdecode(args[0])).name != 'wpa_supplicant':
            continue
        if b'-i' not in args or args[args.index(b'-i')+1] != interface.encode():
            continue
        if b'-c' not in args or os.fsdecode(args[args.index(b'-c')+1]) != str(WIFI_CONFIG):
            raise ValueError('Wireless is managed by another configuration')
        return int(process.name)
    return None


def wireless_dhcp(interface):
    pidfile = RUNTIME/('dhcp-'+interface+'.pid')
    if not pidfile.exists():
        return None
    pid = int(pidfile.read_text())
    try:
        args = Path(f'/proc/{pid}/cmdline').read_bytes().split(b'\0')
    except FileNotFoundError:
        return None
    if (Path(os.fsdecode(args[0])).name != 'udhcpc' or b'-i' not in args
        or args[args.index(b'-i')+1] != interface.encode() or b'-p' not in args
        or os.fsdecode(args[args.index(b'-p')+1]) != str(pidfile)):
        raise ValueError('Wireless DHCP ownership could not be verified')
    return pid

BIOS_BASE = Path('/sys/class/firmware-attributes/dell-wmi-sysman/attributes')
BIOS_WRITABLE = {'FnLock', 'FnLockMode', 'KeyboardIllumination',
                 'KbdBacklightTimeoutAc', 'KbdBacklightTimeoutBatt'}


def run(args, timeout=5, **kwargs):
    result = subprocess.run(args, capture_output=True, text=True, timeout=timeout, **kwargs)
    if result.returncode:
        raise ValueError(result.stderr.strip() or 'System operation failed')
    return result.stdout


def wireless():
    for path in Path('/sys/class/net').glob('*'):
        if (path/'wireless').exists() and re.fullmatch(r'[A-Za-z0-9_.-]+', path.name):
            return path.name
    raise ValueError('No wireless interface available')


def validate_wifi(value):
    if not isinstance(value, dict) or set(value) != {'ssid', 'password'}:
        raise ValueError('Wi-Fi requires SSID and password')
    ssid, password = value['ssid'], value['password']
    if not isinstance(ssid, str) or not 1 <= len(ssid.encode('utf-8')) <= 32 or '\0' in ssid or '\n' in ssid:
        raise ValueError('Invalid Wi-Fi name')
    if not isinstance(password, str) or not 8 <= len(password.encode('utf-8')) <= 63 or '\n' in password or '\0' in password:
        raise ValueError('WPA password must contain 8–63 bytes')
    return ssid, password


def scan_wifi():
    interface = wireless()
    run(['ip', 'link', 'set', interface, 'up'])
    text = run(['iw', 'dev', interface, 'scan'], timeout=8)
    names = sorted({line.strip()[6:] for line in text.splitlines() if line.strip().startswith('SSID: ')})
    return {'interface': interface, 'networks': names[:80]}


def connect_wifi(value):
    ssid, password = validate_wifi(value)
    interface = wireless()
    daemon = managed_wireless(interface)
    dhcp = wireless_dhcp(interface)
    # Password stays on stdin; never in argv, logs or response JSON.
    config = run(['wpa_passphrase', ssid], input=password+'\n')
    config = '\n'.join(line for line in config.splitlines() if not line.strip().startswith('#psk='))
    config = 'ctrl_interface=/run/wpa_supplicant\n'+config+'\n'
    path = WIFI_CONFIG
    previous = path.read_bytes() if path.exists() else None
    temporary = path.with_suffix('.new')
    with temporary.open('w') as stream:
        os.chmod(temporary, 0o600)
        stream.write(config)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)
    try:
        run(['ip', 'link', 'set', interface, 'up'])
        pidfile = RUNTIME/('wpa-'+interface+'.pid')
        # Use the existing daemon if it has a control socket. Never terminate
        # unrelated processes or touch the Ethernet DHCP/driver.
        if daemon:
            reply = run(['wpa_cli', '-i', interface, 'reconfigure'], timeout=3)
            if 'OK' not in reply.splitlines():
                raise ValueError('Wireless reconfiguration was rejected')
        else:
            run(['wpa_supplicant', '-B', '-i', interface, '-c', str(path), '-P', str(pidfile)])
        if dhcp:
            os.kill(dhcp, signal.SIGUSR1)
        else:
            subprocess.Popen(['udhcpc', '-i', interface, '-b', '-t', '5', '-T', '2',
                              '-p', str(RUNTIME/('dhcp-'+interface+'.pid'))],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return {'interface': interface, 'ssid': ssid, 'state': 'Connecting; check live network status'}
    except (OSError, ValueError, subprocess.SubprocessError):
        if daemon is None:
            try:
                started = managed_wireless(interface)
                if started:
                    os.kill(started, signal.SIGTERM)
            except (OSError, ValueError):
                pass
        if previous is None:
            path.unlink(missing_ok=True)
        else:
            path.write_bytes(previous)
            os.chmod(path, 0o600)
        # If an existing daemon accepted the new file before a later failure,
        # ask that same verified owner to reread its restored configuration.
        if daemon and previous is not None:
            try:
                run(['wpa_cli', '-i', interface, 'reconfigure'], timeout=3)
            except (OSError, ValueError, subprocess.SubprocessError):
                pass
        raise ValueError('Could not configure wireless. Ethernet management was not changed.')


def bios_list():
    if platform_config.profile() == 'hyperv-dev':
        raise ValueError('Dell BIOS attributes are unavailable on this Hyper-V VM')
    attributes = json.loads(run(['/usr/local/sbin/companion-bios', 'list']))
    for name, item in attributes.items():
        item['writable'] = name in BIOS_WRITABLE and item.get('type') == 'enumeration'
    return attributes


def bios_set(value):
    if not isinstance(value, dict) or set(value) != {'name', 'value'} or value['name'] not in BIOS_WRITABLE:
        raise ValueError('This BIOS setting is read-only in Companion')
    if not isinstance(value['value'], str):
        raise ValueError('Invalid BIOS enum')
    info = bios_list()[value['name']]
    if value['value'] not in (info.get('possible_values') or '').split(';'):
        raise ValueError('Unsupported BIOS value')
    return json.loads(run(['/usr/local/sbin/companion-bios', 'set', value['name'], value['value']]))


def admin_console(kind):
    commands = {'admin': ['/bin/sh', '-l'], 'wifi': ['/usr/local/sbin/companion-wifi']}
    if kind not in commands:
        raise ValueError('Unknown console')
    env = dict(os.environ, DISPLAY=':0', XAUTHORITY='/run/companion-desktop/Xauthority')
    process = subprocess.Popen(['xterm', '-T', 'Companion Administrator', '-fa', 'DejaVu Sans Mono', '-fs', '14',
                                '-e', *commands[kind]], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return {'pid': process.pid, 'privilege': 'root'}


def verify_boot():
    if platform_config.profile() == 'hyperv-dev':
        return platform_config.verify_vm_boot(run)
    run(['sha256sum', '-c', '/var/lib/companion/presentation-20260927T161814Z/protected.sha256'])
    if 'DriverOrder: 0000,0001\n' not in run(['efibootmgr', '--driver']):
        raise ValueError('Firmware driver order needs management review before power action')
    boot = run(['efibootmgr', '-v'])
    order = re.search(r'^BootOrder: ([0-9A-Fa-f,]+)$', boot, re.M)
    entries = dict(re.findall(r'^Boot([0-9A-Fa-f]{4})\*? (.+)$', boot, re.M))
    if ('BootNext:' in boot or not order or order[1].split(',')[:2] != ['0005', '0000']
        or '\\EFI\\alpine\\grubx64.efi' not in entries.get('0005', '')
        or '\\EFI\\Boot\\BootX64.efi' not in entries.get('0000', '')):
        raise ValueError('Boot selection needs management review before power action')
    extras = order[1].split(',')[2:]
    for entry in extras:
        device = entries.get(entry, '')
        if ('MAC(' not in device or ('IPv4(' not in device and 'IPv6(' not in device)
            or '{auto_created_boot_option}' not in device):
            raise ValueError('Unexpected boot entry; management review required')
    if extras:
        # Dell appends NIC entries during normal startup. Restore only the
        # already-proven SSD order; never select a network entry or BootNext.
        (RUNTIME/'power-boot-before.txt').write_text(boot)
        run(['efibootmgr', '-o', '0005,0000'])
        if 'BootOrder: 0005,0000\n' not in run(['efibootmgr']):
            raise ValueError('Could not verify restored SSD boot order')


def power(value):
    if not isinstance(value, dict) or value.get('confirm') is not True or value.get('operation') not in ('reboot', 'poweroff'):
        raise ValueError('Confirm the specific power action')
    verify_boot()
    command = ['/sbin/'+value['operation']]
    threading.Timer(2, lambda: subprocess.Popen(command)).start()
    return {'scheduled': value['operation']}

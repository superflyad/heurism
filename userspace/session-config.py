#!/usr/bin/env python3
"""Generate explicit Xorg input configuration without changing mdev or networking."""
from pathlib import Path
import json
import subprocess
import platform_config

devices = []
platform = platform_config.profile()
Path('/run/companion-desktop').mkdir(parents=True, exist_ok=True)
for sound in sorted(Path('/sys/class/sound').glob('card[0-9]*')):
    # mdev created the cold-boot ALSA nodes before udev started. PulseAudio
    # requires the card's SOUND_INITIALIZED change-event classification.
    # Populate this card only; test does not execute queued RUN rules.
    if not (sound / ('controlC'+sound.name[4:])).exists():
        continue
    result = subprocess.run(['udevadm', 'test', '--action=change', str(sound)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=5, text=True)
    Path('/run/companion-desktop/audio-'+sound.name+'-udev.log').write_text(result.stdout)
    if result.returncode:
        print('Audio classification failed: '+sound.name, flush=True)
for event in sorted(Path('/sys/class/input').glob('event*')):
    name = (event / 'device/name').read_text().strip()
    if name == 'AT Translated Set 2 keyboard':
        devices.append(('keyboard', str(Path('/dev/input') / event.name), 'CoreKeyboard'))
    elif name.endswith('Touchpad'):
        devices.append(('touchpad', str(Path('/dev/input') / event.name), 'CorePointer'))
    elif name == 'CUST0000:00 04F3:2A4B':
        devices.append(('touchscreen', str(Path('/dev/input') / event.name), 'SendCoreEvents'))
    elif platform == 'hyperv-dev' and name == 'Microsoft Vmbus HID-compliant Mouse':
        devices.append(('pointer', str(Path('/dev/input') / event.name), 'CorePointer'))

cards = [p for p in Path('/sys/class/drm').glob('card*') if (p / 'device/vendor').exists()
         and (p / 'device/vendor').read_text().strip() == '0x8086']
if platform == 'dell' and (len(cards) != 1 or not any(d[0] == 'keyboard' for d in devices)):
    raise SystemExit('Expected Intel display and keyboard were not found')
if platform == 'hyperv-dev' and (not Path('/dev/fb0').exists() or
        not any(d[0] == 'keyboard' for d in devices) or not any(d[0] == 'pointer' for d in devices)):
    raise SystemExit('Expected Hyper-V framebuffer, keyboard and pointer were not found')
config = '''Section "ServerFlags"
 Option "AutoAddDevices" "false"
 Option "AutoAddGPU" "false"
 Option "BlankTime" "0"
 Option "StandbyTime" "0"
 Option "SuspendTime" "0"
 Option "OffTime" "0"
EndSection
Section "Device"
 Identifier "intel"
 Driver "modesetting"
 Option "AccelMethod" "none"
 Option "kmsdev" "%s"
EndSection
Section "Screen"
 Identifier "screen"
 Device "intel"
EndSection
''' % ('/dev/dri/' + cards[0].name if platform == 'dell' else '/dev/fb0')
if platform == 'hyperv-dev':
    config = config.replace('Identifier "intel"', 'Identifier "hyperv"').replace('Device "intel"', 'Device "hyperv"')
    config = config.replace('Driver "modesetting"', 'Driver "fbdev"').replace('Option "kmsdev"', 'Option "fbdev"')
for identifier, device, role in devices:
    # libinput needs input classification metadata. Populate only these selected
    # input devices; do not switch the system device manager or trigger networking.
    event = Path(device).name
    result = subprocess.run(['udevadm', 'test', '/sys/class/input/' + event],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=5, text=True)
    Path('/run/companion-desktop').mkdir(parents=True, exist_ok=True)
    Path('/run/companion-desktop/' + identifier + '-udev.log').write_text(result.stdout)
    if result.returncode:
        raise SystemExit('Input classification failed: ' + identifier)
    config += f'''Section "InputDevice"
 Identifier "{identifier}"
 Driver "libinput"
 Option "Device" "{device}"
 Option "Tapping" "true"
 Option "NaturalScrolling" "true"
 Option "ScrollMethod" "twofinger"
 Option "DisableWhileTyping" "true"
EndSection
'''
config += 'Section "ServerLayout"\n Identifier "companion"\n Screen "screen"\n'
for identifier, device, role in devices:
    config += f' InputDevice "{identifier}" "{role}"\n'
config += 'EndSection\n'
Path('/run/companion-desktop').mkdir(parents=True, exist_ok=True)
Path('/run/companion-desktop/xorg.conf').write_text(config)
print(json.dumps({'platform': platform, 'display': cards[0].name if platform == 'dell' else 'fb0', 'input': devices}), flush=True)

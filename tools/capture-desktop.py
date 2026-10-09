"""Capture the trusted Dell X screen and convert its bounded XWD dump to PNG."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
from PIL import Image

repo = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--name', default='physical')
args = parser.parse_args()
if not args.name.replace('-', '').isalnum():
    parser.error('Name must be alphanumeric with optional hyphens')
remote = '/var/lib/companion/desktop-stage/evidence/' + args.name + '.xwd'
subprocess.run([sys.executable, str(repo/'tools/dell.py'), '--command',
               'DISPLAY=:0 XAUTHORITY=/run/companion-desktop/Xauthority xwd -root -silent -out '+remote], check=True)
state = json.loads((repo/'artifacts/targets/dell.json').read_text())
identity = repo/'artifacts/ssh'
out = repo/'build/desktop'
out.mkdir(parents=True, exist_ok=True)
local = out/(args.name+'.xwd')
options = ['-i', str(identity/'companion_client_ed25519'), '-o', 'BatchMode=yes',
           '-o', 'StrictHostKeyChecking=yes', '-o', 'HostKeyAlias=companion-dell',
           '-o', 'UserKnownHostsFile='+str(identity/'known_hosts')]
subprocess.run(['scp', *options, 'root@'+state['address']+':'+remote, str(local)], check=True, timeout=30)
raw = local.read_bytes()
header = struct.unpack('>25I', raw[:100])
size, version, format_, depth, width, height, xoffset, order = header[:8]
bits, stride, visual, red, green, blue, _, _, colors = header[11:20]
assert version == 7 and format_ == 2 and depth == 24 and bits in (24, 32) and order == 0
assert (red, green, blue) == (0xff0000, 0xff00, 0xff) and xoffset == 0
assert 0 < width <= 16384 and 0 < height <= 16384 and stride >= width * (bits//8)
pixels = raw[size + colors*12:]
assert len(pixels) == stride * height
image = Image.frombytes('RGB', (width, height), pixels, 'raw', 'BGRX' if bits == 32 else 'BGR', stride)
path = out/(args.name+'.png')
image.save(path)
print(path)

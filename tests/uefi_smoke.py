"""Boot the real EFI application from virtual USB in QEMU/EDK II.

Usage: python tests/uefi_smoke.py PATH_TO_QEMU_DIRECTORY
Requires tools/build.ps1 -Test first. No installed QEMU service or physical disk writes.
"""
import ctypes
import json
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import sys
import time
import zlib

repo = Path(__file__).resolve().parents[1]
runtime = Path(sys.argv[1]).resolve()
output = repo / 'build' / 'uefi-smoke'
output.mkdir(parents=True, exist_ok=True)
shutil.copyfile(runtime / 'share' / 'edk2-i386-vars.fd', output / 'vars.fd')
shutil.copytree(repo / 'build' / 'esp', output / 'esp', dirs_exist_ok=True)

class Framebuffer(ctypes.Structure):
    _fields_ = [('pixels', ctypes.POINTER(ctypes.c_uint32)),
                ('width', ctypes.c_uint32), ('height', ctypes.c_uint32),
                ('stride', ctypes.c_uint32), ('format', ctypes.c_uint32),
                ('size', ctypes.c_uint64)]

dll = ctypes.CDLL(str(repo / 'build' / 'tests.dll'))
dll.companion_screen.argtypes = [ctypes.POINTER(Framebuffer)]
dll.companion_screen.restype = None

def title_matches(path):
    with path.open('rb') as handle:
        assert handle.readline().strip() == b'P6'
        width, height = map(int, handle.readline().split())
        assert handle.readline().strip() == b'255'
        actual = handle.read()
    pixels = (ctypes.c_uint32 * (width * height))()
    fb = Framebuffer(pixels, width, height, width, 1, width * height * 4)
    dll.companion_screen(ctypes.byref(fb))
    scale = 5 if width >= 960 and height >= 600 else 2
    title_width = 9 * 6 * scale
    x = max(0, (width - title_width) // 2)
    y = height // 3
    for row in range(y, y + 7 * scale):
        for col in range(x, x + title_width):
            pixel = pixels[row * width + col]
            expected = bytes(((pixel >> 16) & 255, (pixel >> 8) & 255, pixel & 255))
            index = (row * width + col) * 3
            if actual[index:index+3] != expected:
                return False
    return True

def save_png(ppm, png):
    with ppm.open('rb') as handle:
        assert handle.readline().strip() == b'P6'
        width, height = map(int, handle.readline().split())
        assert handle.readline().strip() == b'255'
        pixels = handle.read()
    rows = b''.join(b'\0' + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    png.write_bytes(b'\x89PNG\r\n\x1a\n'
                    + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
                    + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))

with socket.socket() as reservation:
    reservation.bind(('127.0.0.1', 0))
    port = reservation.getsockname()[1]
args = [str(runtime / 'qemu-system-x86_64.exe'), '-machine', 'q35', '-accel', 'tcg',
        '-m', '256M', '-display', 'none', '-net', 'none',
        '-drive', f'if=pflash,format=raw,readonly=on,file={runtime / "share" / "edk2-x86_64-code.fd"}',
        '-drive', f'if=pflash,format=raw,file={output / "vars.fd"}',
        '-device', 'qemu-xhci',
        '-drive', f'if=none,id=stick,format=raw,file=fat:rw:{output / "esp"}',
        '-device', 'usb-storage,drive=stick,bootindex=1',
        '-qmp', f'tcp:127.0.0.1:{port},server=on,wait=off',
        '-serial', f'file:{output / "serial.log"}']
stderr = (output / 'qemu.log').open('w')
process = subprocess.Popen(args, stdout=stderr, stderr=stderr, creationflags=0x08000000)
connection = None
try:
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError('QEMU exited: ' + (output / 'qemu.log').read_text())
        try:
            connection = socket.create_connection(('127.0.0.1', port), timeout=2)
            break
        except OSError:
            time.sleep(0.2)
    assert connection, 'QMP did not become available'
    stream = connection.makefile('rwb')
    greeting = json.loads(stream.readline())
    assert 'QMP' in greeting

    def command(name, arguments=None):
        request = {'execute': name}
        if arguments is not None:
            request['arguments'] = arguments
        stream.write(json.dumps(request).encode() + b'\n')
        stream.flush()
        while True:
            response = json.loads(stream.readline())
            if 'error' in response:
                raise RuntimeError(response['error'])
            if 'return' in response:
                return response['return']

    command('qmp_capabilities')
    screenshot = output / 'companion.ppm'
    deadline = time.monotonic() + 60
    matched = False
    while time.monotonic() < deadline:
        command('screendump', {'filename': str(screenshot)})
        if title_matches(screenshot):
            matched = True
            break
        time.sleep(1)
    assert matched, f'Companion title not observed; inspect {screenshot} and serial.log'
    save_png(screenshot, output / 'companion.png')
    print('PASS: real x64 EFI executable booted through EDK II from virtual USB', flush=True)
    print(f'Firmware screenshot: {screenshot}', flush=True)
    command('human-monitor-command', {'command-line': 'sendkey esc'})
    time.sleep(3)
    after = output / 'after-escape.ppm'
    command('screendump', {'filename': str(after)})
    assert not title_matches(after), 'Escape did not leave Companion screen'
    save_png(after, output / 'after-escape.png')
    print('PASS: Escape returned control to firmware', flush=True)
    command('quit')
    process.wait(timeout=10)
finally:
    if connection:
        connection.close()
    if process.poll() is None:
        process.terminate()
        process.wait(timeout=10)
    stderr.close()

"""Check the actual PE image and run freestanding C against mock firmware."""
import ctypes
import struct
import sys
import zlib
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
assert data[:2] == b'MZ', 'Missing DOS header'
pe = struct.unpack_from('<I', data, 0x3C)[0]
assert data[pe:pe+4] == b'PE\0\0', 'Missing PE signature'
machine, sections = struct.unpack_from('<HH', data, pe + 4)
assert machine == 0x8664, 'Not x86-64'
optional = pe + 24
assert struct.unpack_from('<H', data, optional)[0] == 0x20B, 'Not PE32+'
assert struct.unpack_from('<H', data, optional + 68)[0] == 10, 'Not an EFI application'
entry = struct.unpack_from('<I', data, optional + 16)[0]
assert entry, 'Missing entry point'
assert struct.unpack_from('<II', data, optional + 120) == (0, 0), 'Unexpected OS imports'
reloc_rva, reloc_size = struct.unpack_from('<II', data, optional + 152)
assert reloc_rva and reloc_size, 'Missing relocations for firmware loading'
table = optional + struct.unpack_from('<H', data, pe + 20)[0]
executable_entry = False
for n in range(sections):
    offset = table + n * 40
    size, rva = struct.unpack_from('<II', data, offset + 8)
    flags = struct.unpack_from('<I', data, offset + 36)[0]
    if rva <= entry < rva + size and flags & 0x20000000:
        executable_entry = True
assert executable_entry, 'Entry point not in executable section'
print(f'PASS: x64 PE32+ EFI application, {len(data)} bytes, relocatable, no OS imports')
dll = ctypes.CDLL(str(Path(sys.argv[2]).resolve()))
dll.run_tests.restype = ctypes.c_int
line = dll.run_tests()
assert line == 0, f'C verification failed at tests/host.c:{line}'
print('PASS: framebuffer bounds, stride, RGB/BGR, rejected modes, mock UEFI boot and failure paths')

# Render the actual C UI into host memory. This preview is not an emulator boot.
class Framebuffer(ctypes.Structure):
    _fields_ = [('pixels', ctypes.POINTER(ctypes.c_uint32)),
                ('width', ctypes.c_uint32), ('height', ctypes.c_uint32),
                ('stride', ctypes.c_uint32), ('format', ctypes.c_uint32),
                ('size', ctypes.c_uint64)]

width, height = 1280, 720
pixels = (ctypes.c_uint32 * (width * height))()
fb = Framebuffer(pixels, width, height, width, 1, width * height * 4)
dll.companion_screen.argtypes = [ctypes.POINTER(Framebuffer)]
dll.companion_screen.restype = None
dll.companion_screen(ctypes.byref(fb))
rows = bytearray()
for y in range(height):
    rows.append(0)
    for x in range(width):
        pixel = pixels[y * width + x]
        rows.extend(((pixel >> 16) & 255, (pixel >> 8) & 255, pixel & 255))

def chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))

png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))
preview = Path(sys.argv[1]).resolve().parents[3] / 'companion-screen.png'
preview.write_bytes(png)
print(f'Host renderer preview: {preview}')

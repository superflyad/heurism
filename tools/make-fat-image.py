"""Create a 2 GiB MBR/FAT32 UEFI test disk from a directory (standard library).

Only creates an ordinary image file. Never opens a physical disk.
"""
from array import array
import math
from pathlib import Path
import struct
import sys

source = Path(sys.argv[1]).resolve()
target = Path(sys.argv[2]).resolve()
assert source.is_dir() and not target.is_dir()
sector = 512
start = 2048
total = 2 * 1024**3 // sector
partition_sectors = total - start
cluster_sectors = 32
cluster_bytes = sector * cluster_sectors
reserved = 32
fat_sectors = math.ceil((partition_sectors // cluster_sectors + 2) * 4 / sector)
data_start = start + reserved + 2 * fat_sectors
cluster_count = (total - data_start) // cluster_sectors
fat = array('I', [0]) * (fat_sectors * sector // 4)
fat[0], fat[1] = 0x0ffffff8, 0x0fffffff
next_cluster = 2
nodes = {}

def short_name(path, ordinal):
    name = path.name
    parts = name.rsplit('.', 1)
    base = parts[0] if len(parts) == 2 else name
    extension = parts[1] if len(parts) == 2 else ''
    legal = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$~!#%&-{}()@`'
    if 1 <= len(base) <= 8 and len(extension) <= 3 and all(c in legal for c in (base + extension).upper()):
        return base.upper().ljust(8).encode() + extension.upper().ljust(3).encode()
    return f'F{ordinal:07d}'.encode() + extension.upper()[:3].ljust(3).encode('ascii', 'replace')

def lfn_entries(name, alias):
    checksum = 0
    for byte in alias:
        checksum = (((checksum & 1) << 7) + (checksum >> 1) + byte) & 255
    text = name.encode('utf-16le') + b'\0\0'
    count = math.ceil(len(text) / 26)
    text = text.ljust(count * 26, b'\xff')
    result = []
    for index in range(count, 0, -1):
        fragment = text[(index - 1) * 26:index * 26]
        entry = bytearray(32)
        entry[0] = index | (0x40 if index == count else 0)
        entry[1:11] = fragment[:10]; entry[11] = 0x0f; entry[13] = checksum
        entry[14:26] = fragment[10:22]; entry[28:32] = fragment[22:26]
        result.append(bytes(entry))
    return result

def entry(alias, attributes, cluster, size=0):
    result = bytearray(32)
    result[:11] = alias; result[11] = attributes
    struct.pack_into('<H', result, 20, cluster >> 16)
    struct.pack_into('<H', result, 26, cluster & 0xffff)
    struct.pack_into('<I', result, 28, size)
    return bytes(result)

def allocate(path):
    global next_cluster
    if path.is_dir():
        children = sorted(path.iterdir())
        count = 1 + (0 if path == source else 2)
        for child in children:
            count += 1 + math.ceil((len(child.name.encode('utf-16le')) + 2) / 26)
        size = count * 32
    else:
        children = []; size = path.stat().st_size
    count = max(1, math.ceil(size / cluster_bytes))
    first = next_cluster
    for cluster in range(first, first + count):
        fat[cluster] = cluster + 1 if cluster < first + count - 1 else 0x0fffffff
    next_cluster += count
    assert next_cluster < cluster_count + 2, 'Payload exceeds test image'
    nodes[path] = (first, count, children)
    for child in children:
        allocate(child)

allocate(source)
assert nodes[source][0] == 2
target.parent.mkdir(parents=True, exist_ok=True)
with target.open('wb') as image:
    image.truncate(total * sector)
    mbr = bytearray(sector)
    struct.pack_into('<I', mbr, 440, 0x434f4d50)
    mbr[446] = 0x80; mbr[450] = 0xef
    struct.pack_into('<II', mbr, 454, start, partition_sectors)
    mbr[510:512] = b'\x55\xaa'; image.write(mbr)
    boot = bytearray(sector)
    boot[:3] = b'\xeb\x58\x90'; boot[3:11] = b'MSWIN4.1'
    struct.pack_into('<HBHBHHBHHHII', boot, 11, sector, cluster_sectors, reserved,
                     2, 0, 0, 0xf8, 0, 63, 255, start, partition_sectors)
    struct.pack_into('<IHHIHH', boot, 36, fat_sectors, 0, 0, 2, 1, 6)
    boot[64] = 0x80; boot[66] = 0x29
    struct.pack_into('<I', boot, 67, 0x434f4d50)
    boot[71:82] = b'COMPANION  '; boot[82:90] = b'FAT32   '
    boot[510:512] = b'\x55\xaa'
    fsinfo = bytearray(sector)
    struct.pack_into('<I', fsinfo, 0, 0x41615252)
    struct.pack_into('<III', fsinfo, 484, 0x61417272, cluster_count - (next_cluster - 2), next_cluster)
    struct.pack_into('<I', fsinfo, 508, 0xaa550000)
    for offset, value in [(start, boot), (start + 1, fsinfo), (start + 6, boot), (start + 7, fsinfo)]:
        image.seek(offset * sector); image.write(value)
    if sys.byteorder != 'little': fat.byteswap()
    for index in range(2):
        image.seek((start + reserved + index * fat_sectors) * sector); image.write(fat.tobytes())
    for path, (first, count, children) in nodes.items():
        image.seek((data_start + (first - 2) * cluster_sectors) * sector)
        if path.is_dir():
            contents = bytearray()
            if path != source:
                contents.extend(entry(b'.          ', 0x10, first))
                parent_cluster = 0 if path.parent == source else nodes[path.parent][0]
                contents.extend(entry(b'..         ', 0x10, parent_cluster))
            for ordinal, child in enumerate(children, 1):
                alias = short_name(child, ordinal)
                for long_entry in lfn_entries(child.name, alias): contents.extend(long_entry)
                contents.extend(entry(alias, 0x10 if child.is_dir() else 0x20,
                                      nodes[child][0], 0 if child.is_dir() else child.stat().st_size))
            contents.extend(bytes(32))
            image.write(contents)
        else:
            with path.open('rb') as file:
                while data := file.read(4 * 1024**2): image.write(data)
print(f'FAT32 UEFI disk image: {target} ({next_cluster - 2} allocated clusters)')

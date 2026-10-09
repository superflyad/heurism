"""Check actual EFI headers and the probe's host ABI/recovery tests."""
import ctypes
import hashlib
from pathlib import Path
import struct
import sys

root = Path(sys.argv[1]).resolve()
data = (root / 'fvprobex64.efi').read_bytes()
assert data[:2] == b'MZ'
pe = struct.unpack_from('<I', data, 0x3c)[0]
assert data[pe:pe+4] == b'PE\0\0'
assert struct.unpack_from('<H', data, pe+4)[0] == 0x8664
optional = pe + 24
assert struct.unpack_from('<H', data, optional)[0] == 0x20b
assert struct.unpack_from('<H', data, optional+68)[0] == 10
assert struct.unpack_from('<II', data, optional+120) == (0, 0)
assert all(struct.unpack_from('<II', data, optional+152))
dll = ctypes.CDLL(str(root / 'probe-tests.dll'))
assert dll.probe_run_tests() == 0
dll.probe_mock_report.restype = ctypes.c_char_p
report = dll.probe_mock_report().decode()
assert 'PROBE_COMPLETE' in report
assert 'base=0x00000000ff110000' in report
assert 'FV_FILES count=0x0000000000000000' in report
if root.name == 'update-probe':
    assert 'COMPANION_UPDATE_PROBE_01' in report
    assert 'FMP_INFO status=0x0000000000000000' in report
    assert 'FMP_IMAGE provider=0x0000000000000000 index=0x0000000000000001' in report
    assert 'setting=0x0000000000000007' in report
if root.name == 'policy-probe':
    assert 'COMPANION_POLICY_PROBE_01' in report
    assert 'signature_policy_byte=0x0000000000000001' in report
    assert 'fallback_version=0x0000000000000005' in report
    assert 'status=0x800000000000000e present=0x0000000000000000' in report
if root.name == 'extension-probe':
    assert 'COMPANION_EXTENSION_PROBE_01' in report
    assert 'EXTENSION_LOCATE status=0x0000000000000000' in report
    assert 'magic=0x314458454d504f43 revision=0x0000000000000001' in report
if root.name == 'http-inspect-probe':
    assert 'COMPANION_HTTP_INSPECT_01' in report
    assert 'HTTP_INSPECTION_COMPLETE' in report
    assert 'HII_DATABASE status=0x800000000000000e' in report
(root / 'mock-report.txt').write_text(report, encoding='utf-8')
print('PASS: relocatable x64 EFI, no OS imports; read-only protocol fixtures, ESP report, GRUB handoff and bounds')
print('EFI SHA256:', hashlib.sha256(data).hexdigest())

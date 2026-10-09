"""Extract the verified Dell recovery download as data using pinned BIOSUtilities.

No executable payload or flash utility is launched. All output is private.
"""
from contextlib import redirect_stdout
import hashlib
import json
from pathlib import Path
import subprocess
import sys

repo = Path(__file__).resolve().parents[1]
root = repo / 'artifacts/firmware'
source = root / 'research-biosutilities'
revision = '70c3a0852a6aa2643c8114ea73bc833e3b4cff0d'
head = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
if head != revision:
    raise SystemExit('Parser checkout differs from reviewed revision')
package = root / 'vendor/BIOS_IMG-1.35.0.rcv'
digest = hashlib.sha256(package.read_bytes()).hexdigest()
if digest != 'e649ae3fc5684a7bc7790bc3f2dca7f4235986d80af469ec60b926fc0e7fa9ed':
    raise SystemExit('Recovery package differs from Dell published hash')
destination = root / 'vendor/recovery-1.35.0-extracted'
manifest = root / 'recovery-package-analysis.json'
if destination.exists():
    raise SystemExit('Extraction already exists; preserve it and inspect the saved manifest')
sys.path.insert(0, str(source))
from biosutilities.dell_pfs_extract import DellPfsExtract

parser = DellPfsExtract(input_object=str(package), extract_path=str(destination),
                        advanced=True, structure=True)
if parser._is_pfs_pkg(input_object=package.read_bytes()) or not parser.check_format():
    raise SystemExit('Unexpected package format')
with (root / 'vendor/recovery-extraction.txt').open('w', encoding='utf-8') as stream:
    with redirect_stdout(stream):
        succeeded = parser.parse_format()
if not succeeded:
    raise SystemExit('Parser did not report success; preserve output for inspection')
files = []
for path in sorted(destination.rglob('*')):
    if not path.is_file():
        continue
    if not path.resolve().is_relative_to(destination.resolve()):
        raise SystemExit('Output escaped extraction directory')
    data = path.read_bytes()
    files.append({'path': path.relative_to(destination).as_posix(), 'bytes': len(data),
                  'sha256': hashlib.sha256(data).hexdigest()})
report = {'scope': 'File-only parsing; signature presence is not signature validation '
                   'or hardware acceptance', 'package_sha256': digest,
          'parser_revision': revision, 'file_count': len(files), 'files': files}
manifest.write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({key: value for key, value in report.items() if key != 'files'}, indent=2))

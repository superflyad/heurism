"""Record upstream coreboot board support at an exact commit (no target writes)."""
from datetime import datetime, timezone
import json
from pathlib import Path
from urllib.request import Request, urlopen

def fetch(url):
    with urlopen(Request(url, headers={'User-Agent': 'Companion-firmware-research'}), timeout=45) as response:
        return json.load(response)

commit = fetch('https://api.github.com/repos/coreboot/coreboot/commits/main')['sha']
tree = fetch('https://api.github.com/repos/coreboot/coreboot/git/trees/' + commit + '?recursive=1')
if tree.get('truncated'):
    raise SystemExit('Upstream tree truncated; support cannot be assessed from this result')
paths = [entry['path'] for entry in tree['tree']]
report = {
    'utc': datetime.now(timezone.utc).isoformat(), 'coreboot_commit': commit,
    'source': 'https://github.com/coreboot/coreboot/tree/' + commit,
    'dell_board_paths': [p for p in paths if p.startswith('src/mainboard/dell/')],
    'target_name_matches': [p for p in paths if any(s in p.lower() for s in ['7506', 'vk62x'])],
    'tigerlake_soc_present': 'src/soc/intel/tigerlake/Kconfig' in paths,
    'cse_source_paths': [p for p in paths if p.startswith('src/soc/intel/common/block/cse/')],
    'limitations': 'Main upstream tree only; no assessment of unmerged or private ports. '
                   'Chipset support does not establish support for the Dell board or firmware acceptance.'}
destination = Path(__file__).resolve().parents[1] / 'artifacts/firmware/coreboot-support.json'
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({key: value for key, value in report.items() if key != 'dell_board_paths'}, indent=2))

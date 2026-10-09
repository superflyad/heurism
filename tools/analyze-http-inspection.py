"""Summarize the completed physical inspection; do not infer diskless boot."""
import hashlib
import json
from pathlib import Path
import re
import uuid
repo=Path(__file__).resolve().parents[1];root=repo/'artifacts/firmware'
text=(root/'http-inspect-observation.txt').read_text()
assert text.startswith('COMPANION_HTTP_INSPECT_01\n') and text.endswith('PROBE_COMPLETE\n')
assert 'HTTP_INSPECTION_COMPLETE\n' in text
module=(repo/'artifacts/research/independent-bootstrap/http-boot-dxe.bin').read_bytes()
config_guid='4d20583a-7765-4e7a-8a67-dcde74ee3ec5'
assert uuid.UUID(config_guid).bytes_le in module
protocols={}
for name,status,count in re.findall(r'^INSPECT_LOCATE (\w+) status=(0x[0-9a-f]+) count=(0x[0-9a-f]+)$',text,re.M):
 protocols[name]={'status':status,'count':int(count,16)}
providers=[line for line in text.splitlines() if line.startswith('INSPECT_INTERFACE LOAD_FILE ')]
packages=[line for line in text.splitlines() if line.startswith('HII_LIST_ITEM ')]
bindings=[line for line in text.splitlines() if line.startswith('BINDING_IMAGE ')]
report={'boot_id':json.loads((root/'http-inspect-probe-deployment.json').read_text())['verification']['boot_id'],
 'report_sha256':hashlib.sha256(text.encode()).hexdigest(),'protocols':protocols,
 'loadfile_providers':len(providers),'all_loadfile_providers_have_pxe':all('pxe_interface_status=0x0000000000000000' in line for line in providers),
 'uri_loadfile_paths':sum('URI_PRESENT' in line for line in providers),
 'driver_binding_images_inspected':len(bindings),
 'http_boot_bindings':sum('http_boot_image=0x0000000000000001' in line for line in bindings),
 'hii_package_lists':len(packages),
 'exports_skipped':text.count('HII_EXPORT_SKIPPED'),
 'http_boot_config_guid_in_saved_driver':config_guid,
 'http_boot_config_guid_observed':config_guid in text,
 'http_varstore_observed':'HTTP_BOOT_CONFIG_IFR_NVDATA' in text,
 'keyword_only_http_packages':sum('http_keyword=0x0000000000000001' in line for line in packages),
 'scope':'Physical snapshot before management OS, standard interfaces only; no connection/configuration operations. Absence here is not proof that initialization cannot publish the driver later.'}
assert report['hii_package_lists']==48 and report['exports_skipped']==0
(root/'http-inspect-analysis.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))

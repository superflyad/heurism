"""Read-only offline checks: NVRAM record format and Dell loader references."""
import hashlib,json,re,struct,uuid
from pathlib import Path
repo=Path(__file__).resolve().parents[1];root=repo/'artifacts/firmware';out=repo/'artifacts/research/nv-dispatch'
image=(root/'bios16-with-nv-extension.bin').read_bytes();payload=(root/'nv-extension-payload-readback.bin').read_bytes()
assert hashlib.sha256(image).hexdigest()==json.loads((root/'bios16-with-nv-extension.json').read_text())['sha256']
assert len(payload)==2048 and hashlib.sha256(payload).hexdigest()=='b89ffa86d43a68a5296ed762702b25617d805e29a15f1f596dc6aed2f0d0151a'
at=image.index(payload);name=('CompanionExtensionImage01\0').encode('utf-16-le');name_at=image.rfind(name,at-512,at)
assert name_at>=0
# EFI variable records have a variable header and name, not an FFS file header.
guid=uuid.UUID('1d8ce97b-55e6-4b2e-9276-bb1e9b6615a1').bytes_le
guid_at=image.rfind(guid,name_at-128,name_at);assert guid_at>=0
name_size,data_size=struct.unpack_from('<II',image,guid_at-8)
assert name_size==len(name) and data_size==len(payload)
assert guid_at+16==name_at and name_at+name_size==at
core=(root/'update-modules/dxe-core.bin').read_bytes()
assert hashlib.sha256(core).hexdigest()=='8641816dae6964f620aeac118b0303376f1abe0d3e6be6f066e17cbf52d6c246'
pe=struct.unpack_from('<I',core,60)[0];sections=pe+24+struct.unpack_from('<H',core,pe+20)[0]
def rva(offset):
 for i in range(struct.unpack_from('<H',core,pe+6)[0]):
  _,virtual_size,address,size,start=struct.unpack_from('<8sIIII',core,sections+40*i)
  if start<=offset<start+size:return address+offset-start
 raise ValueError('Unmapped file offset')
text=(root/'update-modules/dxe-core.txt').read_text();references={}
for label,value in {'fv2':'220e73b6-6bdb-4413-8405-b974b108619a','filesystem':'964e5b22-6459-11d2-8e39-00a0c969723b',
                    'loadfile':'56ec3091-954c-11d2-8e3f-00a0c969723b','loadfile2':'4006c0c1-fcb3-403e-996d-4a6c8724e06d'}.items():
 offset=core.find(uuid.UUID(value).bytes_le);assert offset>=0
 address=rva(offset);lines=[line.strip() for line in text.splitlines() if re.search(r'# 0x'+format(address,'x')+r'\b',line)]
 references[label]={'guid':value,'guid_rva':hex(address),'rip_references':lines}
report={'scope':__doc__,'bios_sha256':hashlib.sha256(image).hexdigest(),'payload_offset':hex(at),
 'variable_name_offset':hex(name_at),'vendor_guid_offset':hex(guid_at),
 'variable_header_prefix_hex':image[max(0,guid_at-32):guid_at].hex(),
 'variable_store_volume_guid':str(uuid.UUID(bytes_le=image[16:32])),
 'payload_record_type':'EFI variable name/vendor GUID/data record',
 'variable_name_size':name_size,'variable_data_size':data_size,
 'dell_core_sha256':hashlib.sha256(core).hexdigest(),'loader_protocol_references':references,
 'note':'References identify candidates for emulation; they do not prove every Dell-specific loading route absent.'}
(out/'offline-analysis.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

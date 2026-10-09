"""Locate bootstrap candidate references; offsets are evidence, not execution proof."""
import hashlib
import json
from pathlib import Path
import re
import struct
import uuid
repo=Path(__file__).resolve().parents[1];out=repo/'artifacts/research/independent-bootstrap'
survey=json.loads((out/'module-survey.json').read_text());results=[]
for module in survey['modules']:
 name=module['name'];body=(out/(name+'.bin')).read_bytes()
 assert hashlib.sha256(body).hexdigest()==module['sha256']
 text=(out/(name+'.txt')).read_text();pe=struct.unpack_from('<I',body,60)[0]
 section=pe+24+struct.unpack_from('<H',body,pe+20)[0]
 def rva(offset):
  for i in range(struct.unpack_from('<H',body,pe+6)[0]):
   _,_,address,size,start=struct.unpack_from('<8sIIII',body,section+40*i)
   if start<=offset<start+size:return address+offset-start
  raise ValueError('Offset outside PE section')
 targets={'http-prefix':b'http://\0','https-prefix':b'https://\0',
          'http-disabled-message':'HTTP is disabled'.encode('utf-16-le'),
          'loadfile-guid':uuid.UUID('56ec3091-954c-11d2-8e3f-00a0c969723b').bytes_le,
          'bootcurrent':'BootCurrent\0'.encode('utf-16-le')}
 references={}
 for label,value in targets.items():
  entries=[]
  for match in re.finditer(re.escape(value),body):
   address=rva(match.start())
   lines=[line.strip() for line in text.splitlines() if re.search(r'# 0x'+format(address,'x')+r'\b',line)]
   entries.append({'file_offset':hex(match.start()),'rva':hex(address),'references':lines})
  if entries:references[label]=entries
 results.append({'name':name,'references':references})
(out/'provider-references.json').write_text(json.dumps(results,indent=2))
print(json.dumps([m for m in results if m['name'] in ['http-boot-dxe','special-boot-stub']],indent=2))

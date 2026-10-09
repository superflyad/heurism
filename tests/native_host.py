"""Exercise actual C ELF validation and final-map handoff with hostile inputs."""
import argparse
import ctypes
import json
from pathlib import Path
import struct
import subprocess

repo=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,default=repo/'build/native')
a=p.parse_args(); out=a.build.resolve()
llvm=Path('C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/x64/bin')
objects=[]
for source in ['tests/native_host.c','tests/acpi_host.c','tests/pci_host.c','tests/i8042_host.c','drivers/input/i8042.c','platform/x86_64/pci.c','platform/x86_64/device_memory.c','platform/x86_64/acpi.c','platform/x86_64/acpi_memory.c','boot/elf.c','boot/kernel_handoff.c','kernel/pages.c','ui/framebuffer.c','common/memory.c']:
    obj=out/(source.replace('/','_')+'.host.obj');objects.append(obj)
    subprocess.run([str(llvm/'clang.exe'),'--target=x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-Wall','-Wextra','-Werror','-c',str(repo/source),'-o',str(obj)],check=True)
dll_path=out/'native-host.dll'
subprocess.run([str(llvm/'lld-link.exe'),'/dll','/noentry','/nodefaultlib','/machine:x64','/timestamp:0','/export:run_handoff_tests','/export:run_page_tests','/export:run_acpi_tests','/export:run_pci_tests','/export:run_i8042_tests','/export:elf_plan','/export:framebuffer_text','/export:framebuffer_rect','/out:'+str(dll_path),*map(str,objects)],check=True)
dll=ctypes.CDLL(str(dll_path));dll.run_handoff_tests.restype=ctypes.c_int
assert dll.run_handoff_tests()==0,'Handoff fixture failed'
assert dll.run_page_tests()==0,'Page allocator fixture failed'
acpi_result=dll.run_acpi_tests();assert acpi_result==0,'ACPI fixture failed at C line '+str(acpi_result)
pci_result=dll.run_pci_tests();assert pci_result==0,'PCI fixture failed at C line '+str(pci_result)

input_result=dll.run_i8042_tests();assert input_result==0,'i8042 fixture failed at C line '+str(input_result)

class Framebuffer(ctypes.Structure):
    _fields_=[('pixels',ctypes.POINTER(ctypes.c_uint32)),('width',ctypes.c_uint32),('height',ctypes.c_uint32),('stride',ctypes.c_uint32),('format',ctypes.c_uint32),('size',ctypes.c_uint64)]
dll.framebuffer_text.argtypes=[ctypes.POINTER(Framebuffer),ctypes.c_uint32,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_char_p,ctypes.c_uint32]
pixels=(ctypes.c_uint32*(12*14))();fb=Framebuffer(pixels,12,14,12,1,12*14*4)
for code in range(33,127):
    for i in range(len(pixels)):pixels[i]=0
    dll.framebuffer_text(ctypes.byref(fb),0,0,2,bytes([code]),0xffffff)
    assert any(pixels),'Printable character invisible: '+chr(code)

dll.elf_plan.argtypes=[ctypes.c_void_p,ctypes.c_uint64,ctypes.c_void_p];dll.elf_plan.restype=ctypes.c_int
def accepted(data):
    buffer=ctypes.create_string_buffer(bytes(data));plan=ctypes.create_string_buffer(4096)
    return bool(dll.elf_plan(buffer,len(data),plan))
kernel=(out/'kernel.elf').read_bytes();assert accepted(kernel)
cases={'truncated':kernel[:63],'oversized':b'\0'*(16*1024*1024+1)}
def mutate(name,offset,fmt,value):
    data=bytearray(kernel);struct.pack_into(fmt,data,offset,value);cases[name]=data
for name,offset,fmt,value in [('not_elf',0,'I',0),('wrong_arch',18,'H',183),('dynamic',16,'H',3),('bad_version',20,'I',2),('entry_outside',24,'Q',0),('ph_overflow',32,'Q',0xfffffffffffffff0),('ph_count',56,'H',65535),('ph_stride',54,'H',1)]:mutate(name,offset,fmt,value)
phoff=struct.unpack_from('<Q',kernel,32)[0]
for name,offset,fmt,value in [('interp',0,'I',3),('destination_low',24,'Q',0),('filesz_overflow',32,'Q',0xfffffffffffffff0),('file_offset_overflow',8,'Q',0xfffffffffffffff0),('mem_overflow',40,'Q',0xfffffffffffffff0),('align_not_power',48,'Q',3),('writable_code',4,'I',7),('entry_in_bss',32,'Q',0)]:mutate(name,phoff+offset,fmt,value)
# Set both physical and virtual addresses outside the allowed region.
outside=bytearray(kernel);struct.pack_into('<QQ',outside,phoff+16,0x1000,0x1000);cases['outside_destination']=outside
overlap=bytearray(kernel);overlap[phoff+56:phoff+112]=overlap[phoff:phoff+56];cases['overlapping_segments']=overlap
for name,data in cases.items():assert not accepted(data),name+' accepted'
# Actual PE image: entry belongs to executable section, relocation exists, no imports.
efi=(out/'companion-loader.efi').read_bytes();pe=struct.unpack_from('<I',efi,60)[0];opt=pe+24
assert efi[:2]==b'MZ' and efi[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',efi,pe+4)[0]==0x8664
assert struct.unpack_from('<H',efi,opt)[0]==0x20b and struct.unpack_from('<H',efi,opt+68)[0]==10
assert struct.unpack_from('<II',efi,opt+120)==(0,0)
assert all(struct.unpack_from('<II',efi,opt+152))
entry=struct.unpack_from('<I',efi,opt+16)[0];table=opt+struct.unpack_from('<H',efi,pe+20)[0]
assert any(struct.unpack_from('<I',efi,table+n*40+12)[0]<=entry<sum(struct.unpack_from('<II',efi,table+n*40+8)) and struct.unpack_from('<I',efi,table+n*40+36)[0]&0x20000000 for n in range(struct.unpack_from('<H',efi,pe+6)[0]))
result={'elf_rejected_cases':list(cases),'handoff_cases_passed':9,'page_invariants_passed':True,'acpi_invariants_passed':True,'pci_invariants_passed':True,'i8042_invariants_passed':True,'printable_glyphs_visible':True,'efi_image_checks_passed':True}
(out/'host-verification.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))

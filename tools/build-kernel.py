"""Build the freestanding ELF64 kernel and Companion EFI loader, without GRUB.

The output is isolated from management/recovery files. --vm-diagnostics opts
into emulated COM1 I/O; diagnostic builds are VM-only.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

repo=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--llvm',type=Path,default=Path('C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/x64/bin'))
parser.add_argument('--out',type=Path,default=repo/'build/native')
parser.add_argument('--vm-diagnostics',action='store_true')
parser.add_argument('--vm-network',action='store_true',help='Isolated e1000/UDP fixture; requires diagnostics; never deploy this public test key')
parser.add_argument('--vm-usb-network',action='store_true',help='Isolated CDC ECM USB Ethernet/public management fixture')
parser.add_argument('--vm-usb',action='store_true',help='Isolated QEMU xHCI descriptor inspection; requires diagnostics')
parser.add_argument('--keyboard',choices=['none','i8042'],help='Explicit platform input profile; VM diagnostics default to q35 i8042')
parser.add_argument('--stale-map-test',action='store_true')
parser.add_argument('--exception-test',action='store_true')
parser.add_argument('--pagefault-test',action='store_true')
args=parser.parse_args()
keyboard=args.keyboard or ('i8042' if args.vm_diagnostics else 'none')
if args.vm_usb_network and (not args.vm_diagnostics or not args.vm_usb or args.vm_network):parser.error('USB networking is VM-only and requires --vm-usb without --vm-network')
if args.vm_usb and not args.vm_diagnostics:parser.error('USB controller profile is VM-only; --vm-diagnostics required')
if args.vm_network and not args.vm_diagnostics:parser.error('Public-key-fixture networking is VM-only; --vm-diagnostics required')
if (args.stale_map_test or args.exception_test or args.pagefault_test) and not args.vm_diagnostics:parser.error('Fault injection is VM-only')
if args.exception_test and args.pagefault_test:parser.error('Choose one exception fixture')
out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
cc=args.llvm/'clang.exe'; pe_link=args.llvm/'lld-link.exe'; elf_link=args.llvm/'ld.lld.exe'
flags=['-std=c11','-ffreestanding','-fno-stack-protector','-fno-builtin','-mno-red-zone','-mgeneral-regs-only','-Wall','-Wextra','-Werror','-O2','-g0','-ffunction-sections','-fdata-sections']
def run(argv):subprocess.run([str(a) for a in argv],check=True)
def compile_sources(sources,target,suffix,extra=()):
    objects=[]
    for source in sources:
        obj=out/(source.replace('/','_')+suffix)
        run([cc,'--target='+target,*flags,*extra,'-c',repo/source,'-o',obj]);objects.append(obj)
    return objects
kernel=compile_sources(['kernel/main.c','kernel/pages.c','kernel/vm_network.c','kernel/vm_usb.c','kernel/vm_management.c','drivers/net/usb_ecm.c','drivers/usb/xhci.c','drivers/usb/descriptors.c','kernel/network.c','kernel/management.c','drivers/net/e1000.c','common/sha256.c','drivers/input/i8042.c','platform/x86_64/entry.S','platform/x86_64/serial.c','platform/x86_64/interrupts.c','platform/x86_64/isr.S','platform/x86_64/timer.c','platform/x86_64/paging.c','platform/x86_64/sparse_identity.c','platform/x86_64/acpi.c','platform/x86_64/acpi_memory.c','platform/x86_64/pci.c','platform/x86_64/device_memory.c','ui/framebuffer.c','common/memory.c'],'x86_64-unknown-none-elf','.o',['-fno-pic','-fno-pie','-mcmodel=small','-DCOMPANION_VM_USB_NETWORK='+str(int(args.vm_usb_network)),'-DCOMPANION_VM_USB='+str(int(args.vm_usb)),'-DCOMPANION_VM_NETWORK='+str(int(args.vm_network))])
run([elf_link,'-static','--gc-sections','--build-id=none','-z','max-page-size=4096','-T',repo/'kernel/linker.ld','-Map='+str(out/'kernel.map'),'-o',out/'kernel.elf',*kernel])
loader=compile_sources(['boot/kernel_loader.c','boot/kernel_handoff.c','boot/elf.c','boot/kernel_jump.S','boot/framebuffer_gop.c','ui/framebuffer.c','platform/x86_64/serial.c','common/memory.c'],'x86_64-pc-windows-msvc','.obj',
    ['-fshort-wchar','-DCOMPANION_VM_USB_NETWORK='+str(int(args.vm_usb_network)),'-DCOMPANION_VM_USB='+str(int(args.vm_usb)),'-DCOMPANION_VM_NETWORK='+str(int(args.vm_network)),'-DCOMPANION_VM_DIAGNOSTICS='+str(int(args.vm_diagnostics)),'-DCOMPANION_I8042='+str(int(keyboard=='i8042')),'-DCOMPANION_STALE_MAP_TEST='+str(int(args.stale_map_test)),'-DCOMPANION_KERNEL_TEST_FLAGS='+str(0x100 if args.exception_test else 0x200 if args.pagefault_test else 0)])
run([pe_link,'/subsystem:efi_application','/entry:efi_main','/nodefaultlib','/machine:x64','/dynamicbase','/timestamp:0','/opt:ref','/out:'+str(out/'companion-loader.efi'),*loader])
esp=out/'esp'; (esp/'EFI/BOOT').mkdir(parents=True,exist_ok=True);(esp/'EFI/Companion').mkdir(parents=True,exist_ok=True)
shutil.copyfile(out/'companion-loader.efi',esp/'EFI/BOOT/BOOTX64.EFI');shutil.copyfile(out/'kernel.elf',esp/'EFI/Companion/kernel.elf')
report={'vm_only':args.vm_diagnostics,'vm_network':args.vm_network,'vm_usb':args.vm_usb,'vm_usb_network':args.vm_usb_network,'keyboard':keyboard,'stale_map_test':args.stale_map_test,'exception_test':args.exception_test,'pagefault_test':args.pagefault_test,'files':{name:{'size':(out/name).stat().st_size,'sha256':hashlib.sha256((out/name).read_bytes()).hexdigest()} for name in ['kernel.elf','companion-loader.efi']}}
(out/'manifest.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

"""Execute the actual xHCI C driver with bounded hostile register/DMA models."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess

repo=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,default=repo/'build/native-usb');a=p.parse_args()
out=a.build.resolve();out.mkdir(parents=True,exist_ok=True)
llvm=Path('C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/x64/bin')
objects=[]
for source in ['drivers/usb/xhci.c','drivers/usb/descriptors.c','drivers/net/usb_ecm.c','tests/usb_host_exports.c','common/memory.c']:
    obj=out/(source.replace('/','_')+'.host.obj');objects.append(obj)
    subprocess.run([str(llvm/'clang.exe'),'--target=x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-Wall','-Wextra','-Werror','-c',str(repo/source),'-o',str(obj)],check=True)
exports=['xhci_start','xhci_stop','xhci_noop','xhci_inspect_port','xhci_open_port','xhci_close_port','xhci_control','xhci_configure_bulk','xhci_bulk_send','xhci_bulk_receive','xhci_bulk_tx_idle','usb_ecm_descriptor','usb_ecm_mac','usb_ecm_frame_boundary','usb_ecm_start','usb_device_descriptor','usb_configuration_descriptor','xhci_structure_size','xhci_io_size','usb_info_size']
library=out/'usb-host.dll'
subprocess.run([str(llvm/'lld-link.exe'),'/dll','/noentry','/nodefaultlib','/timestamp:0','/out:'+str(library),*['/export:'+s for s in exports],*map(str,objects)],check=True)
dll=C.CDLL(str(library))
Read=C.CFUNCTYPE(C.c_uint32,C.c_void_p,C.c_uint32)
Write=C.CFUNCTYPE(None,C.c_void_p,C.c_uint32,C.c_uint32)
Allocate=C.CFUNCTYPE(C.c_uint64,C.c_void_p)
Release=C.CFUNCTYPE(None,C.c_void_p,C.c_uint64)
Fence=C.CFUNCTYPE(None,C.c_void_p)
Master=C.CFUNCTYPE(None,C.c_void_p,C.c_uint8)
class Io(C.Structure):
    _fields_=[('context',C.c_void_p),('read',Read),('write',Write),('allocate',Allocate),('release',Release),('fence',Fence),('bus_master',Master),('poll_limit',C.c_uint32)]
class Bulk(C.Structure):
    _fields_=[(n,C.c_uint64) for n in ['ring','buffer','pointer']]+[(n,C.c_uint32) for n in ['next','requested','actual']]+[('max_packet',C.c_uint16)]+[(n,C.c_uint8) for n in ['dci','cycle','pending','done']]
class Endpoint(C.Structure):
    _fields_=[('address',C.c_uint8),('burst',C.c_uint8),('max_packet',C.c_uint16)]
class Ecm(C.Structure):
    _fields_=[('in_',Endpoint),('out',Endpoint),('max_frame',C.c_uint16)]+[(n,C.c_uint8) for n in ['configuration','control_interface','data_interface','alternate','mac_string']]
class Xhci(C.Structure):
    _fields_=[('io',Io)]+[(n,C.c_uint32) for n in ['op','runtime','doorbell','slots','ports','context_size','command_next','event_next','transfer_next']]+[(n,C.c_uint8) for n in ['command_cycle','event_cycle','transfer_cycle','running','failed']]+[(n,C.c_uint32) for n in ['allocated','commands','transfers','events','port_events','last_completion']]+[('pages',C.c_uint64*48)]+[(n,C.c_uint64) for n in ['dcbaa','command_ring','event_ring','erst','input','output','transfer_ring','buffer']]+[(n,C.c_uint32) for n in ['active_slot','active_port','active_speed']]+[('bulk_in',Bulk),('bulk_out',Bulk)]
class Info(C.Structure):
    _fields_=[(n,C.c_uint16) for n in ['vendor','product','usb_version','device_version','ep0_packet','configuration_bytes']]+[(n,C.c_uint8) for n in ['port','speed','device_class','configurations','interfaces']]
assert C.sizeof(Xhci)==dll.xhci_structure_size() and C.sizeof(Io)==dll.xhci_io_size() and C.sizeof(Info)==dll.usb_info_size()
dll.xhci_start.argtypes=[C.POINTER(Xhci),C.POINTER(Io)]
dll.xhci_stop.argtypes=dll.xhci_noop.argtypes=[C.POINTER(Xhci)]
dll.xhci_inspect_port.argtypes=[C.POINTER(Xhci),C.c_uint32,C.POINTER(Info)]
dll.xhci_open_port.argtypes=dll.xhci_inspect_port.argtypes
dll.xhci_close_port.argtypes=dll.xhci_bulk_tx_idle.argtypes=[C.POINTER(Xhci)]
dll.xhci_control.argtypes=[C.POINTER(Xhci),C.c_uint8,C.c_uint8,C.c_uint16,C.c_uint16,C.c_void_p,C.c_uint32]
dll.xhci_configure_bulk.argtypes=[C.POINTER(Xhci),C.POINTER(Endpoint),C.POINTER(Endpoint)]
dll.xhci_bulk_send.argtypes=[C.POINTER(Xhci),C.c_void_p,C.c_uint32]
dll.xhci_bulk_receive.argtypes=[C.POINTER(Xhci),C.c_void_p,C.c_uint32,C.POINTER(C.c_uint32)]
dll.usb_ecm_descriptor.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.POINTER(Ecm)]
dll.usb_ecm_mac.argtypes=[C.c_void_p,C.c_uint32,C.c_void_p]
dll.usb_ecm_frame_boundary.argtypes=[C.POINTER(C.c_uint8),C.c_uint32]
dll.usb_ecm_start.argtypes=[C.POINTER(Xhci),C.POINTER(Info),C.c_void_p]
dll.usb_device_descriptor.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.POINTER(Info)]
dll.usb_configuration_descriptor.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint8)]
# A real low-address host arena lets the same DMA pointer checks execute in C.
win=C.WinDLL('kernel32',use_last_error=True)
win.VirtualAlloc.argtypes=[C.c_void_p,C.c_size_t,C.c_uint32,C.c_uint32];win.VirtualAlloc.restype=C.c_void_p
win.VirtualFree.argtypes=[C.c_void_p,C.c_size_t,C.c_uint32];win.VirtualFree.restype=C.c_int
arena=None
for hint in [0x10000000,0x20000000,0x30000000,0x40000000]:
    arena=win.VirtualAlloc(hint,48*4096,0x3000,4)
    if arena:break
if not arena:raise RuntimeError('Could not reserve low DMA fixture arena')
def u64(address):return C.c_uint64.from_address(address).value
def u32(address):return C.c_uint32.from_address(address).value
class Model:
    def __init__(self,mode='normal',fail_alloc=0,scratch=0,context_size=32,speed=1,in_dci=5,out_dci=4,packet=64,burst=0,ecm_fixture=None,fail_control_at=0):
        self.mode=mode;self.fail_alloc=fail_alloc;self.allocated=set();self.allocations=0;self.releases=0;self.enabled=False;self.errors=[];self.reads=0;self.writes=[];self.event_index=0;self.event_cycle=1;self.command_index=0;self.command_cycle=1
        self.context_size=context_size;self.speed=speed;self.in_dci=in_dci;self.out_dci=out_dci;self.packet=packet;self.burst=burst
        self.ecm_fixture=ecm_fixture;self.control_requests=[];self.fail_control_at=fail_control_at
        self.registers={0:0x1000040,4:8|(1<<8)|(8<<24),8:((scratch&31)<<27)|((scratch&0x3e0)<<16),16:0,20:0x2000,24:0x1000,0x40:0,0x44:1,0x48:1}
        if context_size==64:self.registers[16]|=4
        self.callbacks=[Read(self.read),Write(self.write),Allocate(self.allocate),Release(self.release),Fence(lambda _:None),Master(self.master)]
        self.io=Io(None,*self.callbacks,32)
    def check(self,condition,message):
        if not condition:self.errors.append(message)
    def read(self,context,offset):
        self.reads+=1;self.check(offset%4==0 and offset<0x4000,'read outside controller')
        if self.mode=='not-ready' and offset==0x44:return 1|(1<<11)
        if self.mode=='host-error' and offset==0x44 and self.enabled:return (1<<12)|(self.registers.get(offset,0)&1)
        return self.registers.get(offset,0)
    def write(self,context,offset,value):
        self.check(offset%4==0 and offset<0x4000,'write outside controller');self.writes.append((offset,value));self.registers[offset]=value
        if offset==0x40:
            if value&2:
                if self.mode!='reset-stuck':self.registers[0x40]=0
                self.registers[0x44]=1
            elif value&1:self.registers[0x44]=1 if self.mode=='run-stuck' else 0
            elif self.mode!='halt-stuck':self.registers[0x44]=1
        if offset==0x1034:
            self.check(self.enabled,'ERST fetched before DMA enabled')
            address=(self.registers.get(0x1034,0)<<32)|self.registers.get(0x1030,0)
            self.check(address in self.allocated and u32(address+8)==64,'invalid event table')
        if offset==0x440:
            self.registers[offset]=1|2|(self.speed<<10)|(1<<9)
        if offset==0x2004 and value!=1:
            self.bulk(value)
        if offset==0x2004 and value==1:
            if self.mode=='transfer-timeout':return
            output=u64(self.registers[0x70]+8)
            transfer=u32(output+self.context_size+8)&~15
            index=self.transfer_index
            setup=transfer+index*16;data=setup+16;status=data+16
            immediate=u64(setup);size=(immediate>>48)&0xffff
            request=(immediate&255,(immediate>>8)&255,(immediate>>16)&65535,(immediate>>32)&65535,size)
            self.control_requests.append(request)
            if len(self.control_requests)==self.fail_control_at:
                self.event(status,32,6,1,1);return
            if not size:status=data
            self.transfer_index+=3 if size else 2
            self.check(((u32(setup+12)>>10)&63)==2 and (u32(setup+12)&1),'setup not published')
            self.check((not size or ((u32(data+12)>>10)&63)==3) and ((u32(status+12)>>10)&63)==4,'data/status order')
            kind=(immediate>>24)&0xff;destination=u64(data)
            descriptor=bytes.fromhex('12010002000000402505a2a4000101020301') if kind==1 else bytes.fromhex('090222000101008032090400000103010100092111010001223f000705810308000a')
            if kind==1 and self.speed==4:descriptor=bytes.fromhex('120100030000000957230106003001020602')
            if self.ecm_fixture and request[:2]==(0x80,6):
                if kind==1:descriptor=bytes.fromhex(self.ecm_fixture['device_hex'])
                elif kind==2:descriptor=bytes.fromhex(self.ecm_fixture['configuration_hex'][request[2]&255])
                elif kind==3:
                    self.check(request[2]==0x303 and request[3]==0x409,'incorrect ECM MAC string request')
                    descriptor=bytes([26,3])+'7cc2c61db2f5'.encode('utf-16le')
                else:self.errors.append('unexpected descriptor type')
            if self.ecm_fixture and not size:
                self.check(request in [(0,9,2,0,0),(1,11,1,1,0),(0x21,0x43,12,0,0)],'unexpected ECM setup control')
            if self.mode=='malformed-descriptor':descriptor=bytes([0])+descriptor[1:]
            if size:
                self.check(size<=len(descriptor) and u32(data+8)==size,'incorrect descriptor request length')
                C.memmove(destination,descriptor[:size],size)
            pointer=data if self.mode=='short-transfer' else status
            code=13 if self.mode=='short-transfer' else 6 if self.mode=='transfer-error' else 1
            endpoint=2 if self.mode=='wrong-endpoint' else 1
            self.event(pointer,32,code,1,endpoint,1 if self.mode=='short-transfer' else 0)
        if offset==0x2000:
            if self.mode=='timeout':return
            self.check(self.enabled and not(self.registers[0x44]&1),'doorbell before run/DMA')
            ring=((self.registers.get(0x5c,0)<<32)|self.registers.get(0x58,0))&~63
            address=ring+self.command_index*16;control=u32(address+12)
            if (control>>10)&63==6:
                self.check(bool(control&2) and u64(address)==ring,'invalid link TRB');self.command_index=0;self.command_cycle^=1;address=ring;control=u32(address+12)
            kind=(control>>10)&63;slot=0
            self.check((control&1)==self.command_cycle and kind in [9,10,11,12,13,23],'command ownership/type')
            if kind==9:slot=1
            if kind in [10,11,12,13]:slot=control>>24;self.check(slot==1,'slot mismatch')
            if kind==11:
                input=u64(address);output=u64(self.registers[0x70]+slot*8)
                stride=self.context_size;packet=512 if self.speed==4 else 64 if self.speed==3 else 8
                self.check(u32(input+4)==3 and u32(input+stride)==((self.speed<<20)|(1<<27)) and u32(input+stride+4)==(1<<16),'invalid address context')
                self.check(((u32(input+stride*2+4)>>3)&7)==4 and u32(input+stride*2+4)>>16==packet,'invalid initial EP0 context')
                C.memmove(output,input+stride,stride*2);self.transfer_index=0
            if kind==12:
                input=u64(address);output=u64(self.registers[0x70]+slot*8)
                stride=self.context_size
                self.check(u32(input+4)==(1|(1<<self.in_dci)|(1<<self.out_dci)) and u32(input+stride)>>27==max(self.in_dci,self.out_dci),'bulk context selection')
                for dci in [self.out_dci,self.in_dci]:
                    context=input+(dci+1)*stride;word=u32(context+4)
                    self.check((word>>16)==self.packet and ((word>>3)&7)==(2 if dci==self.out_dci else 6) and ((word>>8)&255)==self.burst,'bulk packet/type/burst')
                    self.check(u32(context+16)==2048,'bulk average TRB length')
                    if stride==64:self.check(C.string_at(context+32,32)==bytes(32),'64-byte context reserved tail')
                    C.memmove(output+dci*stride,context,stride)
                self.bulk_index={self.out_dci:0,self.in_dci:0};self.bulk_cycle={self.out_dci:1,self.in_dci:1}
            pointer=address+16 if self.mode=='wrong-pointer' else address
            event_kind=32 if self.mode=='wrong-event' else 33;code=5 if self.mode=='command-error' else 1
            self.event(pointer,event_kind,code,slot);self.command_index+=1
    def bulk(self,dci):
        self.check(dci in [self.out_dci,self.in_dci],'unexpected bulk DCI')
        output=u64(self.registers[0x70]+8);ring=u32(output+dci*self.context_size+8)&~15
        index=self.bulk_index[dci];pointer=ring+index*16;control=u32(pointer+12)
        if (control>>10)&63==6:
            self.check(u64(pointer)==ring and (control&2),'bulk link')
            index=0;self.bulk_cycle[dci]^=1;pointer=ring;control=u32(pointer+12)
        self.check((control&1)==self.bulk_cycle[dci] and ((control>>10)&63)==1,'bulk TRB ownership/type')
        if self.mode=='bulk-nak':return
        requested=u32(pointer+8)&0x1ffff;buffer=u64(pointer)
        if dci==self.out_dci and not(control&32):
            self.check(requested and requested%self.packet==0 and not(control&16),'ZLP data must be separate TD')
            index+=1;pointer=ring+index*16;control=u32(pointer+12)
            if (control>>10)&63==6:
                index=0;self.bulk_cycle[dci]^=1;pointer=ring;control=u32(pointer+12)
            self.check((control&1)==self.bulk_cycle[dci] and ((control>>10)&63)==1 and u32(pointer+8)==0 and (control&32),'missing terminating ZLP TD')
            requested=0
        actual=min(requested,17) if dci==self.in_dci else requested
        if dci==self.in_dci and self.mode=='bulk-zero':actual=0
        if dci==self.in_dci and self.mode=='bulk-full':actual=requested
        if dci==self.in_dci:C.memmove(buffer,b'usb receive bytes'[:actual].ljust(actual,b'!'),actual)
        residual=requested-actual
        if self.mode=='bulk-bad-residual':residual=requested+1
        code=6 if self.mode=='bulk-error' else 13 if dci==self.in_dci and residual else 1
        endpoint=3 if self.mode=='bulk-wrong-endpoint' else dci
        event_pointer=pointer+16 if self.mode=='bulk-wrong-pointer' else pointer
        self.bulk_index[dci]=index+1;self.event(event_pointer,32,code,1,endpoint,residual)
    def event(self,pointer,kind,code,slot=0,endpoint=0,residual=0):
        table=(self.registers.get(0x1034,0)<<32)|self.registers.get(0x1030,0);event=u64(table)+self.event_index*16
        C.memmove(event,struct.pack('<QII',pointer,(code<<24)|residual,(slot<<24)|(endpoint<<16)|(kind<<10)|self.event_cycle),16)
        self.event_index+=1
        if self.event_index==64:self.event_index=0;self.event_cycle^=1
    def allocate(self,context):
        self.allocations+=1
        if self.allocations==self.fail_alloc:return 0
        for index in range(48):
            page=arena+index*4096
            if page not in self.allocated:self.allocated.add(page);return page
        return 0
    def release(self,context,page):
        self.check(not self.enabled,'DMA memory freed while bus master enabled')
        self.check(page in self.allocated,'foreign/double release')
        self.allocated.discard(page);self.releases+=1
    def master(self,context,enable):
        if enable:self.check(len(self.allocated)>=8 and bool(self.registers.get(0x70)) and bool(self.registers.get(0x58)),'DMA before owned rings')
        self.enabled=bool(enable)
    def verify(self):assert not self.errors,self.errors

cases=[];descriptor_cases=0
try:
    model=Model();x=Xhci();assert dll.xhci_start(C.byref(x),C.byref(model.io))
    for _ in range(200):assert dll.xhci_noop(C.byref(x))
    assert x.commands==x.events==200 and model.command_cycle==x.command_cycle and model.event_cycle==x.event_cycle
    assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify();cases.append('command/event rings wrap three times')
    for mode in ['normal','transfer-timeout','short-transfer','transfer-error','wrong-endpoint','malformed-descriptor']:
        model=Model(mode);model.registers[0x440]=1|(1<<10)|(1<<9);x=Xhci();info=Info()
        assert dll.xhci_start(C.byref(x),C.byref(model.io))
        result=dll.xhci_inspect_port(C.byref(x),1,C.byref(info))
        if mode=='normal':assert result==1 and info.vendor==0x525 and info.product==0xa4a2 and info.ep0_packet==64 and x.transfers==4
        else:assert result==-1 and x.failed and not info.vendor
        assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify();cases.append('control '+mode)
    def open_bulk(mode='normal',fail_alloc=0):
        model=Model(fail_alloc=fail_alloc);model.registers[0x440]=1|(1<<10)|(1<<9);x=Xhci();info=Info()
        assert dll.xhci_start(C.byref(x),C.byref(model.io)) and dll.xhci_open_port(C.byref(x),1,C.byref(info))==1
        model.mode=mode
        return model,x
    in_ep=Endpoint(0x82,0,64);out_ep=Endpoint(2,0,64)
    model,x=open_bulk();assert dll.xhci_configure_bulk(C.byref(x),C.byref(in_ep),C.byref(out_ep))
    assert not dll.xhci_configure_bulk(C.byref(x),C.byref(in_ep),C.byref(out_ep)) # no active-ring replacement
    buffer=C.create_string_buffer(2048);size=C.c_uint32()
    assert not dll.xhci_bulk_receive(C.byref(x),buffer,len(buffer),C.byref(size))
    for n in range(200):
        packet=C.create_string_buffer(bytes([n])* (64 if n%3==0 else 65))
        assert dll.xhci_bulk_send(C.byref(x),packet,len(packet.raw)-1) and dll.xhci_bulk_tx_idle(C.byref(x))
        assert dll.xhci_bulk_receive(C.byref(x),buffer,len(buffer),C.byref(size)) and size.value==17 and buffer.raw[:17]==b'usb receive bytes'
    assert not dll.xhci_close_port(C.byref(x)) # pending receive owns its DMA
    assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify();cases.append('bulk rings wrap with short IN and separate ZLP OUT TDs')
    for stride in [32,64]:
        for speed,packet,burst in [(3,512,0),(4,1024,3)]:
            model=Model(context_size=stride,speed=speed,in_dci=3,out_dci=4,packet=packet,burst=burst)
            model.registers[0x440]=1|(speed<<10)|(1<<9);x=Xhci();info=Info()
            assert dll.xhci_start(C.byref(x),C.byref(model.io)) and dll.xhci_open_port(C.byref(x),1,C.byref(info))==1
            endpoint_in=Endpoint(0x81,burst,packet);endpoint_out=Endpoint(2,burst,packet)
            assert dll.xhci_configure_bulk(C.byref(x),C.byref(endpoint_in),C.byref(endpoint_out))
            assert not dll.xhci_bulk_receive(C.byref(x),buffer,len(buffer),C.byref(size))
            for n in range(70):
                payload=C.create_string_buffer(bytes([n])*(packet if n%2==0 else 65))
                assert dll.xhci_bulk_send(C.byref(x),payload,len(payload.raw)-1) and dll.xhci_bulk_tx_idle(C.byref(x))
                assert dll.xhci_bulk_receive(C.byref(x),buffer,len(buffer),C.byref(size)) and size.value==17
            assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify()
            cases.append('bulk context '+str(stride)+' speed '+str(speed)+' EP1 IN/EP2 OUT wrap')
    for mode,wanted in [('bulk-zero',0),('bulk-full',2048)]:
        model,x=open_bulk(mode);assert dll.xhci_configure_bulk(C.byref(x),C.byref(in_ep),C.byref(out_ep))
        assert not dll.xhci_bulk_receive(C.byref(x),buffer,len(buffer),C.byref(size))
        assert dll.xhci_bulk_receive(C.byref(x),buffer,len(buffer),C.byref(size)) and size.value==wanted and x.bulk_in.pending
        assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify();cases.append(mode)
    model,x=open_bulk();assert dll.xhci_configure_bulk(C.byref(x),C.byref(in_ep),C.byref(out_ep))
    guard=C.create_string_buffer(b'unchanged')
    assert not dll.xhci_bulk_receive(C.byref(x),guard,8,C.byref(size))
    assert not dll.xhci_bulk_receive(C.byref(x),guard,8,C.byref(size)) and x.failed and guard.value==b'unchanged'
    assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify();cases.append('receive capacity checked before copy')
    for mode in ['bulk-error','bulk-bad-residual','bulk-wrong-endpoint','bulk-wrong-pointer']:
        model,x=open_bulk(mode);assert dll.xhci_configure_bulk(C.byref(x),C.byref(in_ep),C.byref(out_ep))
        assert not dll.xhci_bulk_receive(C.byref(x),buffer,len(buffer),C.byref(size))
        assert not dll.xhci_bulk_receive(C.byref(x),buffer,len(buffer),C.byref(size)) and x.failed and x.allocated==12
        assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify();cases.append(mode)
    model,x=open_bulk('bulk-nak');assert dll.xhci_configure_bulk(C.byref(x),C.byref(in_ep),C.byref(out_ep))
    assert not dll.xhci_bulk_receive(C.byref(x),buffer,len(buffer),C.byref(size)) and x.bulk_in.pending
    before=len(model.writes)
    for _ in range(50):assert not dll.xhci_bulk_receive(C.byref(x),buffer,len(buffer),C.byref(size)) and not x.failed
    assert len(model.writes)==before # no new TD/doorbell while a NAK is pending
    assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify();cases.append('NAK pending with bounded nonblocking poll')
    for fail in range(9,13):
        model,x=open_bulk(fail_alloc=fail);assert not dll.xhci_configure_bulk(C.byref(x),C.byref(in_ep),C.byref(out_ep)) and x.failed
        assert model.allocated and not model.releases
        assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify();cases.append('bulk allocation failure '+str(fail))
    for ep in [Endpoint(0x80,0,64),Endpoint(0x12,0,64),Endpoint(0x82,1,64),Endpoint(0x82,0,65)]:
        model,x=open_bulk();before=len(model.writes)
        assert not dll.xhci_configure_bulk(C.byref(x),C.byref(ep),C.byref(out_ep)) and len(model.writes)==before and not x.failed
        assert dll.xhci_close_port(C.byref(x)) and dll.xhci_stop(C.byref(x));model.verify();cases.append('invalid bulk endpoint '+str((ep.address,ep.burst,ep.max_packet)))
    for mode in ['timeout','wrong-pointer','wrong-event','command-error','host-error']:
        model=Model(mode);x=Xhci();assert dll.xhci_start(C.byref(x),C.byref(model.io))
        before=model.reads;assert not dll.xhci_noop(C.byref(x)) and x.failed and model.reads-before<=32
        assert not dll.xhci_noop(C.byref(x)) and dll.xhci_stop(C.byref(x)) and not model.allocated
        model.verify();cases.append(mode)
    for mode in ['not-ready','reset-stuck','run-stuck']:
        model=Model(mode);x=Xhci();assert not dll.xhci_start(C.byref(x),C.byref(model.io))
        assert not model.enabled and not model.allocated and model.reads<200;model.verify();cases.append(mode)
    model=Model();x=Xhci();assert dll.xhci_start(C.byref(x),C.byref(model.io));model.mode='halt-stuck'
    assert not dll.xhci_stop(C.byref(x)) and x.allocated==8 and len(model.allocated)==8 and not model.enabled and model.releases==0
    model.mode='normal';assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify();cases.append('uncertain halt retains DMA')
    for fail in range(1,9):
        model=Model(fail_alloc=fail);x=Xhci();assert not dll.xhci_start(C.byref(x),C.byref(model.io)) and not model.allocated and not model.enabled
        model.verify();cases.append('allocation failure '+str(fail))
    for scratch in [1,31,32]:
        model=Model(scratch=scratch);x=Xhci();assert dll.xhci_start(C.byref(x),C.byref(model.io))
        assert x.allocated==9+scratch and u64(x.dcbaa) in model.allocated
        for index in range(scratch):assert u64(u64(x.dcbaa)+index*8) in model.allocated
        assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify();cases.append('scratchpads '+str(scratch))
    mutations=[(0,0xffffffff),(0,0x1000011),(4,0),(4,8|(1<<8)|(33<<24)),(8,2<<21),(20,3),(20,0x3ffc),(24,0x3fe0),(24,0x2020),(16,0x40000000),(16,0x00800000)]
    for offset,value in mutations:
        model=Model();model.registers[offset]=value
        if offset==16 and value==0x00800000:model.registers[0x200]=1|(1<<16)
        x=Xhci();assert not dll.xhci_start(C.byref(x),C.byref(model.io)) and not model.writes and not model.allocated
        model.verify();cases.append('invalid capability '+hex(offset)+'/'+hex(value))
    # Independent USB descriptor byte fixtures, not generated by driver code.
    device=bytearray.fromhex('12010002000000402505a2a4000101020301')
    def device_ok(data,speed=1):
        data=C.create_string_buffer(bytes(data));info=Info();return bool(dll.usb_device_descriptor(data,len(data.raw)-1,speed,C.byref(info)))
    assert device_ok(device) and device_ok(device,3)
    for speed,packet,version in [(1,8,0x200),(2,8,0x110),(3,64,0x200),(4,9,0x300)]:
        data=device.copy();data[7]=packet;struct.pack_into('<H',data,2,version);assert device_ok(data,speed)
    for length in range(18):assert not device_ok(device[:length]);descriptor_cases+=1
    for index,value in [(0,17),(1,2),(7,0),(7,9),(17,0)]:
        data=device.copy();data[index]=value;assert not device_ok(data);descriptor_cases+=1
    assert not device_ok(device,0) and not device_ok(device,5)
    config=bytearray.fromhex('090222000101008032090400000103010100092111010001223f000705810308000a')
    def config_ok(data):
        buf=C.create_string_buffer(bytes(data));interfaces=C.c_uint8();return bool(dll.usb_configuration_descriptor(buf,len(data),C.byref(interfaces)))
    assert len(config)==34 and config_ok(config)
    for length in range(len(config)):assert not config_ok(config[:length]);descriptor_cases+=1
    for index,value in [(0,8),(1,1),(2,33),(4,0),(4,2),(5,0),(7,0),(9,0),(9,1),(9,250),(11,1),(12,1),(13,0),(27,0),(27,8),(29,0x80),(29,0x91),(30,0),(31,0)]:
        data=config.copy();data[index]=value;assert not config_ok(data),(index,value);descriptor_cases+=1
    # Arbitrary length/type fuzz stays within a capped caller-owned byte array.
    rng=random.Random(193)
    for _ in range(2000):
        data=bytes(rng.randrange(256) for _ in range(rng.randrange(0,1100)));assert not config_ok(data);descriptor_cases+=1
    # Independent CDC ECM descriptors with interface 1 alternate 0/1 and EP2 IN/OUT.
    ecm=bytearray.fromhex('09025000020107c032090400000102060005052400100105240600010d240f0300000000ea050000000705810310002009040100000a00000009040101020a0000040705820240000007050202400000')
    def ecm_ok(data,speed=1):
        desc=Ecm();buf=C.create_string_buffer(bytes(data));return bool(dll.usb_ecm_descriptor(buf,len(data),speed,C.byref(desc))),desc
    good,desc=ecm_ok(ecm);assert good and desc.configuration==1 and desc.alternate==1 and desc.mac_string==3 and desc.in_.address==0x82 and desc.out.address==2 and desc.max_frame==1514
    for length in range(len(ecm)):assert not ecm_ok(ecm[:length])[0];descriptor_cases+=1
    for index,value in [(15,2),(26,1),(30,0),(31,0),(36,255),(37,255),(49,2),(75,0x82),(70,65),(70,0)]:
        data=ecm.copy();data[index]=value;assert not ecm_ok(data)[0],(index,value);descriptor_cases+=1
    assert not ecm_ok(ecm,2)[0] and not ecm_ok(ecm,3)[0] and not ecm_ok(ecm,4)[0]
    # Captured physical descriptors, not generated from the parser under test.
    fixture_path=repo/'tests/fixtures/dell-realtek-usb.json';fixture=json.loads(fixture_path.read_text())
    captured=[bytes.fromhex(h) for h in fixture['configuration_hex']]
    raw=bytes.fromhex(fixture['device_hex'])+b''.join(captured)
    assert hashlib.sha256(raw).hexdigest()==fixture['raw_sha256'] and device_ok(raw[:18],4)
    assert not ecm_ok(captured[0],4)[0] # current vendor configuration is not ECM
    good,dell=ecm_ok(captured[1],4)
    assert good and (dell.configuration,dell.control_interface,dell.data_interface,dell.alternate)==(2,0,1,1)
    assert (dell.in_.address,dell.out.address,dell.in_.max_packet,dell.out.max_packet,dell.in_.burst,dell.out.burst)==(0x81,2,1024,1024,3,3)
    expected_setup=[(0x80,6,0x200,0,9),(0x80,6,0x200,0,57),(0x80,6,0x201,0,9),(0x80,6,0x201,0,98),
                    (0x80,6,0x303,0x409,26),(0,9,2,0,0),(1,11,1,1,0),(0x21,0x43,12,0,0)]
    for fail in range(9):
        model=Model(context_size=64,speed=4,in_dci=3,out_dci=4,packet=1024,burst=3,ecm_fixture=fixture,fail_control_at=4+fail if fail else 0)
        model.registers[0x440]=1|(4<<10)|(1<<9);x=Xhci();info=Info()
        assert dll.xhci_start(C.byref(x),C.byref(model.io)) and dll.xhci_open_port(C.byref(x),1,C.byref(info))==1
        output_mac=C.create_string_buffer(6)
        result=dll.usb_ecm_start(C.byref(x),C.byref(info),output_mac)
        assert result==(0 if fail else 1)
        assert model.control_requests[4:]==expected_setup[:fail] if fail else model.control_requests[4:]==expected_setup
        if not fail:assert output_mac.raw==bytes.fromhex('7cc2c61db2f5')
        assert dll.xhci_stop(C.byref(x)) and not model.allocated;model.verify()
        cases.append('captured SuperSpeed ECM setup '+('control failure '+str(fail) if fail else 'success'))
    ss=bytearray(captured[1]);ss_rejected=0
    for length in range(len(ss)):
        assert not ecm_ok(ss[:length],4)[0];ss_rejected+=1
    for index,value in [(48,5),(49,0x31),(50,16),(51,1),(51,2),(51,3),(52,255),(53,255),(79,5),(80,0x31),(81,16),(82,1),(83,1),(84,1),(92,5),(94,16),(95,1),(96,1),(97,1),(20,1),(21,0),(36,0)]:
        bad=ss.copy();bad[index]=value;assert not ecm_ok(bad,4)[0],(index,value);ss_rejected+=1
    # Remove or displace an endpoint companion while preserving wTotalLength.
    for start in [48,79,92]:
        bad=ss[:start]+ss[start+6:];struct.pack_into('<H',bad,2,len(bad))
        assert not ecm_ok(bad,4)[0];ss_rejected+=1
    bad=ss[:79]+ss[85:92]+ss[79:85]+ss[92:]
    assert config_ok(bad) # structurally valid; wrong companion adjacency
    assert not ecm_ok(bad,4)[0];ss_rejected+=1
    descriptor_cases+=ss_rejected
    string=bytearray([26,3])+bytearray('525400123456'.encode('utf-16le'));mac=C.create_string_buffer(6)
    assert dll.usb_ecm_mac(C.create_string_buffer(bytes(string)),26,mac) and mac.raw==bytes.fromhex('525400123456')
    for index,value in [(0,25),(1,1),(2,ord('Z')),(3,1),(4,ord('3'))]:
        data=string.copy();data[index]=value;assert not dll.usb_ecm_mac(C.create_string_buffer(bytes(data)),26,mac);descriptor_cases+=1
    # An oversized transfer's signed-looking tail must never become a new
    # frame. QEMU caps frames at 2048, so multi-TD draining is a host C test.
    boundary_cases=0
    for lengths,accepted,states in [
        ([60,1514,0],[1,1,0],[0,0,0]),
        ([1536,122],[0,1],[0,0]),
        ([2048,122,122],[0,0,1],[1,0,0]),
        ([2048,2048,122,122],[0,0,0,1],[1,1,0,0]),
        ([2048,0,122],[0,0,1],[1,0,0])]:
        discard=C.c_uint8()
        for length,wanted,state in zip(lengths,accepted,states):
            assert dll.usb_ecm_frame_boundary(C.byref(discard),length)==wanted and discard.value==state
            boundary_cases+=1
    assert not dll.usb_ecm_frame_boundary(None,60)
    report={'passed':True,'controller_cases':cases,'descriptor_rejected_cases':descriptor_cases,'frame_boundary_cases':boundary_cases,
            'dell_captured_ecm_configuration_verified':True,'superspeed_invalid_cases':ss_rejected,'dell_fixture_sha256':hashlib.sha256(fixture_path.read_bytes()).hexdigest(),
            'harness_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'source_sha256':{name:hashlib.sha256((repo/name).read_bytes()).hexdigest() for name in ['drivers/usb/xhci.c','drivers/usb/descriptors.c','drivers/usb/xhci.h','drivers/net/usb_ecm.c','drivers/net/usb_ecm.h']},
            'scope':'Host register/DMA model; QEMU transfers tested separately; not physical xHCI proof'}
    (out/'usb-host-verification.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
finally:assert win.VirtualFree(arena,0,0x8000)

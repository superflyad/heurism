"""Exercise actual native packet/auth C code against independent Python oracles."""
import argparse
import ctypes as C
import hashlib
import hmac
import json
from pathlib import Path
import random
import struct
import subprocess
from network_wire import KEY,MAC,IP,arp,ping,udp,request,parse_ipv4,verify_response,checksum

repo=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,default=repo/'build/native-network');args=p.parse_args()
out=args.build.resolve();out.mkdir(parents=True,exist_ok=True)
llvm=Path('C:/Program Files/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/x64/bin');objects=[]
for source in ['common/sha256.c','common/memory.c','kernel/management.c','kernel/network.c']:
    obj=out/(source.replace('/','_')+'.host.obj');objects.append(obj)
    subprocess.run([str(llvm/'clang.exe'),'--target=x86_64-pc-windows-msvc','-std=c11','-ffreestanding','-fno-builtin','-fno-stack-protector','-O2','-Wall','-Wextra','-Werror','-c',str(repo/source),'-o',str(obj)],check=True)
path=out/'network-host.dll'
subprocess.run([str(llvm/'lld-link.exe'),'/dll','/noentry','/nodefaultlib','/machine:x64','/timestamp:0','/export:sha256','/export:hmac_sha256','/export:management_init','/export:network_reply','/out:'+str(path),*map(str,objects)],check=True)
dll=C.CDLL(str(path));dll.sha256.argtypes=[C.c_void_p,C.c_uint32,C.c_void_p];dll.hmac_sha256.argtypes=[C.c_void_p,C.c_uint32,C.c_void_p,C.c_uint32,C.c_void_p]
rng=random.Random(7506);crypto_cases=0
for length in [0,1,3,55,56,57,63,64,65,127,128,129,1000,1024,65536,1000000]:
    data=b'a'*length if length==1000000 else rng.randbytes(length);digest=C.create_string_buffer(32)
    dll.sha256(data,len(data),digest);assert digest.raw==hashlib.sha256(data).digest();crypto_cases+=1
    for key_length in [0,1,31,32,63,64,65,128,256]:
        key=rng.randbytes(key_length);dll.hmac_sha256(key,len(key),data,len(data),digest)
        assert digest.raw==hmac.digest(key,data,'sha256');crypto_cases+=1

class Management(C.Structure):
    _fields_=[('key',C.c_ubyte*32),('boot_nonce',C.c_ubyte*16),('last_sequence',C.c_uint64),('accepted',C.c_uint64),('rejected',C.c_uint64),('ready',C.c_ubyte)]
class Network(C.Structure):
    _fields_=[('mac',C.c_ubyte*6),('ip',C.c_ubyte*4),('management',Management),('accepted',C.c_uint64),('dropped',C.c_uint64)]
class Status(C.Structure):_fields_=[('ticks',C.c_uint64),('free_pages',C.c_uint64),('received',C.c_uint64),('transmitted',C.c_uint64)]
dll.management_init.argtypes=[C.POINTER(Management),C.c_void_p,C.c_void_p]
dll.network_reply.argtypes=[C.POINTER(Network),C.c_void_p,C.c_uint32,C.POINTER(Status),C.c_void_p,C.c_uint32,C.POINTER(C.c_ubyte)];dll.network_reply.restype=C.c_uint32
nonce=bytes(range(1,17));challenge=bytes(range(17,33));status=Status(123,456,7,8)
def fresh(boot=nonce):
    n=Network();n.mac[:]=MAC;n.ip[:]=IP;assert dll.management_init(C.byref(n.management),KEY,boot)==1;return n
def reply(n,packet,capacity=1514):
    buf=C.create_string_buffer(b'\xcc'*1530,1530);action=C.c_ubyte(99)
    size=dll.network_reply(C.byref(n),packet,len(packet),C.byref(status),buf,capacity,C.byref(action))
    assert buf.raw[1514:]==b'\xcc'*16
    return buf.raw[:size],action.value
n=fresh();data,action=reply(n,arp());assert len(data)==42 and action==0 and data[20:22]==b'\0\2'
data,action=reply(n,ping(5));proto,icmp=parse_ipv4(data);assert proto==1 and checksum(icmp)==0 and icmp[0]==0 and action==0
data,_=reply(n,udp(request(1,0,b'\0'*16,challenge)));_,values=verify_response(data,1,0,challenge,nonce);assert values==(123,456,7,8,0)
status_packet=udp(request(2,1,nonce,challenge));data,action=reply(n,status_packet);verify_response(data,2,1,challenge,nonce);assert action==0
assert not reply(n,status_packet)[0] and n.management.last_sequence==1
assert not reply(n,udp(request(3,1,nonce,challenge)))[0]
assert not reply(fresh(bytes(range(2,18))),status_packet)[0]
data,action=reply(n,udp(request(3,2,nonce,challenge)));verify_response(data,3,2,challenge,nonce);assert action==3

bad_packets={}
for length in range(14):bad_packets['short_ethernet_'+str(length)]=status_packet[:length]
for length in [14,20,33,34,41,42,121]:bad_packets['truncated_'+str(length)]=status_packet[:length]
for label,offset,value in [('wrong_mac',0,0),('source_multicast',6,1),('wrong_type',12,0x81),('ihl_options',14,0x46),('bad_ip_checksum',24,0),('bad_udp_checksum',40,0)]:
    frame=bytearray(status_packet);frame[offset]=value;bad_packets[label]=bytes(frame)
for flags in [0x8000,0x2000,1]:
    from network_wire import ethernet,ipv4
    bad_packets['fragment_'+str(flags)]=ethernet(0x800,ipv4(17,status_packet[34:],flags=flags))
bad_packets['zero_udp_checksum']=status_packet[:40]+b'\0\0'+status_packet[42:]
for bit in range(640):
    payload=bytearray(request(2,1,nonce,challenge));payload[bit//8]^=1<<(bit%8)
    bad_packets['auth_bit_'+str(bit)]=udp(bytes(payload))
for label,offset,value in [('signed_bad_magic',0,88),('signed_bad_version',4,2),('signed_reserved',6,1),('signed_unknown_operation',5,9)]:
    payload=bytearray(request(2,1,nonce,challenge));payload[offset]=value
    payload[-32:]=hmac.digest(KEY,payload[:48],'sha256');bad_packets[label]=udp(bytes(payload))
bad_packets['hello_sequence']=udp(request(1,1,b'\0'*16,challenge))
bad_packets['hello_nonce']=udp(request(1,0,nonce,challenge))
bad_packets['zero_challenge']=udp(request(2,1,nonce,b'\0'*16))
bad_packets['zero_sequence']=udp(request(2,0,nonce,challenge))
for label,packet in bad_packets.items():
    test=fresh();data,action=reply(test,packet);assert not data and action==0 and not test.management.last_sequence,label
for length in [0,1,47,48,79,81,512]:
    data,_=reply(fresh(),udp(rng.randbytes(length)));assert not data
for _ in range(2000):
    packet=rng.randbytes(rng.randrange(0,1600));data,action=reply(fresh(),packet);assert not data and not action
assert not reply(fresh(),status_packet,1513)[0]
zero=Management();assert not dll.management_init(C.byref(zero),b'\0'*32,nonce) and not zero.ready
assert not dll.management_init(C.byref(zero),KEY,b'\0'*16) and not zero.ready
result={'crypto_oracle_cases':crypto_cases,'invalid_frame_auth_cases':len(bad_packets)+7+2000,'valid_arp_icmp_and_authenticated_commands':True,'replay_and_cross_boot_rejected':True,'scope':'actual host C; no physical NIC claim'}
(out/'network-host-verification.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))

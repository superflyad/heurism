"""Independent Python packet/protocol oracle for the isolated native VM tests."""
import hashlib
import hmac
import struct

KEY=bytes(range(32))  # Public fixture; never a deployment credential.
MAC=bytes.fromhex('525400123456')
PEER_MAC=bytes.fromhex('525400abcdef')
IP=bytes([10,0,2,15])
PEER_IP=bytes([10,0,2,2])
PORT=47333

def checksum(data):
    if len(data)%2:data+=b'\0'
    total=sum(struct.unpack('>'+str(len(data)//2)+'H',data))
    while total>>16:total=(total&65535)+(total>>16)
    return (~total)&65535

def ethernet(kind,payload,dst=MAC,source=PEER_MAC):return dst+source+struct.pack('>H',kind)+payload

def ipv4(protocol,payload,source=PEER_IP,destination=IP,flags=0):
    header=struct.pack('>BBHHHBBH4s4s',0x45,0,len(payload)+20,0x1234,flags,64,protocol,0,source,destination)
    header=header[:10]+struct.pack('>H',checksum(header))+header[12:]
    return header+payload

def udp(payload,source_port=49000,destination_port=PORT):
    data=struct.pack('>HHHH',source_port,destination_port,len(payload)+8,0)+payload
    pseudo=PEER_IP+IP+struct.pack('>BBH',0,17,len(data))
    tag=checksum(pseudo+data) or 65535
    return ethernet(0x800,ipv4(17,data[:6]+struct.pack('>H',tag)+data[8:]))

def arp():
    return ethernet(0x806,struct.pack('>HHBBH6s4s6s4s',1,0x800,6,4,1,PEER_MAC,PEER_IP,b'\0'*6,IP),b'\xff'*6)

def ping(sequence,payload=b'companion native'):
    data=struct.pack('>BBHHH',8,0,0,0x434d,sequence)+payload
    data=data[:2]+struct.pack('>H',checksum(data))+data[4:]
    return ethernet(0x800,ipv4(1,data))

def request(operation,sequence,boot_nonce,client_nonce,key=KEY):
    data=struct.pack('>4sBBHQ16s16s',b'CMP1',1,operation,0,sequence,boot_nonce,client_nonce)
    return data+hmac.digest(key,data,'sha256')

def parse_ipv4(frame):
    assert frame[:6]==PEER_MAC and frame[6:12]==MAC and frame[12:14]==b'\x08\0'
    ip=frame[14:];assert ip[0]==0x45 and checksum(ip[:20])==0
    total=struct.unpack_from('>H',ip,2)[0];assert 20<=total<=len(ip)
    assert ip[12:16]==IP and ip[16:20]==PEER_IP
    return ip[9],ip[20:total]

def verify_response(frame,operation,sequence,client_nonce,boot_nonce=None):
    proto,udp_data=parse_ipv4(frame);assert proto==17
    source,dest,length,tag=struct.unpack_from('>HHHH',udp_data)
    assert source==PORT and dest==49000 and length==len(udp_data) and tag
    assert checksum(IP+PEER_IP+struct.pack('>BBH',0,17,length)+udp_data)==0
    data=udp_data[8:];assert len(data)==120
    assert hmac.compare_digest(data[-32:],hmac.digest(KEY,data[:-32],'sha256'))
    magic,version,op,reserved,seq,nonce,challenge=struct.unpack_from('>4sBBHQ16s16s',data)
    assert (magic,version,op,reserved,seq,challenge)==(b'CMP1',1,operation|128,0,sequence,client_nonce)
    assert any(nonce) and (boot_nonce is None or nonce==boot_nonce)
    return nonce,struct.unpack_from('>5Q',data,48)

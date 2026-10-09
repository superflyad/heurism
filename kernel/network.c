#include "network.h"
#include "../common/sha256.h"
static U16 big16(const U8 *p) { return (U16)((U16)p[0]<<8|p[1]); }
static void put16(U8 *p,U16 n) { p[0]=(U8)(n>>8);p[1]=(U8)n; }
static void copy(U8 *a,const U8 *b,U32 n) { while(n--) *a++=*b++; }
static U32 sum(const U8 *p,U32 n,U32 total) { while(n>=2) { total+=big16(p);p+=2;n-=2; }if(n) total+=(U32)*p<<8;return total; }
static U16 checksum(U32 total) { while(total>>16) total=(total&65535)+(total>>16);return (U16)~total; }
static int same(const U8 *a,const U8 *b,U32 n) { return bytes_equal_constant(a,b,n); }
static U32 pseudo(const U8 *ip,U16 size) { return sum(ip+12,8,17+size); }
U32 network_reply(Network *net,const U8 *packet,U32 size,const ManagementStatus *status,U8 *out,U32 capacity,U8 *action) {
    static const U8 broadcast[6]={255,255,255,255,255,255};
    if(action) *action=0;
    if(!net || !packet || !out || !action || !status || size<14 || size>1514 || capacity<1514) return 0;
    if(packet[6]&1 || (!same(packet,net->mac,6) && !same(packet,broadcast,6))) goto dropped;
    U16 type=big16(packet+12);
    if(type==0x806) {
        if(size<42 || big16(packet+14)!=1 || big16(packet+16)!=0x800 || packet[18]!=6 || packet[19]!=4 ||
           big16(packet+20)!=1 || !same(packet+22,packet+6,6) || !same(packet+38,net->ip,4)) goto dropped;
        copy(out,packet+6,6);copy(out+6,net->mac,6);put16(out+12,0x806);
        copy(out+14,packet+14,28);put16(out+20,2);copy(out+32,packet+22,6);copy(out+38,packet+28,4);
        copy(out+22,net->mac,6);copy(out+28,net->ip,4);++net->accepted;return 42;
    }
    if(type!=0x800 || size<34 || !same(packet,net->mac,6)) goto dropped;
    const U8 *ip=packet+14;U32 total=big16(ip+2);
    if(ip[0]!=0x45 || total<20 || total>size-14 || total>1500 || !ip[8] || (big16(ip+6)&0xbfff) ||
       checksum(sum(ip,20,0)) || !same(ip+16,net->ip,4) || !ip[12] || ip[12]>=224 || same(ip+12,net->ip,4)) goto dropped;
    U32 response=0;
    copy(out,packet+6,6);copy(out+6,net->mac,6);put16(out+12,0x800);
    copy(out+14,ip,20);out[15]=0;out[22]=64;copy(out+26,net->ip,4);copy(out+30,ip+12,4);
    if(ip[9]==1) {
        if(total<28 || ip[20]!=8 || ip[21] || checksum(sum(ip+20,total-20,0))) goto dropped;
        copy(out+34,ip+20,total-20);out[34]=0;put16(out+36,0);put16(out+36,checksum(sum(out+34,total-20,0)));response=total;
    } else if(ip[9]==17) {
        const U8 *udp=ip+20;U32 udp_length=total-20;
        if(total<28 || big16(udp+2)!=MANAGEMENT_PORT || !big16(udp) || big16(udp+4)!=udp_length ||
           !big16(udp+6) || checksum(sum(udp,udp_length,pseudo(ip,(U16)udp_length)))) goto dropped;
        U32 reply=management_reply(&net->management,udp+8,udp_length-8,status,out+42,capacity-42,action);
        if(!reply) goto dropped;
        put16(out+34,MANAGEMENT_PORT);put16(out+36,big16(udp));put16(out+38,(U16)(reply+8));put16(out+40,0);
        U16 tag=checksum(sum(out+34,reply+8,pseudo(out+14,(U16)(reply+8))));put16(out+40,tag?tag:65535);response=20+8+reply;
    } else goto dropped;
    put16(out+16,(U16)response);put16(out+24,0);put16(out+24,checksum(sum(out+14,20,0)));
    ++net->accepted;return response+14;
dropped:
    ++net->dropped;return 0;
}

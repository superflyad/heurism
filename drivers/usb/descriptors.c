#include "xhci.h"
static U16 le16(const U8 *p) { return (U16)(p[0]|(U16)p[1]<<8); }
int usb_device_descriptor(const U8 *p,U32 size,U32 speed,UsbDeviceInfo *out) {
    if(!p || !out || size!=18 || p[0]!=18 || p[1]!=1 || !p[17] || speed<1 || speed>4) return 0;
    U32 packet=p[7];
    if(speed==4) { if(packet!=9 || le16(p+2)<0x300) return 0;packet=512; }
    else if(speed==3) { if(packet!=64) return 0; }
    else if(speed==2) { if(packet!=8) return 0; }
    else if(packet!=8 && packet!=16 && packet!=32 && packet!=64) return 0;
    out->vendor=le16(p+8);out->product=le16(p+10);out->usb_version=le16(p+2);
    out->device_version=le16(p+12);out->ep0_packet=(U16)packet;
    out->device_class=p[4];out->configurations=p[17];return 1;
}
int usb_configuration_descriptor(const U8 *p,U32 size,U8 *interfaces) {
    if(!p || !interfaces || size<9 || size>1024 || p[0]!=9 || p[1]!=2 ||
       le16(p+2)!=size || !p[4] || !p[5] || !(p[7]&0x80) || (p[7]&31)) return 0;
    U8 seen[256]={0},active=0,declared=0,endpoints=0,addresses[32]={0},last_interface=0,last_alternate=0;U32 count=0;
    for(U32 offset=9;offset<size;) {
        if(size-offset<2 || p[offset]<2 || p[offset]>size-offset) return 0;
        U32 length=p[offset],type=p[offset+1];const U8 *d=p+offset;
        if(type==2) return 0;
        if(type==4) {
            if(length!=9 || d[2]>=p[4] || (active && endpoints!=declared)) return 0;
            if(d[3]==0) { if(seen[d[2]]) return 0;seen[d[2]]=1;++count; }
            else if(!seen[d[2]] || !active || d[2]!=last_interface || d[3]<=last_alternate) return 0;
            last_interface=d[2];last_alternate=d[3];
            active=1;declared=d[4];endpoints=0;
            for(U32 i=0;i<32;++i) addresses[i]=0;
        } else if(type==5) {
            if(!active || length<7 || (d[2]&0x70) || !(d[2]&15) || !le16(d+4) ||
               (d[3]&3)==0 || ++endpoints>declared) return 0;
            U32 index=(d[2]&15)+((d[2]&0x80)?16:0);if(addresses[index]) return 0;addresses[index]=1;
        }
        offset+=length;
    }
    if(!active || endpoints!=declared || count!=p[4]) return 0;
    *interfaces=(U8)count;return 1;
}

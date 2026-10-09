#include "usb_ecm.h"
int usb_ecm_frame_boundary(U8 *discard,U32 length) {
    if(!discard) return 0;
    if(!length) { *discard=0;return 0; }
    if(*discard || length>1514) { *discard=length>=USB_ECM_TRANSFER_SIZE;return 0; }
    return 1;
}
static U16 le16(const U8 *p) { return p[0]|(U16)p[1]<<8; }
int usb_ecm_descriptor(const U8 *p,U32 size,U32 speed,UsbEcmDescriptor *out) {
    if(!out) return 0;*out=(UsbEcmDescriptor){0};U8 interfaces;
    if(!usb_configuration_descriptor(p,size,&interfaces) || (speed!=1 && speed!=3 && speed!=4)) return 0;
    UsbEcmDescriptor found={0};found.configuration=p[5];
    U8 interface=255,alternate=0,control=0,data=0,have_control=0,have_union=0,have_ethernet=0,have_header=0,union_slave=255;
    U8 pending_type=0;U16 pending_packet=0;UsbBulkEndpoint *pending_bulk=0;
    for(U32 offset=9;offset<size;offset+=p[offset]) {
        const U8 *d=p+offset;U32 length=d[0],type=d[1];
        /* SuperSpeed companions immediately follow their endpoint. Streams
         * are not supported by the single-ring bulk transport. */
        if(pending_type && type!=0x30) return 0;
        if(type==0x30) {
            if(speed!=4 || !pending_type || length!=6 || d[2]>15) return 0;
            if(pending_type==2) {
                if(d[3] || le16(d+4)) return 0;
                if(pending_bulk) pending_bulk->burst=d[2];
            } else if(pending_type==3) {
                if(d[3] || !le16(d+4) || le16(d+4)>(U32)pending_packet*(d[2]+1)) return 0;
            } else return 0;
            pending_type=0;pending_bulk=0;continue;
        }
        if(type==0x31) return 0; /* SuperSpeedPlus is not this profile. */
        if(type==4) {
            interface=d[2];alternate=d[3];control=d[5]==2 && d[6]==6 && d[7]==0;
            data=d[5]==10 && !d[6] && !d[7];
            if(control) { if(have_control || alternate) return 0;have_control=1;found.control_interface=interface; }
            if(data && d[4]) {
                if(found.in.address || found.out.address) return 0;
                found.data_interface=interface;found.alternate=alternate;
            }
        } else if(type==0x24 && control) {
            if(length<3) return 0;
            if(d[2]==0) {
                if(length!=5 || have_header || le16(d+3)<0x110 || le16(d+3)>0x120) return 0;
                have_header=1;
            } else if(d[2]==6) {
                if(length!=5 || have_union || d[3]!=interface) return 0;
                union_slave=d[4];have_union=1;
            } else if(d[2]==15) {
                if(length!=13 || have_ethernet || !d[3]) return 0;
                found.mac_string=d[3];found.max_frame=le16(d+8);have_ethernet=1;
                if(found.max_frame!=1514) return 0; /* Fixed Ethernet frame budget. */
            }
        } else if(type==5 && data && (d[3]&3)==2) {
            if(interface!=found.data_interface || alternate!=found.alternate || length!=7 || (d[3]&~3U)) return 0;
            UsbBulkEndpoint *ep=(d[2]&0x80)?&found.in:&found.out;
            if(ep->address) return 0;ep->address=d[2];ep->max_packet=le16(d+4);
            if((speed==1 && ep->max_packet!=8 && ep->max_packet!=16 && ep->max_packet!=32 && ep->max_packet!=64) ||
               (speed==3 && ep->max_packet!=512) || (speed==4 && ep->max_packet!=1024)) return 0;
            pending_bulk=ep;
        }
        if(type==5 && speed==4) {
            if(length!=7 || (d[3]&~3U)) return 0;
            pending_type=d[3]&3;pending_packet=le16(d+4);
            if((pending_type!=2 && pending_type!=3) || pending_packet>1024) return 0;
        }
    }
    if(pending_type || !have_header || !have_control || !have_union || !have_ethernet || !found.in.address || !found.out.address || found.data_interface!=union_slave) return 0;
    *out=found;return 1;
}
static int digit(U8 c) { return c>='0' && c<='9'?c-'0':c>='A' && c<='F'?c-'A'+10:c>='a' && c<='f'?c-'a'+10:-1; }
int usb_ecm_mac(const U8 *p,U32 length,U8 *mac) {
    if(!p || !mac || length!=26 || p[0]!=26 || p[1]!=3) return 0;
    U8 parsed[6],nonzero=0;
    for(U32 i=0;i<6;++i) {
        int high=digit(p[2+i*4]),low=digit(p[4+i*4]);
        if(high<0 || low<0 || p[3+i*4] || p[5+i*4]) return 0;
        parsed[i]=(U8)(high*16+low);nonzero|=parsed[i];
    }
    if(!nonzero || (parsed[0]&1)) return 0;
    for(U32 i=0;i<6;++i) mac[i]=parsed[i];return 1;
}
int usb_ecm_start(Xhci *x,const UsbDeviceInfo *device,U8 *mac) {
    if(!x || !device || !mac || !x->active_slot || !device->configurations) return 0;
    U8 bytes[1024];UsbEcmDescriptor descriptor;int found=0;
    for(U32 index=0;index<device->configurations && index<8;++index) {
        U16 value=(U16)(0x200|index);
        if(!xhci_control(x,0x80,6,value,0,bytes,9)) return 0;
        U32 size=le16(bytes+2);if(bytes[0]!=9 || bytes[1]!=2 || size<9 || size>sizeof(bytes) || !xhci_control(x,0x80,6,value,0,bytes,size)) return 0;
        if(usb_ecm_descriptor(bytes,size,device->speed,&descriptor)) { found=1;break; }
    }
    if(!found || !xhci_control(x,0x80,6,(U16)(0x300|descriptor.mac_string),0x409,bytes,26) || !usb_ecm_mac(bytes,26,mac)) return 0;
    if(!xhci_configure_bulk(x,&descriptor.in,&descriptor.out) ||
       !xhci_control(x,0,9,descriptor.configuration,0,0,0) ||
       (descriptor.alternate && !xhci_control(x,1,11,descriptor.alternate,descriptor.data_interface,0,0)) ||
       !xhci_control(x,0x21,0x43,0x0c,descriptor.control_interface,0,0)) return 0;
    return 1;
}

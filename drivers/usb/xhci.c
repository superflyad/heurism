#include "xhci.h"
static U32 read32(Xhci *x,U32 offset) { return x->io.read(x->io.context,offset); }
static void write32(Xhci *x,U32 offset,U32 value) { x->io.write(x->io.context,offset,value); }
static void barrier(Xhci *x) { x->io.fence(x->io.context); }
static void write64(Xhci *x,U32 offset,U64 value) { write32(x,offset,(U32)value);write32(x,offset+4,(U32)(value>>32)); }
static int wait32(Xhci *x,U32 offset,U32 mask,U32 wanted) {
    for(U32 i=0;i<x->io.poll_limit;++i) if((read32(x,offset)&mask)==wanted) return 1;
    return 0;
}
static void clear(U64 address) { U8 *p=(U8 *)address;for(U32 i=0;i<4096;++i) p[i]=0; }
static U64 allocate(Xhci *x) {
    if(x->allocated==XHCI_MAX_PAGES) return 0;
    U64 page=x->io.allocate(x->io.context);
    if(!page) return 0;
    /* An allocator violating the DMA contract is rejected before publication. */
    if((page&4095) || page>0xfffff000ULL) { x->io.release(x->io.context,page);return 0; }
    for(U32 i=0;i<x->allocated;++i) if(x->pages[i]==page) return 0;
    x->pages[x->allocated++]=page;clear(page);return page;
}
int xhci_stop(Xhci *x) {
    if(!x || !x->io.read || !x->io.write || !x->io.bus_master || !x->io.release || !x->io.fence || !x->io.poll_limit) return 0;
    if(x->op) {
        write32(x,x->op,read32(x,x->op)&~5U);
        if(!wait32(x,x->op+4,1,1)) { x->io.bus_master(x->io.context,0);x->failed=1;return 0; }
    }
    x->io.bus_master(x->io.context,0);
    x->running=0;barrier(x);
    for(U32 i=0;i<x->allocated;++i) x->io.release(x->io.context,x->pages[i]);
    x->allocated=0;x->active_slot=0;x->bulk_in=(XhciBulk){0};x->bulk_out=(XhciBulk){0};return 1;
}
int xhci_start(Xhci *x,const XhciIo *io) {
    if(!x || !io || !io->read || !io->write || !io->allocate || !io->release || !io->fence ||
       !io->bus_master || !io->poll_limit || io->poll_limit>1000000) return 0;
    if(x->running || x->allocated) return 0;
    *x=(Xhci){0};x->io=*io;
    U32 cap=read32(x,0),params=read32(x,4),params2=read32(x,8),hcc=read32(x,16);
    U32 op=cap&255,runtime=read32(x,24),doorbell=read32(x,20);
    U32 slots=params&255,ports=params>>24,interrupters=(params>>8)&0x7ff;
    U32 scratchpads=((params2>>27)&31)|((params2>>16)&0x3e0);
    if((cap>>16)<0x100 || (cap>>16)>0x120 || op<0x20 || op>0x100 || (op&3) ||
       !slots || !ports || ports>32 || !interrupters || scratchpads>32 ||
       (runtime&31) || runtime<op+0x400+ports*16 || runtime>XHCI_WINDOW-0x40 ||
       (doorbell&3) || doorbell<op+0x400+ports*16 || doorbell>XHCI_WINDOW-4*(slots+1) ||
       (doorbell<runtime+0x40 && runtime<doorbell+4*(slots+1))) return 0;
    /* Never force firmware ownership or manipulate unknown legacy SMI policy.
     * QEMU has no BIOS-owned legacy capability; physical handoff is pending. */
    U32 extended=(hcc>>16)*4;
    for(U32 count=0;extended;++count) {
        if(count==64 || extended<0x20 || extended>XHCI_WINDOW-16) return 0;
        U32 value=read32(x,extended),next=((value>>8)&255)*4;
        if((value&255)==1 && (value&(1U<<16))) return 0;
        if(next && next>XHCI_WINDOW-extended-16) return 0;
        extended=next?extended+next:0;
    }
    x->op=op;x->runtime=runtime;x->doorbell=doorbell;
    x->slots=slots>32?32:slots;x->ports=ports;x->context_size=(hcc&4)?64:32;
    x->io.bus_master(x->io.context,0);
    if(!wait32(x,op+4,1U<<11,0)) return 0;
    write32(x,op,read32(x,op)&~5U);
    if(!wait32(x,op+4,1,1)) return 0;
    write32(x,op,2);
    if(!wait32(x,op,2,0) || !wait32(x,op+4,1U<<11,0) || !(read32(x,op+8)&1)) return 0;
    x->dcbaa=allocate(x);x->command_ring=allocate(x);x->event_ring=allocate(x);x->erst=allocate(x);
    x->input=allocate(x);x->output=allocate(x);x->transfer_ring=allocate(x);x->buffer=allocate(x);
    if(!x->dcbaa || !x->command_ring || !x->event_ring || !x->erst || !x->input || !x->output || !x->transfer_ring || !x->buffer) goto failed;
    if(scratchpads) {
        U64 array=allocate(x);if(!array) goto failed;
        ((U64 *)x->dcbaa)[0]=array;
        for(U32 i=0;i<scratchpads;++i) { U64 page=allocate(x);if(!page) goto failed;((U64 *)array)[i]=page; }
    }
    ((U64 *)x->erst)[0]=x->event_ring;((U32 *)x->erst)[2]=XHCI_RING_SIZE;
    x->command_cycle=x->event_cycle=1;
    write32(x,op+0x38,x->slots);write64(x,op+0x30,x->dcbaa);
    write64(x,op+0x18,x->command_ring|1);
    write32(x,runtime+0x20,0);write32(x,runtime+0x24,0);
    write32(x,runtime+0x28,1);write64(x,runtime+0x38,x->event_ring);
    /* ERSTBA programming may immediately fetch the segment table, even while
     * halted. All owned memory/pointers are initialized before enabling DMA;
     * reset has already invalidated firmware's command/event state. */
    barrier(x);x->io.bus_master(x->io.context,1);write64(x,runtime+0x30,x->erst);
    write32(x,op,1);if(!wait32(x,op+4,1,0)) goto failed;
    x->running=1;return 1;
failed:
    x->failed=1;(void)xhci_stop(x);return 0;
}
static int next_event(Xhci *x,XhciTrb *out) {
    volatile XhciTrb *event=(volatile XhciTrb *)x->event_ring+x->event_next;
    if((event->control&1)!=x->event_cycle) return 0;
    barrier(x);out->parameter=event->parameter;out->status=event->status;out->control=event->control;
    if(++x->event_next==XHCI_RING_SIZE) { x->event_next=0;x->event_cycle^=1; }
    ++x->events;write64(x,x->runtime+0x38,(x->event_ring+x->event_next*16)|8);
    return 1;
}
static U64 enqueue(Xhci *x,U64 ring,U32 *index,U8 *cycle,U64 parameter,U32 status,U32 control,U8 publish) {
    volatile XhciTrb *trbs=(volatile XhciTrb *)ring;U32 current=*index;U64 address=ring+current*16;
    trbs[current].parameter=parameter;trbs[current].status=status;
    /* Publish a segment's Link before the preceding TRB becomes visible. */
    if(current==XHCI_RING_SIZE-2) {
        trbs[current+1].parameter=ring;trbs[current+1].status=0;barrier(x);
        trbs[current+1].control=(6U<<10)|2|*cycle|(control&16U);
    }
    barrier(x);trbs[current].control=control|(publish?*cycle:*cycle^1U);
    if(++*index==XHCI_RING_SIZE-1) { *index=0;*cycle^=1; }
    return address;
}
static int bulk_event(Xhci *x,const XhciTrb *event) {
    if(((event->control>>10)&63)!=32 || event->control>>24!=x->active_slot || (event->control&4)) return 0;
    U32 dci=(event->control>>16)&31;XhciBulk *ep=dci==x->bulk_in.dci?&x->bulk_in:dci==x->bulk_out.dci?&x->bulk_out:0;
    if(!ep || !ep->pending || event->parameter!=ep->pointer) return 0;
    U32 code=event->status>>24,residual=event->status&0xffffff;
    if((code!=1 && (ep!=&x->bulk_in || code!=13)) || residual>ep->requested ||
       (ep==&x->bulk_out && residual)) { x->failed=1;return -1; }
    ep->actual=ep->requested-residual;ep->pending=0;ep->done=1;++x->transfers;x->last_completion=code;return 1;
}
static int completion(Xhci *x,U32 type,U64 pointer,U32 slot,XhciTrb *out) {
    for(U32 i=0;i<x->io.poll_limit;++i) {
        if(read32(x,x->op+4)&((1U<<2)|(1U<<12))) break;
        XhciTrb event;if(!next_event(x,&event)) continue;
        U32 kind=(event.control>>10)&63;
        if(kind==34) { ++x->port_events;continue; }
        if(bulk_event(x,&event)>0) continue;
        if(x->failed) break;
        if(kind==type && event.parameter==pointer && !(event.control&4) && (!slot || event.control>>24==slot)) {
            *out=event;x->last_completion=event.status>>24;return 1;
        }
        /* With serialized operations an unexpected command/transfer event is
         * never accepted as the current completion. Fail rather than reuse DMA. */
        break;
    }
    x->failed=1;return 0;
}
static int command(Xhci *x,U32 type,U64 parameter,U32 slot,U32 *new_slot) {
    if(!x || !x->running || x->failed) return 0;
    U64 pointer=enqueue(x,x->command_ring,&x->command_next,&x->command_cycle,parameter,0,(type<<10)|(slot<<24),1);
    barrier(x);write32(x,x->doorbell,0);XhciTrb event;
    if(!completion(x,33,pointer,slot,&event) || (event.status>>24)!=1) { x->failed=1;return 0; }
    if(new_slot) *new_slot=event.control>>24;
    ++x->commands;return 1;
}
int xhci_noop(Xhci *x) { return command(x,23,0,0,0); }
static int control(Xhci *x,U32 slot,U8 request_type,U8 request,U16 value,U16 index,U8 *bytes,U32 size) {
    if(size>1024 || (size && !bytes) || !x || !x->running || x->failed) return 0;
    clear(x->buffer);
    U32 direction=(request_type&0x80)?1U<<16:0;
    if(size && !direction) for(U32 i=0;i<size;++i) ((U8 *)x->buffer)[i]=bytes[i];
    U64 setup=request_type|((U64)request<<8)|((U64)value<<16)|((U64)index<<32)|((U64)size<<48);
    U8 setup_cycle=x->transfer_cycle;
    U32 setup_control=(2U<<10)|(1U<<6)|(size?((direction?3U:2U)<<16):0);
    U64 first=enqueue(x,x->transfer_ring,&x->transfer_next,&x->transfer_cycle,setup,8,setup_control,0);
    if(size) (void)enqueue(x,x->transfer_ring,&x->transfer_next,&x->transfer_cycle,x->buffer,size,(3U<<10)|direction|(1U<<2),1);
    U64 status=enqueue(x,x->transfer_ring,&x->transfer_next,&x->transfer_cycle,0,0,(4U<<10)|(1U<<5)|((!size || !direction)?1U<<16:0),1);
    /* Make the complete control request visible atomically at its Setup TRB. */
    barrier(x);((volatile XhciTrb *)first)->control=setup_control|setup_cycle;
    barrier(x);write32(x,x->doorbell+slot*4,1);
    /* Exact length is required for these descriptor requests. A short packet
     * is rejected and the caller halts the controller before reusing buffers. */
    XhciTrb event;
    if(!completion(x,32,status,slot,&event) || ((event.control>>16)&31)!=1 ||
       (event.status>>24)!=1 || (event.status&0xffffff)) { x->failed=1;return 0; }
    if(direction && size) { barrier(x);for(U32 i=0;i<size;++i) bytes[i]=((const U8 *)x->buffer)[i]; }
    ++x->transfers;return 1;
}
int xhci_control(Xhci *x,U8 request_type,U8 request,U16 value,U16 index,U8 *bytes,U32 size) {
    return x && x->active_slot?control(x,x->active_slot,request_type,request,value,index,bytes,size):0;
}
static int descriptor(Xhci *x,U32 slot,U32 type,U32 size) {
    U8 bytes[1024];return size && control(x,slot,0x80,6,(U16)(type<<8),0,bytes,size);
}
int xhci_open_port(Xhci *x,U32 port,UsbDeviceInfo *info) {
    if(!x || !x->running || x->failed || !info || !port || port>x->ports) return -1;
    if(x->active_slot) return -1;
    *info=(UsbDeviceInfo){0};U32 reg=x->op+0x400+(port-1)*16,value=read32(x,reg);
    if(!(value&1)) return 0;
    if(value&(1U<<3)) return -1;
    /* Preserve power/wake policy without echoing PED or W1C status bits. */
    U32 policy=value&((1U<<9)|(3U<<14)|(7U<<25));
    write32(x,reg,policy|(1U<<4));
    if(!wait32(x,reg,1U<<4,0)) goto failed;
    value=read32(x,reg);U32 speed=(value>>10)&15;
    if((value&3)!=3 || speed<1 || speed>4) goto failed;
    U32 slot=0;if(!command(x,9,0,0,&slot) || !slot || slot>x->slots) goto failed;
    clear(x->input);clear(x->output);clear(x->transfer_ring);
    x->transfer_next=0;x->transfer_cycle=1;
    ((U64 *)x->dcbaa)[slot]=x->output;
    U32 *input=(U32 *)x->input,*s=(U32 *)(x->input+x->context_size),*ep=(U32 *)(x->input+2*x->context_size);
    input[1]=3;s[0]=(speed<<20)|(1U<<27);s[1]=port<<16;
    U32 packet=speed==4?512:speed==3?64:8;
    ep[1]=(3U<<1)|(4U<<3)|(packet<<16);ep[2]=(U32)x->transfer_ring|1;ep[3]=0;ep[4]=8;
    if(!command(x,11,x->input,slot,0) || !descriptor(x,slot,1,8)) goto failed;
    const U8 *bytes=(const U8 *)x->buffer;U32 advertised=bytes[7];
    if(bytes[0]!=18 || bytes[1]!=1) goto failed;
    if(speed==4) { if(advertised!=9) goto failed;advertised=512; }
    else if((speed==3 && advertised!=64) || (speed==2 && advertised!=8) ||
            (speed==1 && advertised!=8 && advertised!=16 && advertised!=32 && advertised!=64)) goto failed;
    if(packet!=advertised) {
        clear(x->input);input[1]=2;ep[1]=advertised<<16;
        if(!command(x,13,x->input,slot,0)) goto failed;
    }
    if(!descriptor(x,slot,1,18) || !usb_device_descriptor((const U8 *)x->buffer,18,speed,info)) goto failed;
    if(!descriptor(x,slot,2,9)) goto failed;
    bytes=(const U8 *)x->buffer;U32 length=bytes[2]|(U32)bytes[3]<<8;
    if(bytes[0]!=9 || bytes[1]!=2 || length<9 || length>1024 || !descriptor(x,slot,2,length) ||
       !usb_configuration_descriptor((const U8 *)x->buffer,length,&info->interfaces)) goto failed;
    info->configuration_bytes=(U16)length;info->port=(U8)port;info->speed=(U8)speed;
    x->active_slot=slot;x->active_port=port;x->active_speed=speed;return 1;
failed:
    *info=(UsbDeviceInfo){0};x->failed=1;return -1;
}
int xhci_close_port(Xhci *x) {
    if(!x || !x->active_slot || x->bulk_in.pending || x->bulk_out.pending) return 0;
    if(!command(x,10,0,x->active_slot,0)) return 0;
    ((U64 *)x->dcbaa)[x->active_slot]=0;x->active_slot=0;
    x->bulk_in=(XhciBulk){0};x->bulk_out=(XhciBulk){0};barrier(x);return 1;
}
int xhci_inspect_port(Xhci *x,U32 port,UsbDeviceInfo *info) {
    int result=xhci_open_port(x,port,info);
    if(result==1 && !xhci_close_port(x)) { *info=(UsbDeviceInfo){0};return -1; }return result;
}
static int endpoint_valid(const UsbBulkEndpoint *ep,U32 speed,U8 in) {
    if(!ep || (ep->address&0x70) || !(ep->address&15) || !!(ep->address&0x80)!=in || ep->burst>15) return 0;
    if(speed==1) return !ep->burst && (ep->max_packet==8 || ep->max_packet==16 || ep->max_packet==32 || ep->max_packet==64);
    if(speed==3) return !ep->burst && ep->max_packet==512;
    if(speed==4) return ep->max_packet==1024;
    return 0; /* Low-speed USB has no bulk endpoints. */
}
int xhci_configure_bulk(Xhci *x,const UsbBulkEndpoint *in,const UsbBulkEndpoint *out) {
    if(!x || !x->running || x->failed || !x->active_slot || x->bulk_in.ring || x->bulk_out.ring ||
       !endpoint_valid(in,x->active_speed,1) || !endpoint_valid(out,x->active_speed,0)) return 0;
    XhciBulk *ep[2]={&x->bulk_in,&x->bulk_out};const UsbBulkEndpoint *config[2]={in,out};
    clear(x->input);U32 *input=(U32 *)x->input,*slot=(U32 *)(x->input+x->context_size);
    const U32 *old=(const U32 *)x->output;
    for(U32 i=0;i<4;++i) slot[i]=old[i];
    U32 highest=1;input[1]=1;
    for(U32 n=0;n<2;++n) {
        XhciBulk *b=ep[n];const UsbBulkEndpoint *c=config[n];
        b->ring=allocate(x);b->buffer=allocate(x);if(!b->ring || !b->buffer) { x->failed=1;return 0; }
        b->dci=(U8)((c->address&15)*2+(n==0));b->max_packet=c->max_packet;b->cycle=1;
        if(b->dci>highest) highest=b->dci;
        input[1]|=1U<<b->dci;U32 *context=(U32 *)(x->input+(b->dci+1)*x->context_size);
        context[1]=(3U<<1)|((n==0?6U:2U)<<3)|((U32)c->burst<<8)|((U32)c->max_packet<<16);
        context[2]=(U32)b->ring|1;context[3]=0;context[4]=2048;
    }
    slot[0]=(slot[0]&~(31U<<27))|(highest<<27);
    return command(x,12,x->input,x->active_slot,0);
}
static int poll_bulk(Xhci *x) {
    if(!x || !x->running || x->failed) return 0;
    if(read32(x,x->op+4)&((1U<<2)|(1U<<12))) { x->failed=1;return 0; }
    for(U32 i=0;i<XHCI_RING_SIZE;++i) {
        XhciTrb event;if(!next_event(x,&event)) break;
        if(((event.control>>10)&63)==34) { ++x->port_events;continue; }
        if(bulk_event(x,&event)!=1) { x->failed=1;return 0; }
    }
    return 1;
}
static int submit_bulk(Xhci *x,XhciBulk *ep,U32 size,U8 zlp) {
    if(!ep->ring || ep->pending || ep->done || size>4096) return 0;
    /* A terminating ZLP is its own TD. Chaining a zero-length TRB into the
     * data TD can be coalesced into the preceding packet rather than issuing
     * the required zero-byte USB transaction. Only the final TD requests IOC. */
    U8 cycle=ep->cycle;U32 control=(1U<<10)|(zlp?0U:32U)|(ep==&x->bulk_in?4U:0);
    U64 first=enqueue(x,ep->ring,&ep->next,&ep->cycle,ep->buffer,size,control,0);
    ep->pointer=first;
    if(zlp) ep->pointer=enqueue(x,ep->ring,&ep->next,&ep->cycle,ep->buffer,0,(1U<<10)|32U,1);
    ep->requested=zlp?0:size;ep->pending=1;
    barrier(x);((volatile XhciTrb *)first)->control=control|cycle;
    barrier(x);write32(x,x->doorbell+x->active_slot*4,ep->dci);return 1;
}
int xhci_bulk_send(Xhci *x,const U8 *data,U32 size) {
    if(!data || !size || size>4096 || !poll_bulk(x)) return 0;
    XhciBulk *ep=&x->bulk_out;if(!ep->ring || ep->pending) return 0;ep->done=0;
    for(U32 i=0;i<size;++i) ((U8 *)ep->buffer)[i]=data[i];
    return submit_bulk(x,ep,size,size%ep->max_packet==0);
}
int xhci_bulk_receive(Xhci *x,U8 *data,U32 capacity,U32 *size) {
    if(!data || !size || !poll_bulk(x)) return 0;
    XhciBulk *ep=&x->bulk_in;if(!ep->ring) return 0;
    if(ep->done) {
        U32 actual=ep->actual;ep->done=0;
        if(actual>capacity) { x->failed=1;return 0; }
        barrier(x);for(U32 i=0;i<actual;++i) data[i]=((const U8 *)ep->buffer)[i];*size=actual;
        if(!submit_bulk(x,ep,XHCI_BULK_RX_SIZE,0)) { x->failed=1;return 0; }return 1; /* zero length is a packet boundary */
    }
    if(!ep->pending) (void)submit_bulk(x,ep,XHCI_BULK_RX_SIZE,0);
    return 0;
}
int xhci_bulk_tx_idle(Xhci *x) { return poll_bulk(x) && x->bulk_out.ring && !x->bulk_out.pending; }

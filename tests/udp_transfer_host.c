#include "../boot/udp_transfer_probe.c"
static EFI_BOOT_SERVICES mock_bs;static EFI_SYSTEM_TABLE mock_st;static Binding mock_binding;static Udp mock_udp;
static CompanionExtension mock_service;static UdpToken *pending_tx,*pending_rx;
static void *contexts[2];static void (EFIAPI *notify[2])(EFI_EVENT,void *);
static UdpRx received;static U8 wire[1040];
static U32 events,closed,destroyed,loads,starts,stalls,fail,scenario;
static EFI_STATUS EFIAPI create(Binding *b,EFI_HANDLE *h){if(b!=&mock_binding)fail=1;*h=(EFI_HANDLE)2;events=0;return 0;}
static EFI_STATUS EFIAPI destroy(Binding *b,EFI_HANDLE h){if(b!=&mock_binding || h!=(EFI_HANDLE)2)fail=2;destroyed++;pending_tx=pending_rx=0;return 0;}
static EFI_STATUS EFIAPI locate(EFI_GUID *g,void *search,void **out){if(g!=&binding_guid || search)fail=3;*out=&mock_binding;return 0;}
static EFI_STATUS EFIAPI protocol(EFI_HANDLE h,EFI_GUID *g,void **out){
 if(h==(EFI_HANDLE)2 && g==&udp_guid)*out=&mock_udp;
 else if(h==(EFI_HANDLE)3 && g==&udp_extension_guid)*out=&mock_service;
 else return EFI_UNSUPPORTED;return 0;
}
static EFI_STATUS EFIAPI configure(Udp *u,UdpConfig *c){
 if(u!=&mock_udp)fail=4;if(!c){pending_tx=pending_rx=0;return 0;}
 if(c->use_default || c->address[3]!=238 || c->remote[3]!=122 || c->remote_port!=18081 || c->port!=18082)fail=5;
 return scenario==2?EFI_UNSUPPORTED:0;
}
static EFI_STATUS EFIAPI event(U32 type,UINTN tpl,void (EFIAPI *n)(EFI_EVENT,void *),void *c,EFI_EVENT *out){
 if(type!=0x200 || tpl!=8 || events>=2){fail=6;return EFI_UNSUPPORTED;}contexts[events]=c;notify[events]=n;*out=(EFI_EVENT)(UINTN)(++events);return 0;
}
static EFI_STATUS EFIAPI close_event(EFI_EVENT e){if(pending_tx || pending_rx || !destroyed || !e)fail=7;closed++;return 0;}
static EFI_STATUS EFIAPI receive(Udp *u,UdpToken *t){if(u!=&mock_udp)fail=8;pending_rx=t;return 0;}
static EFI_STATUS EFIAPI transmit(Udp *u,UdpToken *t){
 if(u!=&mock_udp || !pending_rx)fail=9;UdpTx *p=t->packet;U8 *r=p->fragments[0].buffer;
 if(p->length!=16 || p->count!=1 || r[8]>1 || r[12]>1)fail=10;
 for(U32 i=0;i<16;i++)wire[i]=r[i];
 for(U32 i=0;i<1024;i++)wire[16+i]=expected_payload[(UINTN)r[8]*1024+i];
 if(r[12] && r[8]==0)wire[16]^=1;
 if(scenario==3)wire[8]^=1;
 received=(UdpRx){0};received.recycle=(EFI_EVENT)7;received.session=(UdpSession){{10,8,22,122},18081,{10,8,22,238},18082};
 if(scenario==4)received.session.source_port=1234;
 received.length=1040;received.count=1;received.fragments[0]=(Fragment){1040,wire};pending_tx=t;return 0;
}
static void signal_token(UdpToken *t){t->status=0;UINTN i=(UINTN)t->event-1;notify[i](t->event,contexts[i]);}
static EFI_STATUS EFIAPI poll(Udp *u){
 if(u!=&mock_udp)fail=11;if(pending_tx){UdpToken *t=pending_tx;pending_tx=0;signal_token(t);}
 if(pending_rx && scenario!=1){UdpToken *t=pending_rx;pending_rx=0;t->packet=&received;signal_token(t);}return 0;
}
static EFI_STATUS EFIAPI cancel(Udp *u,UdpToken *t){if(u!=&mock_udp)fail=12;(void)t;pending_tx=pending_rx=0;return 0;}
static EFI_STATUS EFIAPI stall(UINTN n){if(n!=10000)fail=13;stalls++;return 0;}
static EFI_STATUS EFIAPI recycle(EFI_EVENT e){if(e!=(EFI_EVENT)7)fail=14;return 0;}
static EFI_STATUS EFIAPI load(U8 policy,EFI_HANDLE parent,void *path,void *buffer,UINTN bytes,EFI_HANDLE *child){
 if(policy || parent!=(EFI_HANDLE)1 || path || buffer!=downloaded || buffer==(void *)expected_payload || bytes!=2048 || !exact_download())fail=15;loads++;*child=(EFI_HANDLE)3;return 0;
}
static EFI_STATUS EFIAPI start(EFI_HANDLE h,UINTN *n,U16 **p){if(h!=(EFI_HANDLE)3 || n || p)fail=16;starts++;return 0;}
static EFI_STATUS EFIAPI info(CompanionExtension *s,UINTN *n,CompanionExtensionInfo *out){if(s!=&mock_service || *n!=32)fail=17;out->magic=COMPANION_EXTENSION_MAGIC;out->revision=1;out->capabilities=1;return 0;}
int udp_run_tests(void){
 mock_bs.locate_protocol=locate;mock_bs.handle_protocol=protocol;mock_bs.create_event=(void *)event;mock_bs.close_event=(void *)close_event;mock_bs.signal_event=(void *)recycle;
 mock_bs.stall=stall;mock_bs.load_image=(void *)load;mock_bs.start_image=(void *)start;mock_st.boot_services=&mock_bs;
 mock_binding.create=create;mock_binding.destroy=destroy;mock_udp.configure=configure;mock_udp.transmit=transmit;mock_udp.receive=receive;mock_udp.cancel=cancel;mock_udp.poll=poll;
 mock_service.revision=1;mock_service.get_info=info;udp_inventory((EFI_HANDLE)1,&mock_st,0);
 if(fail || loads!=1 || starts!=1 || destroyed!=2 || closed!=4)return 100+(int)fail;
 scenario=1;U32 previous=stalls;if(fetch(&mock_st,&mock_binding,0,0) || stalls-previous!=301 || destroyed!=3 || closed!=6 || fail)return 200+(int)fail;
 scenario=2;if(fetch(&mock_st,&mock_binding,0,0) || destroyed!=4 || closed!=6 || fail)return 300+(int)fail;
 scenario=3;if(fetch(&mock_st,&mock_binding,0,0) || destroyed!=5 || closed!=8 || fail)return 400+(int)fail;
 scenario=4;if(fetch(&mock_st,&mock_binding,0,0) || destroyed!=6 || closed!=10 || fail)return 500+(int)fail;
 received.count=5;if(accept_response(&received,wire))return 600;
 return 0;
}

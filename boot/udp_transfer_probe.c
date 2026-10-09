/* Owner UDP transfer via standard firmware services, exact image only. */
#include "extension.h"
#include "../build/nv-extension/payload.h"
typedef struct File File;
static void udp_inventory(EFI_HANDLE,EFI_SYSTEM_TABLE *,File *);
#define PROBE_EXTENSION(st,file) udp_inventory(image,st,file)
#define PROBE_TAG "COMPANION_UDP_TRANSFER_PROBE_01"
#define PROBE_DESCRIPTION "Bounded owner UDP transfer; SSD management fallback retained.\n"
#define PROBE_REPORT_PATH u"\\EFI\\companion\\udp-probe-01.txt"
#define PROBE_GRUB_PATH u"\\EFI\\companion\\recoveryx64.efi"
#include "firmware_probe.c"
typedef struct Binding Binding;
struct Binding { EFI_STATUS (EFIAPI *create)(Binding *,EFI_HANDLE *);EFI_STATUS (EFIAPI *destroy)(Binding *,EFI_HANDLE); };
typedef struct { U8 broadcast,promiscuous,any_port,duplicate,tos,ttl,no_fragment;U32 receive_timeout,transmit_timeout;U8 use_default,address[4],subnet[4];U16 port;U8 remote[4];U16 remote_port; } UdpConfig;
typedef struct { U8 source[4];U16 source_port;U8 dest[4];U16 dest_port; } UdpSession;
typedef struct { U32 length;void *buffer; } Fragment;
typedef struct { UdpSession *session;U8 *gateway;U32 length,count;Fragment fragments[1]; } UdpTx;
typedef struct { U8 timestamp[16];EFI_EVENT recycle;UdpSession session;U32 length,count;Fragment fragments[1]; } UdpRx;
typedef struct { EFI_EVENT event;EFI_STATUS status;void *packet; } UdpToken;
typedef struct Udp Udp;
struct Udp { void *mode;EFI_STATUS (EFIAPI *configure)(Udp *,UdpConfig *);void *groups,*routes;
 EFI_STATUS (EFIAPI *transmit)(Udp *,UdpToken *);EFI_STATUS (EFIAPI *receive)(Udp *,UdpToken *);
 EFI_STATUS (EFIAPI *cancel)(Udp *,UdpToken *);EFI_STATUS (EFIAPI *poll)(Udp *); };
typedef EFI_STATUS (EFIAPI *CreateEvent)(U32,UINTN,void (EFIAPI *)(EFI_EVENT,void *),void *,EFI_EVENT *);
typedef EFI_STATUS (EFIAPI *CloseEvent)(EFI_EVENT);
typedef EFI_STATUS (EFIAPI *SignalEvent)(EFI_EVENT);
static EFI_GUID binding_guid={0x83f01464,0x99bd,0x45e5,{0xb3,0x83,0xaf,0x63,0x05,0xd8,0xe9,0xe6}};
static EFI_GUID udp_guid={0x3ad9df29,0x4501,0x478d,{0xb1,0xf8,0x7f,0x7f,0xe7,0x0e,0x50,0xf3}};
static EFI_GUID udp_extension_guid=COMPANION_EXTENSION_GUID;
static U8 downloaded[2048],response_bytes[1040];
static volatile U32 tx_done,rx_done;
static UdpToken tx_token,rx_token;
static void EFIAPI done(EFI_EVENT event,void *context){(void)event;*(volatile U32 *)context=1;}
static EFI_STATUS await_udp(EFI_BOOT_SERVICES *bs,Udp *udp,UdpToken *token,volatile U32 *completed) {
 for(U32 i=0;i<300 && !*completed;i++){udp->poll(udp);bs->stall(10000);}
 if(!*completed){udp->cancel(udp,token);return 0x8000000000000012ULL;}
 return token->status;
}
static U8 accept_response(UdpRx *rx,U8 *request) {
 if(!rx || rx->length!=1040 || !rx->count || rx->count>4 || rx->session.source_port!=18081 || rx->session.dest_port!=18082)return 0;
 static const U8 host[4]={10,8,22,122},local[4]={10,8,22,238};
 for(U32 i=0;i<4;i++)if(rx->session.source[i]!=host[i] || rx->session.dest[i]!=local[i])return 0;
 UINTN at=0;
 for(U32 i=0;i<rx->count;i++) {
  Fragment *f=&rx->fragments[i];if(!f->buffer || f->length>sizeof(response_bytes)-at)return 0;
  for(U32 j=0;j<f->length;j++)response_bytes[at++]=((U8 *)f->buffer)[j];
 }
 if(at!=sizeof(response_bytes))return 0;
 for(U32 i=0;i<16;i++)if(response_bytes[i]!=request[i])return 0;
 return 1;
}
static U8 exact_download(void) {
 for(UINTN i=0;i<sizeof(expected_payload);i++)if(downloaded[i]!=expected_payload[i])return 0;
 return 1;
}
static U8 fetch(EFI_SYSTEM_TABLE *st,Binding *binding,U32 corrupt,File *file) {
 EFI_BOOT_SERVICES *bs=st->boot_services;EFI_HANDLE child=0;Udp *udp=0;U8 matched=0;
 tx_token=(UdpToken){0};rx_token=(UdpToken){0};tx_done=rx_done=0;
 EFI_STATUS s=binding->create(binding,&child);number("UDP_CREATE status=",s);text("\n");save(file);
 if(EFI_ERROR(s) || !child)return 0;
 s=bs->handle_protocol(child,&udp_guid,(void **)&udp);number("UDP_PROTOCOL status=",s);text("\n");save(file);
 if(EFI_ERROR(s) || !udp)goto finish;
 UdpConfig config={0};config.ttl=64;config.no_fragment=1;config.receive_timeout=2000000;config.transmit_timeout=2000000;
 config.address[0]=10;config.address[1]=8;config.address[2]=22;config.address[3]=238;
 config.subnet[0]=config.subnet[1]=config.subnet[2]=255;config.port=18082;
 config.remote[0]=10;config.remote[1]=8;config.remote[2]=22;config.remote[3]=122;config.remote_port=18081;
 s=udp->configure(udp,&config);number("UDP_CONFIGURE status=",s);text("\n");save(file);
 if(EFI_ERROR(s))goto finish;
 s=((CreateEvent)bs->create_event)(0x200,8,done,(void *)&tx_done,&tx_token.event);if(EFI_ERROR(s))goto finish;
 s=((CreateEvent)bs->create_event)(0x200,8,done,(void *)&rx_done,&rx_token.event);if(EFI_ERROR(s))goto finish;
 for(U32 part=0;part<2;part++) {
  U8 request[16]={'C','M','P','N','E','T','0','1',(U8)part,0,0,0,(U8)corrupt,0,0,0};
  UdpTx packet={0,0,16,1,{{16,request}}};tx_done=rx_done=0;
  tx_token.status=rx_token.status=EFI_NOT_READY;tx_token.packet=&packet;rx_token.packet=0;
  s=udp->receive(udp,&rx_token);if(EFI_ERROR(s))goto finish;
  s=udp->transmit(udp,&tx_token);if(!EFI_ERROR(s))s=await_udp(bs,udp,&tx_token,&tx_done);
  number("UDP_TRANSMIT status=",s);number(" part=",part);text("\n");save(file);
  if(EFI_ERROR(s))goto finish;
  s=await_udp(bs,udp,&rx_token,&rx_done);number("UDP_RECEIVE status=",s);number(" part=",part);text("\n");save(file);
  if(EFI_ERROR(s))goto finish;
  UdpRx *rx=rx_token.packet;U8 accepted=accept_response(rx,request);
  if(accepted)for(U32 i=0;i<1024;i++)downloaded[part*1024+i]=response_bytes[16+i];
  if(rx && rx->recycle)((SignalEvent)bs->signal_event)(rx->recycle);
  rx_token.packet=0;
  if(!accepted){text("UDP_PACKET_REJECTED\n");save(file);goto finish;}
 }
 matched=exact_download();text(matched?"UDP_PAYLOAD_EXACT_MATCH\n":"UDP_PAYLOAD_REJECTED\n");save(file);
finish:
 if(udp){udp->cancel(udp,0);s=udp->configure(udp,0);number("UDP_RESET status=",s);text("\n");}
 s=binding->destroy(binding,child);number("UDP_DESTROY status=",s);text("\n");
 if(tx_token.event)((CloseEvent)bs->close_event)(tx_token.event);
 if(rx_token.event)((CloseEvent)bs->close_event)(rx_token.event);
 save(file);return matched && !EFI_ERROR(s);
}
static void udp_inventory(EFI_HANDLE parent,EFI_SYSTEM_TABLE *st,File *file) {
 EFI_BOOT_SERVICES *bs=st->boot_services;Binding *binding=0;
 EFI_STATUS s=bs->locate_protocol(&binding_guid,0,(void **)&binding);number("UDP_BINDING status=",s);text("\n");save(file);
 if(EFI_ERROR(s) || !binding)return;
 text("UDP_CORRUPT_CONTROL\n");save(file);
 if(fetch(st,binding,1,file)){text("UDP_CONTROL_UNEXPECTED_MATCH\n");save(file);return;}
 text("UDP_VALID_TRANSFER\n");save(file);
 if(!fetch(st,binding,0,file))return;
 EFI_HANDLE driver=0;s=((LoadImage)bs->load_image)(0,parent,0,downloaded,sizeof(downloaded),&driver);
 number("UDP_LOAD_IMAGE status=",s);text("\n");save(file);if(EFI_ERROR(s) || !driver)return;
 s=((StartImage)bs->start_image)(driver,0,0);number("UDP_START_IMAGE status=",s);text("\n");save(file);if(EFI_ERROR(s))return;
 CompanionExtension *service=0;s=bs->handle_protocol(driver,&udp_extension_guid,(void **)&service);
 number("UDP_CHILD_PROTOCOL status=",s);text("\n");save(file);if(EFI_ERROR(s) || !service || service->revision!=1 || !service->get_info)return;
 CompanionExtensionInfo info={0};UINTN bytes=sizeof(info);s=service->get_info(service,&bytes,&info);
 number("UDP_CHILD_INFO status=",s);number(" bytes=",bytes);number(" magic=",info.magic);number(" revision=",info.revision);number(" capabilities=",info.capabilities);text("\n");save(file);
}
_Static_assert(sizeof(UdpConfig)==36,"UDP config ABI");
_Static_assert(__builtin_offsetof(UdpConfig,port)==26,"UDP local port ABI");
_Static_assert(__builtin_offsetof(UdpConfig,remote_port)==32,"UDP remote port ABI");
_Static_assert(sizeof(UdpSession)==12,"UDP session ABI");
_Static_assert(__builtin_offsetof(UdpTx,fragments)==24,"UDP tx ABI");
_Static_assert(__builtin_offsetof(UdpRx,fragments)==48,"UDP rx ABI");

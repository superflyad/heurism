/* Bounded preboot HTTP transfer. Only the pinned owner driver may execute. */
#include "extension.h"
#include "../build/nv-extension/payload.h"
typedef struct File File;
static void http_inventory(EFI_HANDLE,EFI_SYSTEM_TABLE *,File *);
#define PROBE_EXTENSION(st,file) http_inventory(image,st,file)
#define PROBE_TAG "COMPANION_HTTP_TRANSFER_PROBE_01"
#define PROBE_DESCRIPTION "Bounded owner HTTP transfer; SSD management fallback retained.\n"
#define PROBE_REPORT_PATH u"\\EFI\\companion\\http-probe-01.txt"
#define PROBE_GRUB_PATH u"\\EFI\\companion\\recoveryx64.efi"
#include "firmware_probe.c"
typedef struct Binding Binding;
struct Binding {
 EFI_STATUS (EFIAPI *create)(Binding *,EFI_HANDLE *);
 EFI_STATUS (EFIAPI *destroy)(Binding *,EFI_HANDLE);
};
typedef struct { U8 use_default,address[4],subnet[4];U16 port; } HttpV4;
typedef struct { U32 version,timeout;U8 ipv6;HttpV4 *v4; } HttpConfig;
typedef struct { U32 method;U16 *url; } HttpRequest;
typedef struct { char *name,*value; } HttpHeader;
typedef struct { void *data;UINTN header_count;HttpHeader *headers;UINTN length;void *body; } HttpMessage;
typedef struct { EFI_EVENT event;EFI_STATUS status;HttpMessage *message; } HttpToken;
typedef struct Http Http;
struct Http {
 void *mode;
 EFI_STATUS (EFIAPI *configure)(Http *,HttpConfig *);
 EFI_STATUS (EFIAPI *request)(Http *,HttpToken *);
 EFI_STATUS (EFIAPI *cancel)(Http *,HttpToken *);
 EFI_STATUS (EFIAPI *response)(Http *,HttpToken *);
 EFI_STATUS (EFIAPI *poll)(Http *);
};
typedef EFI_STATUS (EFIAPI *CreateEvent)(U32,UINTN,void (EFIAPI *)(EFI_EVENT,void *),void *,EFI_EVENT *);
typedef EFI_STATUS (EFIAPI *CloseEvent)(EFI_EVENT);
static EFI_GUID http_binding_guid={0xbdc8e6af,0xd9bc,0x4379,{0xa7,0x2a,0xe0,0xc4,0xe7,0x5d,0xae,0x1c}};
static EFI_GUID http_guid={0x7a59b29b,0x910b,0x4171,{0x82,0x42,0xa8,0x5a,0x0d,0xf2,0x5b,0x5b}};
static EFI_GUID http_extension_guid=COMPANION_EXTENSION_GUID;
static U16 valid_url[]=u"http://10.8.22.122:18080/extension/companion-http-01";
static U16 corrupt_url[]=u"http://10.8.22.122:18080/corrupt/companion-http-01";
static U8 downloaded[sizeof(expected_payload)];
static volatile U32 request_done,response_done;
static HttpToken tx_token,rx_token;
static HttpMessage request_message,response_message;
static void EFIAPI completed(EFI_EVENT event,void *context){(void)event;*(volatile U32 *)context=1;}
static EFI_STATUS await_http(EFI_BOOT_SERVICES *bs,Http *http,HttpToken *token,volatile U32 *done) {
 for(U32 i=0;i<500 && !*done;i++){http->poll(http);bs->stall(10000);}
 if(!*done){http->cancel(http,token);return 0x8000000000000012ULL;}
 return token->status;
}
static U8 exact_download(UINTN bytes) {
 if(bytes!=sizeof(expected_payload))return 0;
 for(UINTN i=0;i<bytes;i++)if(downloaded[i]!=expected_payload[i])return 0;
 return 1;
}
static void release_headers(EFI_BOOT_SERVICES *bs,HttpMessage *m) {
 if(m->headers && m->header_count<=64) {
  for(UINTN i=0;i<m->header_count;i++){
   if(m->headers[i].name)((FreePool)bs->free_pool)(m->headers[i].name);
   if(m->headers[i].value)((FreePool)bs->free_pool)(m->headers[i].value);
  }
  ((FreePool)bs->free_pool)(m->headers);
 }
 m->headers=0;m->header_count=0;
}
static U8 fetch(EFI_SYSTEM_TABLE *st,Binding *binding,U16 *url,File *file) {
 EFI_BOOT_SERVICES *bs=st->boot_services;EFI_HANDLE child=0;Http *http=0;U8 matched=0;
 tx_token=(HttpToken){0};rx_token=(HttpToken){0};request_done=0;response_done=0;
 request_message=(HttpMessage){0};response_message=(HttpMessage){0};
 EFI_STATUS s=binding->create(binding,&child);number("HTTP_CREATE status=",s);text("\n");save(file);
 if(EFI_ERROR(s) || !child)return 0;
 s=bs->handle_protocol(child,&http_guid,(void **)&http);number("HTTP_PROTOCOL status=",s);text("\n");save(file);
 if(EFI_ERROR(s) || !http)goto finish;
 HttpV4 v4={0,{10,8,22,238},{255,255,255,0},0};HttpConfig config={1,2000,0,&v4};
 s=http->configure(http,&config);number("HTTP_CONFIGURE status=",s);text("\n");save(file);
 if(EFI_ERROR(s))goto finish;
 s=((CreateEvent)bs->create_event)(0x200,8,completed,(void *)&request_done,&tx_token.event);
 if(EFI_ERROR(s))goto finish;
 s=((CreateEvent)bs->create_event)(0x200,8,completed,(void *)&response_done,&rx_token.event);
 if(EFI_ERROR(s))goto finish;
 HttpRequest request={0,url};HttpHeader headers[]={{"Host","10.8.22.122:18080"},{"Connection","close"},{"User-Agent","CompanionUEFI/01"}};
 request_message=(HttpMessage){&request,3,headers,0,0};tx_token.status=EFI_NOT_READY;tx_token.message=&request_message;
 s=http->request(http,&tx_token);number("HTTP_REQUEST_QUEUE status=",s);text("\n");save(file);
 if(EFI_ERROR(s))goto finish;
 s=await_http(bs,http,&tx_token,&request_done);number("HTTP_REQUEST_COMPLETE status=",s);text("\n");save(file);
 if(EFI_ERROR(s))goto finish;
 U32 status_code=0;UINTN total=0;
 for(U32 piece=0;piece<4 && total<sizeof(downloaded);piece++) {
  response_done=0;response_message=(HttpMessage){piece?0:&status_code,0,0,sizeof(downloaded)-total,downloaded+total};
  rx_token.status=EFI_NOT_READY;rx_token.message=&response_message;
  s=http->response(http,&rx_token);
  if(!EFI_ERROR(s))s=await_http(bs,http,&rx_token,&response_done);
  number("HTTP_RESPONSE status=",s);number(" code=",status_code);number(" bytes=",response_message.length);text("\n");save(file);
  if(EFI_ERROR(s) || status_code!=3 || !response_message.length || response_message.length>sizeof(downloaded)-total || response_message.header_count>64)goto finish;
  total+=response_message.length;release_headers(bs,&response_message);
 }
 matched=exact_download(total);text(matched?"HTTP_PAYLOAD_EXACT_MATCH\n":"HTTP_PAYLOAD_REJECTED\n");save(file);
finish:
 if(http){http->cancel(http,0);s=http->configure(http,0);number("HTTP_RESET status=",s);text("\n");}
 s=binding->destroy(binding,child);number("HTTP_DESTROY status=",s);text("\n");
 if(tx_token.event)((CloseEvent)bs->close_event)(tx_token.event);
 if(rx_token.event)((CloseEvent)bs->close_event)(rx_token.event);
 release_headers(bs,&response_message);save(file);
 return matched && !EFI_ERROR(s);
}
static void http_inventory(EFI_HANDLE parent,EFI_SYSTEM_TABLE *st,File *file) {
 EFI_BOOT_SERVICES *bs=st->boot_services;Binding *binding=0;
 EFI_STATUS s=bs->locate_protocol(&http_binding_guid,0,(void **)&binding);
 number("HTTP_BINDING status=",s);text("\n");save(file);
 if(EFI_ERROR(s) || !binding)return;
 /* A corrupted response must be rejected before any owner image executes. */
 text("HTTP_CORRUPT_CONTROL\n");save(file);
 if(fetch(st,binding,corrupt_url,file)){text("HTTP_CONTROL_UNEXPECTED_MATCH\n");save(file);return;}
 text("HTTP_VALID_TRANSFER\n");save(file);
 if(!fetch(st,binding,valid_url,file))return;
 EFI_HANDLE driver=0;
 s=((LoadImage)bs->load_image)(0,parent,0,downloaded,sizeof(downloaded),&driver);
 number("HTTP_LOAD_IMAGE status=",s);text("\n");save(file);
 if(EFI_ERROR(s) || !driver)return;
 s=((StartImage)bs->start_image)(driver,0,0);number("HTTP_START_IMAGE status=",s);text("\n");save(file);
 if(EFI_ERROR(s))return;
 CompanionExtension *service=0;s=bs->handle_protocol(driver,&http_extension_guid,(void **)&service);
 number("HTTP_CHILD_PROTOCOL status=",s);text("\n");save(file);
 if(EFI_ERROR(s) || !service || service->revision!=1 || !service->get_info)return;
 CompanionExtensionInfo info={0};UINTN bytes=sizeof(info);
 s=service->get_info(service,&bytes,&info);number("HTTP_CHILD_INFO status=",s);number(" bytes=",bytes);
 number(" magic=",info.magic);number(" revision=",info.revision);number(" capabilities=",info.capabilities);text("\n");save(file);
}
_Static_assert(sizeof(HttpV4)==12,"HTTPv4 ABI");
_Static_assert(__builtin_offsetof(HttpConfig,v4)==16,"HTTP config ABI");
_Static_assert(__builtin_offsetof(HttpMessage,body)==32,"HTTP message ABI");

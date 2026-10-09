#include "../boot/http_transfer_probe.c"
static EFI_BOOT_SERVICES mock_bs;
static EFI_SYSTEM_TABLE mock_st;
static Binding mock_binding;
static Http mock_http;
static CompanionExtension mock_service;
static HttpToken *pending;
static void *contexts[2];
static void (EFIAPI *notifications[2])(EFI_EVENT,void *);
static U32 corrupt,offset,events,closed,destroyed,loads,starts,stalls,fail,scenario;
static EFI_STATUS EFIAPI create(Binding *b,EFI_HANDLE *h){if(b!=&mock_binding)fail=1;*h=(EFI_HANDLE)2;events=0;offset=0;return 0;}
static EFI_STATUS EFIAPI destroy(Binding *b,EFI_HANDLE h){if(b!=&mock_binding || h!=(EFI_HANDLE)2)fail=2;destroyed++;pending=0;return 0;}
static EFI_STATUS EFIAPI locate(EFI_GUID *g,void *search,void **out){if(g!=&http_binding_guid || search)fail=3;*out=&mock_binding;return 0;}
static EFI_STATUS EFIAPI protocol(EFI_HANDLE h,EFI_GUID *g,void **out){
 if(h==(EFI_HANDLE)2 && g==&http_guid)*out=&mock_http;
 else if(h==(EFI_HANDLE)3 && g==&http_extension_guid)*out=&mock_service;
 else return EFI_UNSUPPORTED;return 0;
}
static EFI_STATUS EFIAPI configure(Http *h,HttpConfig *c){
 if(h!=&mock_http)fail=4;if(!c){pending=0;return 0;}
 if(c->version!=1 || c->timeout!=2000 || c->ipv6 || c->v4->use_default || c->v4->address[3]!=238)fail=5;
 return scenario==2?EFI_UNSUPPORTED:0;
}
static EFI_STATUS EFIAPI event(U32 type,UINTN tpl,void (EFIAPI *notify)(EFI_EVENT,void *),void *context,EFI_EVENT *out){
 if(type!=0x200 || tpl!=8 || events>=2){fail=6;return EFI_UNSUPPORTED;}
 contexts[events]=context;notifications[events]=notify;*out=(EFI_EVENT)(UINTN)(++events);return 0;
}
static EFI_STATUS EFIAPI close_event(EFI_EVENT e){if(pending || !destroyed || !e)fail=7;closed++;return 0;}
static EFI_STATUS EFIAPI request(Http *h,HttpToken *t){
 if(h!=&mock_http || !t->event || !t->message->data)fail=8;
 corrupt=((HttpRequest *)t->message->data)->url==corrupt_url;pending=t;return 0;
}
static EFI_STATUS EFIAPI response(Http *h,HttpToken *t){
 if(h!=&mock_http || t->message->length!=sizeof(downloaded)-offset || t->message->body!=downloaded+offset)fail=9;
 UINTN count=scenario==3?0:1024;
 if(count)for(UINTN i=0;i<count;i++)((U8 *)t->message->body)[i]=expected_payload[offset+i];
 if(corrupt && offset==0 && count)((U8 *)t->message->body)[0]^=1;
 if(t->message->data)*(U32 *)t->message->data=3;
 t->message->length=count;offset+=(U32)count;pending=t;return 0;
}
static EFI_STATUS EFIAPI poll(Http *h){
 if(h!=&mock_http)fail=10;
 if(pending && scenario!=1){HttpToken *t=pending;pending=0;t->status=0;UINTN i=(UINTN)t->event-1;notifications[i](t->event,contexts[i]);}
 return 0;
}
static EFI_STATUS EFIAPI cancel(Http *h,HttpToken *t){if(h!=&mock_http)fail=11;(void)t;pending=0;return 0;}
static EFI_STATUS EFIAPI stall(UINTN n){if(n!=10000)fail=12;stalls++;return 0;}
static EFI_STATUS EFIAPI release(void *p){(void)p;return 0;}
static EFI_STATUS EFIAPI load(U8 policy,EFI_HANDLE parent,void *path,void *buffer,UINTN bytes,EFI_HANDLE *child){
 if(policy || parent!=(EFI_HANDLE)1 || path || buffer!=downloaded || buffer==(void *)expected_payload || bytes!=2048 || !exact_download(bytes))fail=13;
 loads++;*child=(EFI_HANDLE)3;return 0;
}
static EFI_STATUS EFIAPI start(EFI_HANDLE h,UINTN *n,U16 **p){if(h!=(EFI_HANDLE)3 || n || p)fail=14;starts++;return 0;}
static EFI_STATUS EFIAPI info(CompanionExtension *s,UINTN *n,CompanionExtensionInfo *out){
 if(s!=&mock_service || *n!=32)fail=15;out->magic=COMPANION_EXTENSION_MAGIC;out->revision=1;out->capabilities=1;return 0;
}
int http_run_tests(void){
 mock_bs.locate_protocol=locate;mock_bs.handle_protocol=protocol;mock_bs.create_event=(void *)event;mock_bs.close_event=(void *)close_event;
 mock_bs.stall=stall;mock_bs.free_pool=(void *)release;mock_bs.load_image=(void *)load;mock_bs.start_image=(void *)start;
 mock_st.boot_services=&mock_bs;mock_binding.create=create;mock_binding.destroy=destroy;
 mock_http.configure=configure;mock_http.request=request;mock_http.response=response;mock_http.cancel=cancel;mock_http.poll=poll;
 mock_service.revision=1;mock_service.get_info=info;
 http_inventory((EFI_HANDLE)1,&mock_st,0);
 if(fail || loads!=1 || starts!=1 || destroyed!=2 || closed!=4)return 100+(int)fail;
 scenario=1;U32 previous=stalls;
 if(fetch(&mock_st,&mock_binding,valid_url,0) || stalls-previous!=500 || destroyed!=3 || closed!=6 || fail)return 200+(int)fail;
 scenario=2;if(fetch(&mock_st,&mock_binding,valid_url,0) || destroyed!=4 || closed!=6 || fail)return 300+(int)fail;
 scenario=3;if(fetch(&mock_st,&mock_binding,valid_url,0) || destroyed!=5 || closed!=8 || fail)return 400+(int)fail;
 return 0;
}

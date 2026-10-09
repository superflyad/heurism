#include "management.h"
#include "../common/sha256.h"
static U64 big(const U8 *p,U32 n) { U64 v=0;while(n--) v=v<<8|*p++;return v; }
static void put(U8 *p,U64 value,U32 n) { while(n) { --n;p[n]=(U8)value;value>>=8; } }
static int nonzero(const U8 *p,U32 n) { U8 sum=0;while(n--) sum|=*p++;return !!sum; }
int management_init(Management *m,const U8 key[32],const U8 nonce[16]) {
    if(!m) return 0;*m=(Management){0};
    if(!key || !nonce || !nonzero(key,32) || !nonzero(nonce,16)) return 0;
    for(U32 i=0;i<32;++i) m->key[i]=key[i];for(U32 i=0;i<16;++i) m->boot_nonce[i]=nonce[i];m->ready=1;return 1;
}
U32 management_reply(Management *m,const U8 *request,U32 length,const ManagementStatus *s,U8 *out,U32 capacity,U8 *action) {
    if(action) *action=0;
    if(!m || !m->ready || !request || !s || !out || !action || capacity<MANAGEMENT_RESPONSE_SIZE) return 0;
    if(length!=MANAGEMENT_REQUEST_SIZE || request[0]!='C' || request[1]!='M' || request[2]!='P' || request[3]!='1' ||
       request[4]!=1 || request[5]<1 || request[5]>3 || request[6] || request[7] || !nonzero(request+32,16)) goto rejected;
    U8 tag[32];hmac_sha256(m->key,32,request,48,tag);if(!bytes_equal_constant(tag,request+48,32)) goto rejected;
    U64 sequence=big(request+8,8);U8 operation=request[5];
    if(operation==1) { if(sequence || nonzero(request+16,16)) goto rejected; }
    else if(!bytes_equal_constant(request+16,m->boot_nonce,16) || !sequence || sequence<=m->last_sequence) goto rejected;
    for(U32 i=0;i<48;++i) out[i]=request[i];out[5]|=0x80;
    for(U32 i=0;i<16;++i) out[16+i]=m->boot_nonce[i];
    if(operation!=1) m->last_sequence=sequence;
    put(out+48,s->ticks,8);put(out+56,s->free_pages,8);put(out+64,s->received,8);put(out+72,s->transmitted,8);put(out+80,m->last_sequence,8);
    hmac_sha256(m->key,32,out,88,out+88);++m->accepted;
    if(operation==3) *action=3;return MANAGEMENT_RESPONSE_SIZE;
rejected:
    ++m->rejected;return 0;
}

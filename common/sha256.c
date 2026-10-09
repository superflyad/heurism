#include "sha256.h"
typedef struct { U32 state[8];U64 bytes;U8 block[64];U32 used; } Sha;
static const U32 constants[64]={
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
 0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
 0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
 0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
 0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static U32 rotate(U32 x,U32 n) { return (x>>n)|(x<<(32-n)); }
static void compress(Sha *s) {
    U32 w[64],a=s->state[0],b=s->state[1],c=s->state[2],d=s->state[3],e=s->state[4],f=s->state[5],g=s->state[6],h=s->state[7];
    for(U32 i=0;i<16;++i) w[i]=(U32)s->block[i*4]<<24|(U32)s->block[i*4+1]<<16|(U32)s->block[i*4+2]<<8|s->block[i*4+3];
    for(U32 i=16;i<64;++i) {
        U32 x=w[i-15],y=w[i-2];
        w[i]=w[i-16]+(rotate(x,7)^rotate(x,18)^(x>>3))+w[i-7]+(rotate(y,17)^rotate(y,19)^(y>>10));
    }
    for(U32 i=0;i<64;++i) {
        U32 t1=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+constants[i]+w[i];
        U32 t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    s->state[0]+=a;s->state[1]+=b;s->state[2]+=c;s->state[3]+=d;s->state[4]+=e;s->state[5]+=f;s->state[6]+=g;s->state[7]+=h;
}
static void init(Sha *s) { *s=(Sha){{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19},0,{0},0}; }
static void update(Sha *s,const U8 *data,U32 length) {
    s->bytes+=length;
    while(length--) { s->block[s->used++]=*data++;if(s->used==64) { compress(s);s->used=0; } }
}
static void finish(Sha *s,U8 out[32]) {
    U64 bits=s->bytes*8;s->block[s->used++]=0x80;
    if(s->used>56) { while(s->used<64) s->block[s->used++]=0;compress(s);s->used=0; }
    while(s->used<56) s->block[s->used++]=0;
    for(U32 i=0;i<8;++i) s->block[56+i]=(U8)(bits>>(56-8*i));compress(s);
    for(U32 i=0;i<32;++i) out[i]=(U8)(s->state[i/4]>>(24-8*(i%4)));
}
void sha256(const U8 *data,U32 length,U8 out[32]) { Sha s;init(&s);update(&s,data,length);finish(&s,out); }
void hmac_sha256(const U8 *key,U32 key_length,const U8 *data,U32 length,U8 out[32]) {
    U8 normalized[64]={0},inner[32],pad[64];Sha s;
    if(key_length>64) sha256(key,key_length,normalized);
    else for(U32 i=0;i<key_length;++i) normalized[i]=key[i];
    for(U32 i=0;i<64;++i) pad[i]=normalized[i]^0x36;
    init(&s);update(&s,pad,64);update(&s,data,length);finish(&s,inner);
    for(U32 i=0;i<64;++i) pad[i]=normalized[i]^0x5c;
    init(&s);update(&s,pad,64);update(&s,inner,32);finish(&s,out);
    volatile U8 *wipe=normalized;for(U32 i=0;i<64;++i) wipe[i]=0;
    wipe=pad;for(U32 i=0;i<64;++i) wipe[i]=0;wipe=inner;for(U32 i=0;i<32;++i) wipe[i]=0;
}
int bytes_equal_constant(const U8 *a,const U8 *b,U32 length) {
    volatile U8 mismatch=0;for(U32 i=0;i<length;++i) mismatch|=a[i]^b[i];return !mismatch;
}

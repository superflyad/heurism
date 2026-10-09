#include "../drivers/input/i8042.h"
#define CHECK(x) do { if(!(x)) return __LINE__; } while(0)
typedef struct { U8 bytes[64],status[64];U32 head,tail,mode,writes,resends;U8 config_write,config; } Mock;
static void queue(Mock *m,U8 byte,U8 status) { m->bytes[m->tail]=byte;m->status[m->tail++]=status; }
static U8 read_port(void *context,U16 port) {
    Mock *m=context;
    if(port==0x64) return m->mode==1?2:m->mode==5?255:m->head<m->tail?(m->status[m->head]|1):0;
    return m->head<m->tail?m->bytes[m->head++]:0;
}
static void write_port(void *context,U16 port,U8 byte) {
    Mock *m=context;++m->writes;
    if(port==0x64 && byte==0x20) queue(m,0x31,0);
    if(port==0x64 && byte==0x60) m->config_write=1;
    if(port==0x60 && m->config_write) { m->config=byte;m->config_write=0; }
    else if(port==0x60 && byte==0xf4 && m->mode!=4) queue(m,m->mode==3?0xfc:m->mode==2 && m->resends++<2?0xfe:0xfa,0);
}
int run_i8042_tests(void) {
    Mock m={0};I8042Io io={&m,read_port,write_port};I8042Keyboard k;KeyEvent e;
    CHECK(i8042_start(&k,&io) && k.ready && m.config==0x60);
    CHECK(!i8042_poll(&k,&e));
    queue(&m,0x1e,0x20);queue(&m,0x30,0x80);queue(&m,0x1e,0);
    CHECK(i8042_poll(&k,&e) && e.pressed && e.character=='a');
    CHECK(i8042_decode(&k,0x2a,&e) && !e.character && k.left_shift);
    CHECK(i8042_decode(&k,0x36,&e) && k.right_shift);
    CHECK(i8042_decode(&k,0xaa,&e) && !e.pressed && !k.left_shift && k.right_shift);
    CHECK(i8042_decode(&k,0x30,&e) && e.character=='B');
    CHECK(i8042_decode(&k,0xb6,&e) && !k.right_shift);
    CHECK(i8042_decode(&k,0x3a,&e) && k.caps);
    CHECK(i8042_decode(&k,0x3a,&e) && k.caps);
    CHECK(i8042_decode(&k,0x20,&e) && e.character=='D');
    CHECK(i8042_decode(&k,0xba,&e) && !k.caps_down);
    CHECK(i8042_decode(&k,0x2a,&e));CHECK(i8042_decode(&k,0x20,&e) && e.character=='d');
    CHECK(i8042_decode(&k,0x02,&e) && e.character=='!');
    CHECK(!i8042_decode(&k,0xe0,&e));CHECK(i8042_decode(&k,0x2a,&e) && e.code==0x12a && k.left_shift);
    CHECK(!i8042_decode(&k,0xe0,&e));CHECK(i8042_decode(&k,0x48,&e) && e.code==0x148 && !e.character);
    CHECK(!i8042_decode(&k,0xe1,&e));for(U32 i=0;i<5;++i) CHECK(!i8042_decode(&k,0x1e,&e));
    CHECK(i8042_decode(&k,0xaa,&e) && !k.left_shift);
    CHECK(!i8042_decode(&k,0xfa,&e) && !i8042_decode(&k,0xfe,&e));
    CHECK(i8042_decode(&k,0x9e,&e) && !e.pressed && !e.character);
    for(U32 mode=1;mode<=5;++mode) {
        m=(Mock){0};m.mode=mode;
        int ok=i8042_start(&k,&io);
        CHECK((mode==2)==!!ok && (mode==2)==!!k.ready);
        if(mode==1 || mode==5) CHECK(!m.writes);
    }
    CHECK(!i8042_start(&k,0) && !k.ready);
    CHECK(!i8042_decode(0,1,&e));
    return 0;
}

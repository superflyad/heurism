#include "i8042.h"
#define LIMIT 100000
static int input_empty(const I8042Io *io) {
    for(U32 i=0;i<LIMIT;++i) { U8 s=io->read(io->context,0x64);if(s==255) return 0;if(!(s&2)) return 1; }return 0;
}
static int write_byte(const I8042Io *io,U16 port,U8 byte) { if(!input_empty(io)) return 0;io->write(io->context,port,byte);return 1; }
static int response(const I8042Io *io,U8 *byte) {
    for(U32 i=0;i<LIMIT;++i) {
        U8 s=io->read(io->context,0x64);if(s==255) return 0;
        if(s&1) { U8 value=io->read(io->context,0x60);if(s&0xe0) continue;*byte=value;return 1; }
    }return 0;
}
int i8042_start(I8042Keyboard *keyboard,const I8042Io *io) {
    if(!keyboard) return 0;*keyboard=(I8042Keyboard){0};
    if(!io || !io->read || !io->write || !write_byte(io,0x64,0xad)) return 0;
    for(U32 i=0;i<32;++i) { if(!(io->read(io->context,0x64)&1)) break;(void)io->read(io->context,0x60); }
    if(io->read(io->context,0x64)&1) return 0;
    U8 config;
    if(!write_byte(io,0x64,0x20) || !response(io,&config) || !write_byte(io,0x64,0x60) ||
       !write_byte(io,0x60,(config|0x40)&~0x13U) || !write_byte(io,0x64,0xae)) return 0;
    for(U32 retry=0;retry<3;++retry) {
        U8 answer;
        if(!write_byte(io,0x60,0xf4) || !response(io,&answer)) return 0;
        if(answer==0xfa) { keyboard->io=*io;keyboard->ready=1;return 1; }
        if(answer!=0xfe) return 0;
    }
    return 0;
}
int i8042_decode(I8042Keyboard *k,U8 byte,KeyEvent *event) {
    if(!k || !event) return 0;
    if(k->pause_bytes) { --k->pause_bytes;return 0; }
    if(byte==0xe1) { k->pause_bytes=5;k->extended=0;return 0; }
    if(byte==0xe0) { k->extended=1;return 0; }
    if(byte==0xfa || byte==0xfe || byte==0 || byte==255) { k->extended=0;return 0; }
    U8 extended=k->extended,code=byte&127,pressed=!(byte&128);k->extended=0;
    *event=(KeyEvent){(U16)(code|(extended?256:0)),pressed,0};
    if(!extended && code==0x2a) k->left_shift=pressed;
    if(!extended && code==0x36) k->right_shift=pressed;
    if(!extended && code==0x3a) { if(pressed && !k->caps_down) k->caps^=1;k->caps_down=pressed; }
    if(!pressed || extended) return 1;
    static const char normal[128]={
        [1]=27,[2]='1',[3]='2',[4]='3',[5]='4',[6]='5',[7]='6',[8]='7',[9]='8',[10]='9',[11]='0',[12]='-',[13]='=',[14]=8,[15]=9,
        [16]='q',[17]='w',[18]='e',[19]='r',[20]='t',[21]='y',[22]='u',[23]='i',[24]='o',[25]='p',[26]='[',[27]=']',[28]=13,
        [30]='a',[31]='s',[32]='d',[33]='f',[34]='g',[35]='h',[36]='j',[37]='k',[38]='l',[39]=';',[40]='\'',[41]='`',[43]='\\',
        [44]='z',[45]='x',[46]='c',[47]='v',[48]='b',[49]='n',[50]='m',[51]=',',[52]='.',[53]='/',[55]='*',[57]=' '
    };
    static const char shifted[128]={ [2]='!',[3]='@',[4]='#',[5]='$',[6]='%',[7]='^',[8]='&',[9]='*',[10]='(',[11]=')',[12]='_',[13]='+',[26]='{',[27]='}',[39]=':',[40]='"',[41]='~',[43]='|',[51]='<',[52]='>',[53]='?' };
    U8 c=(U8)normal[code],shift=k->left_shift || k->right_shift;
    if(c>='a' && c<='z') { if(shift^k->caps) c-='a'-'A'; }
    else if(shift && shifted[code]) c=(U8)shifted[code];
    event->character=c;return 1;
}
int i8042_poll(I8042Keyboard *k,KeyEvent *event) {
    if(!k || !event || !k->ready) return 0;
    for(U32 i=0;i<32;++i) {
        U8 status=k->io.read(k->io.context,0x64);if(!(status&1) || status==255) return 0;
        U8 byte=k->io.read(k->io.context,0x60);
        if(status&0xe0) { if(!(status&0x20)) k->extended=k->pause_bytes=0;continue; }
        if(i8042_decode(k,byte,event)) return 1;
    }return 0;
}

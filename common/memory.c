/* Compiler-generated freestanding aggregate copies/initialization use these. */
void *memset(void *dest,int value,__SIZE_TYPE__ size) {
    volatile unsigned char *d=dest;
    while(size--) *d++=(unsigned char)value;
    return dest;
}
void *memcpy(void *dest,const void *source,__SIZE_TYPE__ size) {
    volatile unsigned char *d=dest;
    const volatile unsigned char *s=source;
    while(size--) *d++=*s++;
    return dest;
}

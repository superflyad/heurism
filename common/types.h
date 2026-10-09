#ifndef COMPANION_TYPES_H
#define COMPANION_TYPES_H
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long long U64;
_Static_assert(sizeof(U8)==1 && sizeof(U16)==2 && sizeof(U32)==4 && sizeof(U64)==8, "integer widths");
#endif

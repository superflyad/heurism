#ifndef COMPANION_SHA256_H
#define COMPANION_SHA256_H
#include "types.h"
void sha256(const U8 *,U32,U8 [32]);
void hmac_sha256(const U8 *,U32,const U8 *,U32,U8 [32]);
int bytes_equal_constant(const U8 *,const U8 *,U32);
#endif

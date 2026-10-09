#ifndef COMPANION_MANAGEMENT_H
#define COMPANION_MANAGEMENT_H
#include "../common/types.h"
#define MANAGEMENT_PORT 47333
#define MANAGEMENT_REQUEST_SIZE 80
#define MANAGEMENT_RESPONSE_SIZE 120
typedef struct { U8 key[32],boot_nonce[16];U64 last_sequence,accepted,rejected;U8 ready; } Management;
typedef struct { U64 ticks,free_pages,received,transmitted; } ManagementStatus;
int management_init(Management *,const U8 [32],const U8 [16]);
/* Returns response length, or 0; action is 3 only after an authenticated reboot. */
U32 management_reply(Management *,const U8 *,U32,const ManagementStatus *,U8 *,U32,U8 *);
#endif

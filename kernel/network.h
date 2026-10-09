#ifndef COMPANION_NETWORK_H
#define COMPANION_NETWORK_H
#include "management.h"
typedef struct { U8 mac[6],ip[4];Management management;U64 accepted,dropped; } Network;
/* One input frame, at most one bounded reply. No fragmentation, DHCP or routing. */
U32 network_reply(Network *,const U8 *,U32,const ManagementStatus *,U8 *,U32,U8 *);
#endif

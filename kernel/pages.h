#ifndef COMPANION_PAGES_H
#define COMPANION_PAGES_H
#include "boot_info.h"
int pages_init(const BootInfo *);
U64 page_alloc(void);
int page_free(U64);
U64 pages_available(void);
#endif

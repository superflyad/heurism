#ifndef COMPANION_SPARSE_IDENTITY_H
#define COMPANION_SPARSE_IDENTITY_H
#include "../../common/types.h"
#define IDENTITY_LOW_LIMIT (64ULL<<30)
#define IDENTITY_CANONICAL_LIMIT (1ULL<<47)
#define IDENTITY_EXTRA_PDPT 8U
#define IDENTITY_EXTRA_PD 16U
#define IDENTITY_MAP_MAX (1ULL<<30)
/* Additional page tables are kernel-owned low memory. Unrequested high
 * addresses stay absent. No allocation or firmware service is needed. */
typedef struct {
    U64 *root,*low_pdpt,*low_pd;
    U64 limit;
    U32 pdpt_used,pd_used;
    U64 pdpt[IDENTITY_EXTRA_PDPT][512] __attribute__((aligned(4096)));
    U64 pd[IDENTITY_EXTRA_PD][512] __attribute__((aligned(4096)));
} SparseIdentity;
int sparse_identity_init(SparseIdentity *,U64 *,U64 *,U64 *,U32);
/* Caller has validated MMIO ownership and the complete 2-MiB envelope,
 * disabled interrupts and flushed caches. This mutates only owned tables.
 * Failure changes no table or pool count. Leaves are supervisor/RW/UC/NX. */
int sparse_identity_uc(SparseIdentity *,U64,U64);
#endif

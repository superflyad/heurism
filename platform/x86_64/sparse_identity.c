#include "sparse_identity.h"
#define ADDRESS 0x000ffffffffff000ULL
#define HUGE_ADDRESS 0x000fffffffe00000ULL
#define LEAF_FLAGS ((1ULL<<63)|0x9bULL)
static void zero(U64 *p) { for(U32 i=0;i<512;++i) p[i]=0; }
static int low_page(U64 address,U64 size) {
    return address && !(address&4095) && address<IDENTITY_LOW_LIMIT && size<=IDENTITY_LOW_LIMIT-address;
}
int sparse_identity_init(SparseIdentity *s,U64 *root,U64 *pdpt,U64 *pd,U32 bits) {
    if(!s || bits<36 || bits>52 || !low_page((U64)s,sizeof(*s)) ||
       !low_page((U64)root,4096) || !low_page((U64)pdpt,4096) || !low_page((U64)pd,64*4096)) return 0;
    s->root=root;s->low_pdpt=pdpt;s->low_pd=pd;s->pdpt_used=s->pd_used=0;
    s->limit=1ULL<<(bits<47?bits:47);
    for(U32 i=0;i<IDENTITY_EXTRA_PDPT;++i) zero(s->pdpt[i]);
    for(U32 i=0;i<IDENTITY_EXTRA_PD;++i) zero(s->pd[i]);
    return 1;
}
static int table_entry(U64 entry) {
    return (entry&3)==3 && !(entry&~(ADDRESS|0x23ULL)); /* allow hardware Accessed */
}
static U64 *owned_pdpt(SparseIdentity *s,U32 index) {
    U64 entry=s->root[index];if(!table_entry(entry)) return 0;
    U64 address=entry&ADDRESS;
    if(!index && address==(U64)s->low_pdpt) return s->low_pdpt;
    for(U32 i=0;i<s->pdpt_used;++i) if(address==(U64)s->pdpt[i]) return s->pdpt[i];
    return 0;
}
static U64 *owned_pd(SparseIdentity *s,U64 key,U64 entry) {
    if(!table_entry(entry)) return 0;U64 address=entry&ADDRESS;
    if(key<64 && address==(U64)(s->low_pd+key*512)) return s->low_pd+key*512;
    for(U32 i=0;i<s->pd_used;++i) if(address==(U64)s->pd[i]) return s->pd[i];
    return 0;
}
int sparse_identity_uc(SparseIdentity *s,U64 base,U64 size) {
    if(!s || !s->root || !s->low_pdpt || !s->low_pd || s->pdpt_used>IDENTITY_EXTRA_PDPT ||
       s->pd_used>IDENTITY_EXTRA_PD || !size || size>IDENTITY_MAP_MAX || (base&0x1fffff) ||
       (size&0x1fffff) || base>=s->limit || size>s->limit-base || s->limit>IDENTITY_CANONICAL_LIMIT) return 0;
    /* At most two 1-GiB directory spans for this bounded request. Preflight
     * every old leaf and all capacity before changing any reachable table. */
    U64 first=base>>30,last=(base+size-1)>>30,keys[2];U64 *dirs[2];
    U32 roots[2],new_roots=0,new_dirs=0,count=(U32)(last-first+1);
    if(count>2) return 0;
    for(U32 i=0;i<count;++i) {
        U64 key=first+i;keys[i]=key;dirs[i]=0;U32 root=(U32)(key>>9);
        if(root>=256) return 0;
        U64 *table=owned_pdpt(s,root);
        if(!table) {
            if(s->root[root]) return 0;
            if(!new_roots || roots[new_roots-1]!=root) roots[new_roots++]=root;
        } else if(table[key&511]) {
            dirs[i]=owned_pd(s,key,table[key&511]);if(!dirs[i]) return 0;
        }
        if(!dirs[i]) ++new_dirs;
    }
    if(new_roots>IDENTITY_EXTRA_PDPT-s->pdpt_used || new_dirs>IDENTITY_EXTRA_PD-s->pd_used) return 0;
    for(U64 p=base;p<base+size;p+=0x200000) {
        U64 *dir=dirs[(p>>30)-first];if(!dir) continue;
        U64 entry=dir[(p>>21)&511];
        if(entry && ((entry&0x83)!=0x83 || (entry&HUGE_ADDRESS)!=p ||
                     (entry&~(HUGE_ADDRESS|(1ULL<<63)|0xfbULL)))) return 0;
    }
    /* No failure remains possible. Initialize children before publishing their
     * parent pointers; the caller reloads CR3 after this single-CPU update. */
    U32 root_indices[2];
    for(U32 i=0;i<new_roots;++i) { root_indices[i]=s->pdpt_used++;zero(s->pdpt[root_indices[i]]); }
    for(U32 i=0;i<count;++i) if(!dirs[i]) { dirs[i]=s->pd[s->pd_used++];zero(dirs[i]); }
    for(U64 p=base;p<base+size;p+=0x200000) dirs[(p>>30)-first][(p>>21)&511]=p|LEAF_FLAGS;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    for(U32 i=0;i<count;++i) {
        U32 root=(U32)(keys[i]>>9);U64 *table=owned_pdpt(s,root);
        if(!table) for(U32 j=0;j<new_roots;++j) if(roots[j]==root) table=s->pdpt[root_indices[j]];
        table[keys[i]&511]=(U64)dirs[i]|3;
    }
    __atomic_thread_fence(__ATOMIC_RELEASE);
    for(U32 i=0;i<new_roots;++i) s->root[roots[i]]=(U64)s->pdpt[root_indices[i]]|3;
    return 1;
}

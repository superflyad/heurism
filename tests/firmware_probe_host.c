/* Exercise the probe's EFI ABI, report bounds, and return-to-GRUB path. */
#if defined(TEST_HTTP_INSPECT_PROBE) || defined(TEST_HTTP_PREREQ_PROBE) || defined(TEST_HTTP_INIT_PROBE) || defined(TEST_HTTP_ETHERNET_PROBE) || defined(TEST_HTTP_CLEANUP_PROBE)
#ifdef TEST_HTTP_CLEANUP_PROBE
#include "../boot/http_cleanup_probe.c"
#elif defined(TEST_HTTP_ETHERNET_PROBE)
#include "../boot/http_ethernet_probe.c"
#elif defined(TEST_HTTP_INIT_PROBE)
#include "../boot/http_init_probe.c"
#else
#ifdef TEST_HTTP_PREREQ_PROBE
#include "../boot/http_prereq_probe.c"
#else
#include "../boot/http_inspect_probe.c"
#endif
#endif
static EFI_STATUS EFIAPI mock_inspect_database(EFI_GUID *g,void *registration,void **out) {
 (void)g;(void)registration;*out=0;return 0x800000000000000eULL;
}
#elif defined(TEST_UPDATE_PROBE)
#include "../boot/update_probe.c"
static Fmp mock_fmp;
static EFI_HANDLE mock_fmp_handles[1];
static U32 fmp_calls, fmp_bad_bounds, fmp_not_found;
static EFI_STATUS EFIAPI mock_fmp_info(Fmp *p,UINTN *bytes,void *buffer,U32 *version,
                                     U8 *count,UINTN *stride,U32 *package,U16 **name) {
    if(p!=&mock_fmp || *bytes!=sizeof(info_buffer))return EFI_UNSUPPORTED;
    Descriptor *d=buffer;d->index=1;d->type=fmp_guid;d->size=0x1000000;
    d->version=0x12300;d->supported=7;d->setting=7;d->lowest=0x12300;
    *version=3;*count=1;*stride=fmp_bad_bounds?16:sizeof(Descriptor);
    *bytes=sizeof(Descriptor);*package=0xffffffff;*name=0;fmp_calls++;return 0;
}
#elif defined(TEST_EXTENSION_PROBE)
#include "../boot/extension_probe.c"
static CompanionExtension mock_extension;
static U32 extension_calls;
static EFI_STATUS EFIAPI mock_extension_info(CompanionExtension *self,UINTN *bytes,CompanionExtensionInfo *out) {
 if(self!=&mock_extension || *bytes!=sizeof(*out))return EFI_UNSUPPORTED;
 out->magic=COMPANION_EXTENSION_MAGIC;out->revision=1;out->capabilities=1;
 extension_calls++;return 0;
}
static EFI_STATUS EFIAPI mock_extension_locate(EFI_GUID *g,void *registration,void **out) {
 if(!equal(g,&extension_guid) || registration)return EFI_UNSUPPORTED;
 *out=&mock_extension;return 0;
}
#elif defined(TEST_POLICY_PROBE)
#include "../boot/policy_probe.c"
static U8 policy_fixture[2]={0,1};
static U8 policy_fallback_fixture[8]={0,0,0,0,5,0,0,0};
static U32 policy_calls;
static EFI_STATUS EFIAPI mock_policy(EFI_GUID *g,void *registration,void **out) {
    if(registration)return EFI_UNSUPPORTED;
    policy_calls++;
    if(equal(g,&policy_guids[0])) { *out=policy_fixture;return 0; }
    if(equal(g,&policy_guids[1])) { *out=policy_fallback_fixture;return 0; }
    *out=0;return 0x800000000000000eULL;
}
#else
#include "../boot/firmware_probe.c"
#endif
static EFI_BOOT_SERVICES mock_bs;
static EFI_SYSTEM_TABLE mock_st;
static Loaded mock_loaded;
static Fs mock_fs;
static File mock_file;
static Fv mock_fv;
static Fvb mock_fvb;
static Config mock_config;
static EFI_HANDLE mock_handles[1];
static U8 mock_hobs[88], mock_dp[10];
static char saved_report[32768];
static U32 writes, flushes, loads, starts, reads, failure, bad_path;
static EFI_STATUS EFIAPI mock_watch(UINTN time,U64 code,UINTN bytes,U16 *data) {
    if((time!=0 && time!=60)||code||bytes||data) failure=1;
    return 0;
}
static EFI_STATUS EFIAPI mock_close(File *f) {(void)f;return 0;}
static EFI_STATUS EFIAPI mock_flush(File *f) {(void)f;flushes++;return 0;}
static EFI_STATUS EFIAPI mock_position(File *f,U64 at) {(void)f;if(at)failure=2;return 0;}
static EFI_STATUS EFIAPI mock_write(File *f,UINTN *length,void *data) {
    (void)f; if(*length>=sizeof(saved_report)) {failure=3;return EFI_UNSUPPORTED;}
    for(UINTN i=0;i<*length;i++) saved_report[i]=((char *)data)[i];
    saved_report[*length]=0; writes++; return 0;
}
static EFI_STATUS EFIAPI mock_open(File *f,File **out,const U16 *name,U64 mode,U64 attr) {
    (void)f;if(name!=report_path || mode!=0x8000000000000003ULL || attr)failure=4;
    *out=&mock_file;return 0;
}
static EFI_STATUS EFIAPI mock_volume(Fs *fs,File **out) {(void)fs;*out=&mock_file;return 0;}
static EFI_STATUS EFIAPI mock_attr(Fv *f,U64 *out) {(void)f;*out=4;return 0;}
static EFI_STATUS EFIAPI mock_block_attr(Fvb *f,U32 *out) {(void)f;*out=4;return 0;}
static EFI_STATUS EFIAPI mock_physical(Fvb *f,U64 *out) {(void)f;*out=0xff110000;return 0;}
static EFI_STATUS EFIAPI mock_next(Fv *f,void *k,U8 *t,EFI_GUID *g,U32 *a,UINTN *s) {
    (void)f;(void)k;(void)t;(void)g;(void)a;(void)s;return 0x800000000000000eULL;
}
static EFI_STATUS EFIAPI mock_read(Fvb *f,U64 lba,UINTN offset,UINTN *size,U8 *header) {
    (void)f;if(lba || offset || *size!=128)failure=5;
    for(UINTN i=0;i<128;i++)header[i]=0;
    header[34]=2; header[40]='_';header[41]='F';header[42]='V';header[43]='H';
    header[52]=72;header[72]=0xe1;header[73]=0x0f;header[74]=0x57;header[75]=0x8b;
    reads++;return 0;
}
static EFI_STATUS EFIAPI mock_handle(EFI_HANDLE handle,EFI_GUID *g,void **out) {
    if(handle==(EFI_HANDLE)1 && equal(g,&loaded_guid))*out=&mock_loaded;
    else if(handle==(EFI_HANDLE)2 && equal(g,&fs_guid))*out=&mock_fs;
    else if(handle==(EFI_HANDLE)2 && equal(g,&path_guid)) {if(bad_path)mock_dp[2]=0;*out=mock_dp;}
    else if(handle==(EFI_HANDLE)3 && equal(g,&fv_guid))*out=&mock_fv;
    else if(handle==(EFI_HANDLE)3 && equal(g,&fvb_guid))*out=&mock_fvb;
#ifdef TEST_UPDATE_PROBE
    else if(handle==(EFI_HANDLE)5 && equal(g,&fmp_guid))*out=&mock_fmp;
#endif
    else {failure=6;return EFI_UNSUPPORTED;}
    return 0;
}
static EFI_STATUS EFIAPI mock_locate(U32 type,EFI_GUID *g,void *key,UINTN *count,EFI_HANDLE **handles) {
#if defined(TEST_HTTP_INSPECT_PROBE) || defined(TEST_HTTP_PREREQ_PROBE) || defined(TEST_HTTP_INIT_PROBE) || defined(TEST_HTTP_ETHERNET_PROBE) || defined(TEST_HTTP_CLEANUP_PROBE)
    if(!equal(g,&fv_guid))return 0x800000000000000eULL;
#endif
#ifdef TEST_UPDATE_PROBE
    if(type==2 && equal(g,&fmp_guid) && !key) {
        if(fmp_not_found)return 0x800000000000000eULL;
        *count=1;*handles=mock_fmp_handles;return 0;
    }
#endif
    if(type!=2 || !equal(g,&fv_guid) || key)failure=7;
    *count=1;*handles=mock_handles;return 0;
}
static EFI_STATUS EFIAPI mock_free(void *p) {
#ifdef TEST_UPDATE_PROBE
    if(p==mock_fmp_handles)return 0;
#endif
    if(p!=mock_handles)failure=8;return 0;
}
static EFI_STATUS EFIAPI mock_load(U8 policy,EFI_HANDLE parent,void *dp,void *buffer,UINTN size,EFI_HANDLE *out) {
    if(policy || parent!=(EFI_HANDLE)1 || buffer || size)failure=9;
    U8 *p=dp;if(p[0]!=1 || p[2]!=6 || p[6]!=4 || p[7]!=4)failure=10;
    UINTN length=u16(p+8);
    if(length!=4+sizeof(grub_path) || p[6+length]!=0x7f)failure=11;
    for(UINTN i=0;i<sizeof(grub_path);i++)if(p[10+i]!=((U8 *)grub_path)[i])failure=12;
    *out=(EFI_HANDLE)4; loads++;return 0;
}
static EFI_STATUS EFIAPI mock_start(EFI_HANDLE child,UINTN *size,U16 **data) {
    if(child!=(EFI_HANDLE)4 || size || data)failure=13;starts++;return 0;
}
int probe_run_tests(void) {
    mock_bs.handle_protocol=mock_handle;mock_bs.set_watchdog_timer=mock_watch;
    mock_bs.locate_handle_buffer=(void *)mock_locate;mock_bs.free_pool=(void *)mock_free;
    mock_bs.load_image=(void *)mock_load;mock_bs.start_image=(void *)mock_start;
    mock_st.boot_services=&mock_bs;mock_st.configuration_table=&mock_config;mock_st.table_entries=1;
    mock_config.guid=hob_guid;mock_config.table=mock_hobs;
    mock_loaded.device=(EFI_HANDLE)2;mock_fs.open_volume=mock_volume;
    mock_file.open=mock_open;mock_file.close=mock_close;mock_file.write=mock_write;
    mock_file.set_position=mock_position;mock_file.flush=mock_flush;
    mock_fv.attributes=mock_attr;mock_fv.next=mock_next;mock_fv.key_size=8;
    mock_fvb.attributes=mock_block_attr;mock_fvb.physical=mock_physical;mock_fvb.read=mock_read;
    mock_handles[0]=(EFI_HANDLE)3;
#if defined(TEST_HTTP_INSPECT_PROBE) || defined(TEST_HTTP_PREREQ_PROBE) || defined(TEST_HTTP_INIT_PROBE) || defined(TEST_HTTP_ETHERNET_PROBE) || defined(TEST_HTTP_CLEANUP_PROBE)
    mock_bs.locate_protocol=mock_inspect_database;
#endif
#ifdef TEST_POLICY_PROBE
    mock_bs.locate_protocol=mock_policy;
#endif
#ifdef TEST_EXTENSION_PROBE
    mock_bs.locate_protocol=mock_extension_locate;
    mock_extension.revision=1;mock_extension.get_info=mock_extension_info;
#endif
#if defined(TEST_HTTP_INSPECT_PROBE) || defined(TEST_HTTP_PREREQ_PROBE) || defined(TEST_HTTP_INIT_PROBE) || defined(TEST_HTTP_ETHERNET_PROBE) || defined(TEST_HTTP_CLEANUP_PROBE)
    #if defined(TEST_HTTP_INIT_PROBE) || defined(TEST_HTTP_ETHERNET_PROBE) || defined(TEST_HTTP_CLEANUP_PROBE)
    const U32 expected_writes=6;
#elif defined(TEST_HTTP_PREREQ_PROBE)
    const U32 expected_writes=5;
#else
    const U32 expected_writes=14;
#endif
#elif defined(TEST_UPDATE_PROBE)
    mock_fmp_handles[0]=(EFI_HANDLE)5;mock_fmp.get_info=mock_fmp_info;
    const U32 expected_writes=6;
#elif defined(TEST_EXTENSION_PROBE)
    const U32 expected_writes=6;
#elif defined(TEST_POLICY_PROBE)
    const U32 expected_writes=9;
#else
    const U32 expected_writes=4;
#endif
    mock_hobs[0]=1;mock_hobs[2]=56;
    U64 end=(U64)(mock_hobs+80);for(U32 i=0;i<8;i++)mock_hobs[48+i]=(U8)(end>>(i*8));
    mock_hobs[56]=5;mock_hobs[58]=24;
    U64 base=0xff110000;for(U32 i=0;i<8;i++)mock_hobs[64+i]=(U8)(base>>(i*8));
    mock_hobs[74]=2;mock_hobs[80]=0xff;mock_hobs[81]=0xff;mock_hobs[82]=8;
    mock_dp[0]=1;mock_dp[1]=1;mock_dp[2]=6;mock_dp[6]=0x7f;mock_dp[7]=0xff;mock_dp[8]=4;
    if(efi_main((EFI_HANDLE)1,&mock_st)!=0 || failure || writes!=expected_writes || flushes!=expected_writes || reads!=1 || loads!=1 || starts!=1)return 20+(int)failure;
    bad_path=1;
    if(efi_main((EFI_HANDLE)1,&mock_st)!=EFI_UNSUPPORTED || loads!=1 || starts!=1 || failure)return 40+(int)failure;
    mock_hobs[48]=0;mock_hobs[49]=0;mock_hobs[50]=0;mock_hobs[51]=0;
    used=0;hobs(&mock_st);
    if(used>=sizeof(output))return 60;
#ifdef TEST_POLICY_PROBE
    if(policy_calls!=10)return 64;
#endif
#ifdef TEST_EXTENSION_PROBE
    if(extension_calls!=2)return 65;
#endif
#ifdef TEST_UPDATE_PROBE
    if(fmp_calls!=2)return 61;
    fmp_bad_bounds=1;used=0;update_inventory(&mock_st,0);
    output[used]=0;
    /* A too-short descriptor must be rejected without interpreting its fields. */
    U8 found=0;
    for(UINTN i=0;i+14<=used;i++) {
        U8 match=1;const char *needle="FMP_BAD_BOUNDS";
        for(UINTN j=0;j<14;j++)if(output[i+j]!=needle[j])match=0;
        if(match)found=1;
    }
    if(!found || fmp_calls!=3)return 62;
    fmp_not_found=1;used=0;update_inventory(&mock_st,0);
    if(fmp_calls!=3)return 63;
#endif
    return 0;
}
const char *probe_mock_report(void) {return saved_report;}

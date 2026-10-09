#define TEST_HTTP_CLEANUP_PROBE
#include "http_prereq_host.c"
static OpenRecord record;
static UINTN record_count;
static EFI_STATUS record_status;
static U32 record_frees;
static EFI_STATUS EFIAPI info(EFI_HANDLE h,EFI_GUID *g,OpenRecord **out,UINTN *count) {
 if(h!=(EFI_HANDLE)1 || !equal(g,&dhcp_protocol))return EFI_UNSUPPORTED;
 *out=record_status || !record_count?0:&record;*count=record_count;return record_status;
}
static EFI_STATUS EFIAPI free_record(void *p) {
 if(p==&record){record_frees++;return 0;}return fixture_free(p);
}
int cleanup_run_tests(void) {
 int prereq=prereq_run_tests();if(prereq)return prereq;
 static EFI_BOOT_SERVICES bs;static EFI_SYSTEM_TABLE st;st.boot_services=&bs;
 bs.locate_handle_buffer=(void *)fixture_locate;bs.handle_protocol=fixture_handle;bs.free_pool=(void *)free_record;bs.open_protocol_information=(void *)info;
 absent_dhcp=invalid_policy=limit_handles=0;
 for(U32 i=0;i<41;i++)path_fixture[i]=0;
 path_fixture[0]=3;path_fixture[1]=11;path_fixture[2]=37;
 static const U8 mac[6]={0x7c,0xc2,0xc6,0x1d,0xb2,0xf5};
 for(U32 i=0;i<6;i++)path_fixture[4+i]=mac[i];
 path_fixture[37]=0x7f;path_fixture[38]=0xff;path_fixture[39]=4;
 if(cleanup_target(&st)!=(EFI_HANDLE)1)return 14;
 path_fixture[4]^=1;if(cleanup_target(&st))return 15;path_fixture[4]^=1;
 limit_handles=1;if(cleanup_target(&st))return 16;limit_handles=0;
 record_count=1;record_status=0;record.agent=(EFI_HANDLE)2;record.controller=(EFI_HANDLE)1;record.attributes=16;record.open_count=1;
 used=0;open_records(&st,(EFI_HANDLE)1,&dhcp_protocol,(EFI_HANDLE)1,"TEST");
 if(!contains("target_controller=0x0000000000000001") || !contains("attributes=0x0000000000000010") || record_frees!=1)return 17;
 record_count=129;used=0;open_records(&st,(EFI_HANDLE)1,&dhcp_protocol,(EFI_HANDLE)1,"TEST");
 if(!contains("OPEN_RECORD_BOUNDS_REJECTED") || contains("OPEN_RECORD index=") || record_frees!=2)return 18;
 record_status=0x800000000000000eULL;used=0;open_records(&st,(EFI_HANDLE)1,&dhcp_protocol,(EFI_HANDLE)1,"TEST");
 if(contains("OPEN_RECORD index=") || record_frees!=2)return 19;
 record_status=0;record_count=0;used=0;open_records(&st,(EFI_HANDLE)1,&dhcp_protocol,(EFI_HANDLE)1,"TEST");
 if(contains("OPEN_RECORD_BOUNDS_REJECTED") || contains("OPEN_RECORD index=") || record_frees!=2)return 20;
 return 0;
}

#ifndef COMPANION_EXTENSION_H
#define COMPANION_EXTENSION_H
#include "uefi.h"
/* Owner service GUID. Boot-services lifetime only; no SMM/runtime authority. */
#define COMPANION_EXTENSION_GUID {0x377fa1a2,0xaacc,0x4689,{0xb1,0x5b,0x8b,0xa1,0xe8,0x99,0x58,0x18}}
#define COMPANION_EXTENSION_MAGIC 0x314458454d504f43ULL
#define COMPANION_BUFFER_TOO_SMALL 0x8000000000000005ULL
#define COMPANION_INVALID_PARAMETER 0x8000000000000002ULL
typedef struct {
 U64 magic;
 U32 revision, firmware_revision, boot_services_revision, reserved;
 U64 capabilities;
} CompanionExtensionInfo;
typedef struct CompanionExtension CompanionExtension;
struct CompanionExtension {
 U64 revision;
 EFI_STATUS (EFIAPI *get_info)(CompanionExtension *,UINTN *,CompanionExtensionInfo *);
};
_Static_assert(sizeof(CompanionExtensionInfo)==32,"extension information ABI");
#endif

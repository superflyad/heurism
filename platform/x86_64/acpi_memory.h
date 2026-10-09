#ifndef COMPANION_ACPI_MEMORY_H
#define COMPANION_ACPI_MEMORY_H
#include "../../kernel/boot_info.h"
/* Only ACPI reclaim/NVS ranges in the final EFI map, below our identity limit. */
const U8 *acpi_memory_read(void *context,U64 address,U64 size);
#endif

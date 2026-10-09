#ifndef COMPANION_ACPI_H
#define COMPANION_ACPI_H
#include "../../common/types.h"
#define ACPI_MAX_TABLES 64
#define ACPI_MAX_ECAM 16
/* Reader must prove the entire physical range readable before returning it. */
typedef const U8 *(*AcpiRead)(void *, U64, U64);
typedef struct { U64 address; U32 length; char signature[5]; } AcpiTable;
typedef struct { U64 base; U16 segment; U8 first_bus, last_bus; } AcpiEcam;
typedef struct {
    U64 xsdt, local_apic;
    U32 table_count, processor_count, io_apic_count, ecam_count;
    AcpiTable tables[ACPI_MAX_TABLES];
    AcpiEcam ecam[ACPI_MAX_ECAM];
} AcpiInventory;
/* 1: validated inventory, 0: absent RSDP, -1: invalid/unsupported input.
 * Output is empty on failure. No AML execution or device register writes. */
int acpi_inventory(U64 rsdp, AcpiRead read, void *context, AcpiInventory *out);
#endif

ENTRY(kernel_entry)
PHDRS { text PT_LOAD FLAGS(5); rodata PT_LOAD FLAGS(4); data PT_LOAD FLAGS(6); }
SECTIONS {
    . = 0x2000000;
    __text_start = .;
    .text : ALIGN(4096) { KEEP(*(.text.entry)) *(.text .text.*) } :text
    . = ALIGN(4096); __text_end = .; __rodata_start = .;
    .rodata : ALIGN(4096) { *(.rodata .rodata.*) } :rodata
    . = ALIGN(4096); __rodata_end = .;
    .data : ALIGN(4096) { *(.data .data.*) } :data
    .bss : ALIGN(4096) { *(.bss .bss.*) *(COMMON) } :data
    . = ALIGN(4096); __kernel_end = .;
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}

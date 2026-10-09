.section .text.entry,"ax",@progbits
.global kernel_entry
.type kernel_entry,@function
kernel_entry:
    cli
    cld
    xor %rbp,%rbp
    and $-16,%rsp
    call kernel_main
1:  cli
    hlt
    jmp 1b
.size kernel_entry,.-kernel_entry
.section .note.GNU-stack,"",@progbits

.text
.global kernel_jump
# Microsoft x64 args: entry RCX, BootInfo RDX, stack top R8.
# Enter ELF System V ABI with BootInfo RDI and a fresh aligned stack.
kernel_jump:
    cli
    cld
    mov %rcx,%rax
    mov %rdx,%rdi
    mov %r8,%rsp
    and $-16,%rsp
    sub $8,%rsp
    movq $0,(%rsp)
    jmp *%rax

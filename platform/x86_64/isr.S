.text
.global x86_load_gdt
x86_load_gdt:
    lgdt (%rdi)
    pushq $8
    leaq 1f(%rip),%rax
    pushq %rax
    lretq
1:  mov $16,%ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%ss
    xor %eax,%eax
    mov %ax,%fs
    mov %ax,%gs
    mov $24,%ax
    ltr %ax
    ret
isr_0:
    pushq $0
    pushq $0
    jmp isr_common
isr_1:
    pushq $0
    pushq $1
    jmp isr_common
isr_2:
    pushq $0
    pushq $2
    jmp isr_common
isr_3:
    pushq $0
    pushq $3
    jmp isr_common
isr_4:
    pushq $0
    pushq $4
    jmp isr_common
isr_5:
    pushq $0
    pushq $5
    jmp isr_common
isr_6:
    pushq $0
    pushq $6
    jmp isr_common
isr_7:
    pushq $0
    pushq $7
    jmp isr_common
isr_8:
    pushq $8
    jmp isr_common
isr_9:
    pushq $0
    pushq $9
    jmp isr_common
isr_10:
    pushq $10
    jmp isr_common
isr_11:
    pushq $11
    jmp isr_common
isr_12:
    pushq $12
    jmp isr_common
isr_13:
    pushq $13
    jmp isr_common
isr_14:
    pushq $14
    jmp isr_common
isr_15:
    pushq $0
    pushq $15
    jmp isr_common
isr_16:
    pushq $0
    pushq $16
    jmp isr_common
isr_17:
    pushq $17
    jmp isr_common
isr_18:
    pushq $0
    pushq $18
    jmp isr_common
isr_19:
    pushq $0
    pushq $19
    jmp isr_common
isr_20:
    pushq $0
    pushq $20
    jmp isr_common
isr_21:
    pushq $21
    jmp isr_common
isr_22:
    pushq $0
    pushq $22
    jmp isr_common
isr_23:
    pushq $0
    pushq $23
    jmp isr_common
isr_24:
    pushq $0
    pushq $24
    jmp isr_common
isr_25:
    pushq $0
    pushq $25
    jmp isr_common
isr_26:
    pushq $0
    pushq $26
    jmp isr_common
isr_27:
    pushq $0
    pushq $27
    jmp isr_common
isr_28:
    pushq $0
    pushq $28
    jmp isr_common
isr_29:
    pushq $29
    jmp isr_common
isr_30:
    pushq $30
    jmp isr_common
isr_31:
    pushq $0
    pushq $31
    jmp isr_common
isr_32:
    pushq $0
    pushq $32
    jmp isr_common
isr_254:
    pushq $0
    pushq $254
    jmp isr_common
isr_255:
    pushq $0
    pushq $255
    jmp isr_common
isr_common:
    cld
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rbp
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15
    mov %rsp,%rdi
    mov %rsp,%rbx
    and $-16,%rsp
    call x86_interrupt
    mov %rbx,%rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    add $16,%rsp
    iretq
.section .rodata
.global isr_stubs
isr_stubs:
    .quad isr_0
    .quad isr_1
    .quad isr_2
    .quad isr_3
    .quad isr_4
    .quad isr_5
    .quad isr_6
    .quad isr_7
    .quad isr_8
    .quad isr_9
    .quad isr_10
    .quad isr_11
    .quad isr_12
    .quad isr_13
    .quad isr_14
    .quad isr_15
    .quad isr_16
    .quad isr_17
    .quad isr_18
    .quad isr_19
    .quad isr_20
    .quad isr_21
    .quad isr_22
    .quad isr_23
    .quad isr_24
    .quad isr_25
    .quad isr_26
    .quad isr_27
    .quad isr_28
    .quad isr_29
    .quad isr_30
    .quad isr_31
    .quad isr_32
    .quad isr_254
    .quad isr_255
.section .note.GNU-stack,"",@progbits


.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
    .long MAGIC
    .long FLAGS
    .long CHECKSUM

.section .text
.global start
.extern kernel_main
start:
    cli
    mov $stack_top, %esp
    push %ebx        # multiboot info pointer
    push %eax        # magic value
    call kernel_main

halt_loop:
    cli
    hlt
    jmp halt_loop

.section .bss
.align 16
stack_bottom:
    .space 16384
stack_top:

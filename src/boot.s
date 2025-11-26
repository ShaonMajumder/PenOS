.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set VIDEO,    1<<2
.set FLAGS,    ALIGN | MEMINFO | VIDEO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot, "a"
.align 4
    .long MAGIC
    .long FLAGS
    .long CHECKSUM
    .long 1          # request linear framebuffer
    .long 1024       # width
    .long 768        # height
    .long 32         # bpp

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

.global isr_stub_table
.extern isr_dispatch

#define ISR_NOERR(num) \
.global isr##num; \
isr##num: \
    pushl $0; \
    pushl $num; \
    jmp isr_common_stub

#define ISR_ERR(num) \
.global isr##num; \
isr##num: \
    pushl $num; \
    jmp isr_common_stub

isr_stub_table:
#define X(num, handler) .long handler
X(0, isr0)
X(1, isr1)
X(2, isr2)
X(3, isr3)
X(4, isr4)
X(5, isr5)
X(6, isr6)
X(7, isr7)
X(8, isr8)
X(9, isr9)
X(10, isr10)
X(11, isr11)
X(12, isr12)
X(13, isr13)
X(14, isr14)
X(15, isr15)
X(16, isr16)
X(17, isr17)
X(18, isr18)
X(19, isr19)
X(32, isr32)
X(33, isr33)
X(34, isr34)
X(35, isr35)
X(36, isr36)
X(37, isr37)
X(38, isr38)
X(39, isr39)
X(40, isr40)
X(41, isr41)
X(42, isr42)
X(43, isr43)
X(44, isr44)
X(45, isr45)
X(46, isr46)
X(47, isr47)
X(128, isr128)
#undef X

ISR_NOERR(0)
ISR_NOERR(1)
ISR_NOERR(2)
ISR_NOERR(3)
ISR_NOERR(4)
ISR_NOERR(5)
ISR_NOERR(6)
ISR_NOERR(7)
ISR_ERR(8)
ISR_NOERR(9)
ISR_ERR(10)
ISR_ERR(11)
ISR_ERR(12)
ISR_ERR(13)
ISR_ERR(14)
ISR_NOERR(15)
ISR_NOERR(16)
ISR_ERR(17)
ISR_NOERR(18)
ISR_NOERR(19)
ISR_NOERR(32)
ISR_NOERR(33)
ISR_NOERR(34)
ISR_NOERR(35)
ISR_NOERR(36)
ISR_NOERR(37)
ISR_NOERR(38)
ISR_NOERR(39)
ISR_NOERR(40)
ISR_NOERR(41)
ISR_NOERR(42)
ISR_NOERR(43)
ISR_NOERR(44)
ISR_NOERR(45)
ISR_NOERR(46)
ISR_NOERR(47)
ISR_NOERR(128)

isr_common_stub:
    pusha
    push %ds
    push %es
    push %fs
    push %gs

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    push %esp
    call isr_dispatch
    add $4, %esp

    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa

    add $8, %esp
    iretd

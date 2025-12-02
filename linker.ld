ENTRY(start)
SECTIONS
{
    . = 1M;
    .multiboot ALIGN(4K) :
    {
        KEEP(*(.multiboot))
    }
    .text ALIGN(4K) :
    {
        _text_start = .;
        *(.text*)
        _text_end = .;
    }
    .rodata ALIGN(4K) :
    {
        _rodata_start = .;
        *(.rodata*)
        _rodata_end = .;
    }
    .data ALIGN(4K) :
    {
        _data_start = .;
        *(.data*)
        _data_end = .;
    }
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss*)
    }
    end = .;
}

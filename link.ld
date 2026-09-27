ENTRY(start)
SECTIONS
{
    kstart = .;
    .mbheader 0x100000 :
    {
        *(.mbheader)
    }
    .text :
    {
        code = .; _code = .; __code = .;
        *(.text)
        *(.gnu.linkonce.t*)
        . = ALIGN(4096);
    }

    .data :
    {
        data = .; _data = .; __data = .;
        *(.data)
        *(.rodata)
        *(.gnu.linkonce.t*)
        . = ALIGN(4096);
    }

    .bss :
    {
        bss = .; _bss = .; __bss = .;
        *(.bss)
        . = ALIGN(4096);
    }

    end = .; _end = .; __end = .;
}
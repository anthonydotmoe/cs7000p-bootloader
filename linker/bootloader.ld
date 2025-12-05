ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
    AXIRAM(wx) : ORIGIN = 0x24000000, LENGTH = 512K
}

_estack = ORIGIN(AXIRAM) + LENGTH(AXIRAM); /* end of RAM */

SECTIONS
{
    . = 0;

    /* .text section: code goes to flash */
    .text :
    {
        KEEP(*(.isr_vector))

        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
        *(.glue_7)
        *(.glue_7t)

        . = ALIGN(4);
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
    } > FLASH

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > FLASH
    __exidx_end = .;

    .data : ALIGN(8)
    {
        _data = .;
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
        . = ALIGN(8);
        _edata = .;
    } > AXIRAM AT > FLASH
    _etext = LOADADDR(.data);

    _bss_start = .;
    .bss :
    {
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b.*)
        . = ALIGN(8);
    } > AXIRAM
    _bss_end = .;

    _end = .;
    PROVIDE(end = .);
}

OUTPUT_FORMAT("elf32-sparc")
OUTPUT_ARCH(sparc)
SECTIONS
{
    . = 0x1e000000;
    .arg.data : {
        KEEP(*(.arg.data))
        *(.arg.data)
        . = ALIGN(16);
        *(.data*)
    }
    . = 0x1d000000;
    .text : {
        *(.text.controlled_act)
        . = ALIGN(0x400);
        *(.text.multi_b)
        . = ALIGN(0x400);
        *(.text.multi_c)
        . = ALIGN(0x400);
        *(.text.multi_d)
        . = ALIGN(0x400);
        *(.text*)
        *(.rodata*)
        KEEP(*(.uuid.rodata*))
    }
}

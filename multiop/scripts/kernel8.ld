OUTPUT_FORMAT("elf32-sparc")
OUTPUT_ARCH(sparc)
SECTIONS
{
    . = 0x1e000000;
    .arg.data : { KEEP(*(.arg.data)) *(.arg.data) . = ALIGN(16); *(.data*) }
    . = 0x1d000000;
    .text : {
        *(.text.entry0)
        . = ALIGN(0x400);
        *(.text.entry1)
        . = ALIGN(0x400);
        *(.text.entry2)
        . = ALIGN(0x400);
        *(.text.entry3)
        . = ALIGN(0x400);
        *(.text.entry4)
        . = ALIGN(0x400);
        *(.text.entry5)
        . = ALIGN(0x400);
        *(.text.entry6)
        . = ALIGN(0x400);
        *(.text.entry7)
        . = ALIGN(0x400);
        *(.text*)
        *(.rodata*)
        KEEP(*(.uuid.rodata*))
    }
}

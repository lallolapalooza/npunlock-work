/* kernel-lib: linker script for the multi-entry ACT image (src/multi4.c).
 *
 * ACT ranges address code on 0x400-byte boundaries (measured rule: the range
 * code address is rounded down to 0x400), so the four entries are placed
 * 0x400 apart.  Each of the first three entries must stay below 0x400 bytes;
 * the last may grow freely.  Sizes are checked by tools/make_manifest.py
 * (`entry_offsets` / `text_bytes`).
 *
 * The pattern follows multiop/scripts/kernel4big.ld (the workstream that
 * measured the rule); this file is kernel-lib's own copy so the library builds
 * without depending on that tree.
 */
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
        KEEP(*(.text.controlled_act))
        . = ALIGN(0x400);
        KEEP(*(.text.kb_add_packed))
        . = ALIGN(0x400);
        KEEP(*(.text.kb_mul_packed))
        . = ALIGN(0x400);
        KEEP(*(.text.kb_silu))
        . = ALIGN(0x400);
        *(.text*)
        *(.rodata*)
        KEEP(*(.uuid.rodata*))
    }
}

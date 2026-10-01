; load-visibility gate kernel (RoughFinch oracle, K_MIN=6)
; assemble: shave_asm obj load_vis.s -o load_vis.elf   (or asm --hex)
; variant with automatic fill: shave_asm asm load_vis.s --hex --latency 6
        LSU0.LDO.32 i0, i18, 0x28 ; fmt=0
        BRU.NOP
        BRU.NOP
        BRU.NOP
        BRU.NOP
        BRU.NOP
        BRU.NOP
        LSU1.ST.32 i0, i0

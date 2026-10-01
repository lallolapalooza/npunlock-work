	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDO.32 i0, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LD.32.F16.F32 i1, i0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i1, i1, i1
	LSU0.LDO.32 i2, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.ST.32 i1, i2
	LSU0.LDIL i1, 0x04 ; fmt=0
	IAU.ADD i1, i2, i1
	LSU0.LDIL i2, 0x02 ; fmt=0
	IAU.ADD i0, i0, i2
	LSU0.LD.32.F16.F32 i0, i0
	BRU.NOP
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU1.LDIH i2, 0x3F00
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i0, i0, i2
	CMU.CP.32 i0, i0
	CMU.CP.32 i0, i0
	LSU0.ST.32 i0, i1
	BRU.JMP i30
Lfunc_end0:
	.size	controlled_act, Lfunc_end0-controlled_act
                                        // -- End function

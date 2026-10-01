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
	LSU0.LDO.32 i1, i0, 0x8
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x3F ; fmt=0
	IAU.AND i2, i1, i2
	LSU0.LDIL i3, 0x1F ; fmt=0
	IAU.AND i1, i1, i3
	LSU0.LDO.32 i4, i0, 0x4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHR.U32 i4, i4, i1
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i3, i3, i5
	SAU.DIV.U32 i3, i2, i3
	CMU.CMZ.I32 i3
	LSU0.LDIL i5, 0x00 ; fmt=0
	IAU.SUB i3, i5, i3
	IAU.AND i3, i4, i3
	LSU0.LDO.32 i0, i0, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHR.U32 i0, i0, i1
	LSU0.LDIL i1, 0x20 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.ADD i2, i2, i4
	SAU.DIV.U32 i1, i1, i2
	CMU.CMZ.I32 i1
	IAU.SUB i1, i5, i1
	IAU.AND i0, i0, i1
	IAU.OR i0, i0, i3
	LSU0.LDO.32 i1, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.ST.32 i0, i1
	BRU.JMP i30
Lfunc_end0:
	.size	controlled_act, Lfunc_end0-controlled_act
                                        // -- End function

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
	LSU0.LDO.32 i1, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x08 ; fmt=0
	IAU.ADD i2, i1, i2
	LSU0.LDO.32 i3, i0, 0x4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i4, i0, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i0, i0, 0x8
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i5, i0, i0 ; fmt=2
	BRU.NOP
	BRU.NOP
	CMU.CP.32.U32.F32 i5, i5 ; fmt=7
	LSU1.ST.32 i5, i2
	LSU0.LDIL i2, 0x1F ; fmt=0
	IAU.AND i2, i4, i2
	LSU0.LDIL i5, 0x07 ; fmt=0
	IAU.AND i4, i4, i5
	IAU.ADD i2, i4, i2
	LSU0.LDIL i4, 0x7FF ; fmt=0
	IAU.AND i4, i3, i4
	IAU.ADD i2, i2, i4
	LSU0.LDIL i4, 0x1FFF ; fmt=0
	IAU.AND i3, i3, i4
	IAU.ADD i2, i2, i3
	LSU0.LDIL i3, 0xFF ; fmt=0
	IAU.AND i4, i0, i3
	IAU.SHR.U32 i5, i0, 24 ; fmt=3
	IAU.ADD i4, i5, i4
	LSU1.ST.32 i2, i1
	IAU.SHR.U32 i2, i0, 8 ; fmt=3
	IAU.AND i2, i2, i3
	IAU.ADD i2, i4, i2
	IAU.SHR.U32 i0, i0, 16 ; fmt=3
	IAU.AND i0, i0, i3
	IAU.ADD i0, i2, i0
	LSU0.LDIL i2, 0x04 ; fmt=0
	IAU.ADD i1, i1, i2
	LSU1.ST.32 i0, i1
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end0:
	.size	controlled_act, Lfunc_end0-controlled_act
                                        // -- End function

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
	LSU0.LDO.32 i0, i0, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0x03 ; fmt=0
	IAU.AND i1, i0, i3
	LSU0.LDIL i0, 0x01 ; fmt=0
	LSU0.LDIL i2, 0x01 ; fmt=0
	IAU.XOR i4, i1, i0
	IAU.ADD i4, i4, i2
	SAU.DIV.U32 i4, i2, i4
	CMU.CMZ.I32 i4
	LSU0.LDIL i2, 0x02 ; fmt=0
	BRU.BRA i4, LBB0_1
	.byte 0x1b, 0x40
	BRU.BRA LBB0_2
LBB0_1:
	BRU.BRA LBB0_3
LBB0_2:
	IAU.AND i2, i3, i3
LBB0_3:
	LSU0.LDIL i3, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i1, i1, i3
	IAU.ADD i1, i1, i4
	SAU.DIV.U32 i1, i4, i1
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB0_4
	.byte 0x1b, 0x40
	BRU.BRA LBB0_5
LBB0_4:
	BRU.BRA LBB0_6
LBB0_5:
	IAU.AND i0, i2, i2
LBB0_6:
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

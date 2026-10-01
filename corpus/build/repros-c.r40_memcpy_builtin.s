	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDIL i1, 0x00 ; fmt=0
	LSU0.LDO.32 i0, i18, 0x28
	LSU0.LDO.32 i2, i18, 0x0
	IAU.AND i3, i1, i1
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_2:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.ADD i4, i0, i3
	LSU0.LD.8 i5, i2
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i5, i5, 24
	IAU.SHR.U32 i5, i5, 24
	IAU.ADD i5, i1, i5
	LSU0.ST.8 i5, i4
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.ADD i1, i1, i4
	IAU.ADD i3, i3, i4
	LSU0.LDIL i4, 0x28 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i3, i4
	IAU.ADD i4, i4, i5
	SAU.DIV.U32 i4, i5, i4
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB0_1
	.byte 0x2b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_2
LBB0_1:                                 // %for.cond.cleanup
	LSU0.LDIL i1, 0x28 ; fmt=0
	IAU.ADD i1, i0, i1
	LSU0.LD.8 i0, i0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i0, i0, 24
	IAU.SHR.U32 i0, i0, 24
	LSU0.ST.32 i0, i1
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

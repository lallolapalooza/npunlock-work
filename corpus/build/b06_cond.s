	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDIL i2, 0x01 ; fmt=0
	LSU0.LDO.32 i0, i18, 0x28
	LSU0.LDO.32 i1, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i1, i1, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0x0A ; fmt=0
	CMU.CMII.U32 i3, i1
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i3, i3, i4
	BRU.BRA i3, LBB0_6
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_1:                                 // %if.else
	LSU0.LDIL i2, 0x14 ; fmt=0
	CMU.CMII.U32 i2, i1
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i3, i2, i3
	LSU0.LDIL i2, 0x02 ; fmt=0
	BRU.BRA i3, LBB0_6
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_2:                                 // %if.else4
	LSU0.LDIL i2, 0x1E ; fmt=0
	CMU.CMII.U32 i2, i1
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i3, i2, i3
	LSU0.LDIL i2, 0x03 ; fmt=0
	BRU.BRA i3, LBB0_6
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_3:                                 // %if.else7
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.AND i3, i1, i3
	LSU0.LDIL i4, 0x00 ; fmt=0
	IAU.XOR i3, i3, i4
	CMU.CMZ.I32 i3
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i3, i3, i4
	LSU0.LDIL i4, 0x04 ; fmt=0
	LSU0.LDIL i4, 0x06 ; fmt=0
	BRU.BRA i3, LBB0_4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_5
LBB0_4:
	BRU.BRA LBB0_6
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_5:
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_6:                                 // %if.end19
	LSU0.LDIL i3, 0x07 ; fmt=0
	IAU.AND i5, i1, i1
	CMU.CMII.U32 i5, i3
	LSU0.LDIL i4, 0xFFFF ; fmt=0
	LSU1.LDIH i4, 0xFFFF
	LSU0.LDIL i3, 0x01 ; fmt=0
	BRU.BRA i5, LBB0_7
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_8
LBB0_7:
	BRU.BRA LBB0_9
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_8:
	IAU.AND i3, i4, i4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_9:
	IAU.ADD i2, i2, i3
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU1.LDIH i4, 0x01
	CMU.CMII.U32 i4, i1
	LSU0.LDIL i3, 0x100 ; fmt=0
	BRU.BRA i4, LBB0_10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_11
LBB0_10:
	IAU.ADD i2, i2, i3
	BRU.BRA LBB0_12
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_11:
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_12:
	LSU0.LDIL i3, 0x05 ; fmt=0
	IAU.XOR i4, i1, i3
	IAU.AND i5, i4, i4
	CMU.CMZ.I32 i5
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i5, i6
	IAU.SHL i5, i5, 1
	IAU.AND i6, i1, i1
	CMU.CMII.U32 i6, i3
	IAU.OR i3, i5, i6
	CMU.CMZ.I32 i4
	IAU.SHL i4, i4, 2
	IAU.OR i3, i3, i4
	LSU0.LDIL i4, 0x06 ; fmt=0
	CMU.CMII.U32 i4, i1
	IAU.SHL i4, i4, 3
	IAU.OR i3, i3, i4
	LSU0.LDIL i4, 0x04 ; fmt=0
	IAU.ADD i4, i0, i4
	LSU0.ST.32 i3, i4
	LSU0.LDIL i3, 0x200 ; fmt=0
	LSU0.LDIL i4, 0xF0F ; fmt=0
	LSU1.LDIH i4, 0xF0F
	IAU.AND i5, i1, i4
	LSU0.LDIL i4, 0x00 ; fmt=0
	IAU.XOR i5, i5, i4
	CMU.CMZ.I32 i5
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i5, i6
	BRU.BRA i5, LBB0_13
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_14
LBB0_13:
	IAU.ADD i3, i2, i3
	BRU.BRA LBB0_15
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_14:
	IAU.AND i3, i2, i2
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_15:
	IAU.XOR i1, i1, i4
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB0_16
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_17
LBB0_16:
	BRU.BRA LBB0_18
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_17:
	IAU.AND i3, i2, i2
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_18:
	LSU0.ST.32 i3, i0
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

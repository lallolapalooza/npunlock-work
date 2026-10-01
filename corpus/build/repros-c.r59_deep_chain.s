	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDIL i1, 0x01 ; fmt=0
	LSU0.LDO.32 i0, i18, 0x0
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
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i2, 0x01 ; fmt=0
	IAU.XOR i0, i4, i0
	IAU.ADD i0, i0, i2
	SAU.DIV.U32 i6, i2, i0
	CMU.CMZ.I32 i6
	LSU0.LDIL i0, 0x2B ; fmt=0
	IAU.ADD i0, i18, i0
	LSU0.LD.8 i0, i0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i0, i0, 24
	IAU.SHR.U32 i0, i0, 24
	LSU0.LDIL i2, 0x2A ; fmt=0
	IAU.ADD i2, i18, i2
	LSU0.LD.8 i2, i2
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i2, i2, 24
	IAU.SHR.U32 i2, i2, 24
	LSU0.LDIL i3, 0x29 ; fmt=0
	IAU.ADD i3, i18, i3
	LSU0.LD.8 i3, i3
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i3, i3, 24
	IAU.SHR.U32 i3, i3, 24
	LSU0.LDIL i5, 0x28 ; fmt=0
	IAU.ADD i5, i18, i5
	LSU0.LD.8 i5, i5
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i5, i5, 24
	IAU.SHR.U32 i5, i5, 24
	BRU.BRA i6, LBB0_14
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_1:                                 // %entry
	LSU0.LDIL i1, 0x01 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i1, i4, i1
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB0_3
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_2:                                 // %if.then3
	LSU0.LDIL i1, 0x02 ; fmt=0
	BRU.BRA LBB0_14
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_3:                                 // %if.else4
	LSU0.LDIL i1, 0x04 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i6, i4, i6
	SAU.DIV.U32 i6, i1, i6
	CMU.CMZ.I32 i6
	BRU.BRA i6, LBB0_14
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_4:                                 // %if.else7
	LSU0.LDIL i1, 0x08 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i6, i4, i6
	SAU.DIV.U32 i6, i1, i6
	CMU.CMZ.I32 i6
	BRU.BRA i6, LBB0_14
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_5:                                 // %if.else10
	LSU0.LDIL i1, 0x10 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i6, i4, i6
	SAU.DIV.U32 i6, i1, i6
	CMU.CMZ.I32 i6
	BRU.BRA i6, LBB0_14
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_6:                                 // %if.else13
	LSU0.LDIL i1, 0x20 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i6, i4, i6
	SAU.DIV.U32 i6, i1, i6
	CMU.CMZ.I32 i6
	BRU.BRA i6, LBB0_14
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_7:                                 // %if.else16
	LSU0.LDIL i1, 0x40 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i6, i4, i6
	SAU.DIV.U32 i6, i1, i6
	CMU.CMZ.I32 i6
	BRU.BRA i6, LBB0_14
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_8:                                 // %if.else19
	LSU0.LDIL i1, 0x80 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i6, i4, i6
	SAU.DIV.U32 i6, i1, i6
	CMU.CMZ.I32 i6
	BRU.BRA i6, LBB0_14
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_9:                                 // %if.else22
	LSU0.LDIL i1, 0x100 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i6, i4, i6
	SAU.DIV.U32 i6, i1, i6
	CMU.CMZ.I32 i6
	BRU.BRA i6, LBB0_14
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_10:                                // %if.else25
	LSU0.LDIL i1, 0x200 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i6, i4, i6
	SAU.DIV.U32 i6, i1, i6
	CMU.CMZ.I32 i6
	BRU.BRA i6, LBB0_14
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_11:                                // %if.else28
	LSU0.LDIL i6, 0x400 ; fmt=0
	LSU0.LDIL i7, 0x01 ; fmt=0
	IAU.ADD i4, i4, i7
	SAU.DIV.U32 i4, i6, i4
	CMU.CMZ.I32 i4
	LSU0.LDIL i6, 0x800 ; fmt=0
	BRU.BRA i4, LBB0_12
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_13
LBB0_12:
	BRU.BRA LBB0_14
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_13:
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_14:                                // %if.end41
	IAU.SHL i3, i3, 8
	IAU.OR i3, i3, i5
	IAU.SHL i2, i2, 16
	IAU.OR i2, i3, i2
	IAU.SHL i0, i0, 24
	IAU.OR i0, i2, i0
	LSU0.ST.32 i1, i0
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

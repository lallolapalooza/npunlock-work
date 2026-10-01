	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i1, 0x01 ; fmt=0
	IAU.XOR i0, i18, i0
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB0_9
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_1:                                 // %if.end.i
	LSU0.LDO.32 i1, i18, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i0, 0xFFF0 ; fmt=0
	LSU1.LDIH i0, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i1, i0
	LSU0.LDIL i2, 0xFFF1 ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0x01 ; fmt=0
	SAU.DIV.U32 i0, i0, i2
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB0_9
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_2:                                 // %if.end.i
	LSU0.LDO.32 i0, i18, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i2, i0, i2
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB0_9
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_3:                                 // %for.body.i.preheader
	LSU0.LDIL i2, 0x03 ; fmt=0
	IAU.ADD i4, i0, i2
	LSU0.LDIL i0, 0x01 ; fmt=0
LBB0_4:                                 // %for.body.i
                                        // =>This Inner Loop Header: Depth=1
	IAU.AND i2, i0, i0
	LSU0.LDIL i0, 0xFFFD ; fmt=0
	LSU1.LDIH i0, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i4, i0
	LSU0.LDO.32 i3, i0, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i0, i3, i0
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB0_9
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_5:                                 // %for.inc.i
                                        //   in Loop: Header=BB0_4 Depth=1
	LSU0.LDIL i0, 0x04 ; fmt=0
	IAU.ADD i4, i4, i0
	IAU.MUL i0, i3, i2
	LSU0.LDIL i5, 0xFFFF ; fmt=0
	LSU1.LDIH i5, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i1, i5
	LSU0.LDIL i5, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i1, i5
	IAU.ADD i5, i5, i6
	SAU.DIV.U32 i5, i6, i5
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i5
	BRU.BRA i5, LBB0_4
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_6:                                 // %do.end
	LSU0.LDIL i1, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i1, i0, i1
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB0_9
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_7:                                 // %for.body.preheader
	LSU0.LDO.32 i1, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i4, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.MUL i2, i2, i3
	LSU0.LDIL i3, 0x00 ; fmt=0
	IAU.AND i5, i4, i4
LBB0_8:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.XOR i6, i3, i0
	IAU.SHL i6, i6, 1
	IAU.ADD i6, i4, i6
	LSU0.LD.32.F16.F32 i6, i6
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LD.32.F16.F32 i7, i5
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.MUL.F32 i6, i7, i6 ; fmt=2
	BRU.NOP
	BRU.NOP
	LSU1.ST.16.F32.F16 i6, i1
	LSU0.LDIL i6, 0x02 ; fmt=0
	IAU.ADD i5, i5, i6
	IAU.ADD i1, i1, i6
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i3, i3, i6
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i7, i2, i3
	IAU.ADD i7, i7, i6
	SAU.DIV.U32 i6, i6, i7
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i6
	BRU.BRA i6, LBB0_8
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_9:                                 // %cleanup
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

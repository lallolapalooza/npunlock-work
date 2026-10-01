	.text
	.file	"multi8.c"
	.section	.text.e0,"ax",@progbits
	.globl	e0                              // -- Begin function e0
	.type	e0,@function
e0:                                     // @e0
.Le0$local:
// %bb.0:                               // %entry
	LSU0.LDIL i1, 0xFFF0 ; fmt=0
	LSU1.LDIH i1, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i0, i18, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i0, i1
	LSU0.LDIL i2, 0xFFF1 ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0x01 ; fmt=0
	SAU.DIV.U32 i1, i1, i2
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
LBB0_1:                                 // %entry
	LSU0.LDO.32 i1, i18, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
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
LBB0_2:                                 // %for.body.preheader
	LSU0.LDIL i2, 0x03 ; fmt=0
	IAU.ADD i3, i1, i2
	LSU0.LDIL i4, 0x01 ; fmt=0
LBB0_3:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.AND i1, i4, i4
	LSU0.LDIL i2, 0xFFFD ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i3, i2
	LSU0.LDO.32 i2, i2, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i2, i4
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB0_9
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
LBB0_4:                                 // %cleanup
                                        //   in Loop: Header=BB0_3 Depth=1
	LSU0.LDIL i4, 0x10 ; fmt=0
	SAU.DIV.U32 i4, i4, i1
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
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i6, i2, i5
	IAU.ADD i4, i4, i5
	SAU.DIV.U32 i4, i4, i6
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
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB0_9
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
LBB0_5:                                 // %for.inc
                                        //   in Loop: Header=BB0_3 Depth=1
	LSU0.LDIL i4, 0x04 ; fmt=0
	IAU.ADD i3, i3, i4
	IAU.MUL i4, i2, i1
	LSU0.LDIL i5, 0xFFFF ; fmt=0
	LSU1.LDIH i5, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i0, i5
	LSU0.LDIL i5, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i0, i5
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
	BRU.BRA i5, LBB0_3
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
LBB0_6:                                 // %for.end
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i0, i4, i0
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
LBB0_7:                                 // %for.body23.preheader
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i3, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.MUL i1, i1, i2
LBB0_8:                                 // %for.body23
                                        // =>This Inner Loop Header: Depth=1
	LSU0.LD.32.F16.F32 i2, i3
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	LSU1.ST.16.F32.F16 i2, i0
	LSU0.LDIL i2, 0x02 ; fmt=0
	IAU.ADD i3, i3, i2
	IAU.ADD i0, i0, i2
	LSU0.LDIL i2, 0xFFFF ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i1, i2
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	IAU.ADD i2, i2, i4
	SAU.DIV.U32 i2, i4, i2
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
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB0_8
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
LBB0_9:                                 // %cleanup30
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end0:
	.size	e0, Lfunc_end0-e0
                                        // -- End function
	.section	.text.e1,"ax",@progbits
	.globl	e1                              // -- Begin function e1
	.type	e1,@function
e1:                                     // @e1
.Le1$local:
// %bb.0:                               // %entry
	LSU0.LDIL i1, 0xFFF0 ; fmt=0
	LSU1.LDIH i1, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i0, i18, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i0, i1
	LSU0.LDIL i2, 0xFFF1 ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0x01 ; fmt=0
	SAU.DIV.U32 i1, i1, i2
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
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB1_9
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
LBB1_1:                                 // %entry
	LSU0.LDO.32 i1, i18, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB1_9
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
LBB1_2:                                 // %for.body.preheader
	LSU0.LDIL i2, 0x03 ; fmt=0
	IAU.ADD i3, i1, i2
	LSU0.LDIL i4, 0x01 ; fmt=0
LBB1_3:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.AND i1, i4, i4
	LSU0.LDIL i2, 0xFFFD ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i3, i2
	LSU0.LDO.32 i2, i2, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i2, i4
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB1_9
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
LBB1_4:                                 // %cleanup
                                        //   in Loop: Header=BB1_3 Depth=1
	LSU0.LDIL i4, 0x10 ; fmt=0
	SAU.DIV.U32 i4, i4, i1
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
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i6, i2, i5
	IAU.ADD i4, i4, i5
	SAU.DIV.U32 i4, i4, i6
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
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB1_9
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
LBB1_5:                                 // %for.inc
                                        //   in Loop: Header=BB1_3 Depth=1
	LSU0.LDIL i4, 0x04 ; fmt=0
	IAU.ADD i3, i3, i4
	IAU.MUL i4, i2, i1
	LSU0.LDIL i5, 0xFFFF ; fmt=0
	LSU1.LDIH i5, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i0, i5
	LSU0.LDIL i5, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i0, i5
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
	BRU.BRA i5, LBB1_3
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
LBB1_6:                                 // %for.end
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i0, i4, i0
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB1_9
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
LBB1_7:                                 // %for.body23.preheader
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i3, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.MUL i1, i1, i2
LBB1_8:                                 // %for.body23
                                        // =>This Inner Loop Header: Depth=1
	LSU0.LD.32.F16.F32 i2, i3
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	LSU1.ST.16.F32.F16 i2, i0
	LSU0.LDIL i2, 0x02 ; fmt=0
	IAU.ADD i3, i3, i2
	IAU.ADD i0, i0, i2
	LSU0.LDIL i2, 0xFFFF ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i1, i2
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	IAU.ADD i2, i2, i4
	SAU.DIV.U32 i2, i4, i2
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
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB1_8
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
LBB1_9:                                 // %cleanup31
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end1:
	.size	e1, Lfunc_end1-e1
                                        // -- End function
	.section	.text.e2,"ax",@progbits
	.globl	e2                              // -- Begin function e2
	.type	e2,@function
e2:                                     // @e2
.Le2$local:
// %bb.0:                               // %entry
	LSU0.LDIL i1, 0xFFF0 ; fmt=0
	LSU1.LDIH i1, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i0, i18, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i0, i1
	LSU0.LDIL i2, 0xFFF1 ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0x01 ; fmt=0
	SAU.DIV.U32 i1, i1, i2
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
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB2_9
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
LBB2_1:                                 // %entry
	LSU0.LDO.32 i1, i18, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB2_9
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
LBB2_2:                                 // %for.body.preheader
	LSU0.LDIL i2, 0x03 ; fmt=0
	IAU.ADD i3, i1, i2
	LSU0.LDIL i4, 0x01 ; fmt=0
LBB2_3:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.AND i1, i4, i4
	LSU0.LDIL i2, 0xFFFD ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i3, i2
	LSU0.LDO.32 i2, i2, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i2, i4
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB2_9
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
LBB2_4:                                 // %cleanup
                                        //   in Loop: Header=BB2_3 Depth=1
	LSU0.LDIL i4, 0x10 ; fmt=0
	SAU.DIV.U32 i4, i4, i1
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
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i6, i2, i5
	IAU.ADD i4, i4, i5
	SAU.DIV.U32 i4, i4, i6
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
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB2_9
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
LBB2_5:                                 // %for.inc
                                        //   in Loop: Header=BB2_3 Depth=1
	LSU0.LDIL i4, 0x04 ; fmt=0
	IAU.ADD i3, i3, i4
	IAU.MUL i4, i2, i1
	LSU0.LDIL i5, 0xFFFF ; fmt=0
	LSU1.LDIH i5, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i0, i5
	LSU0.LDIL i5, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i0, i5
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
	BRU.BRA i5, LBB2_3
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
LBB2_6:                                 // %for.end
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i0, i4, i0
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB2_9
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
LBB2_7:                                 // %for.body23.preheader
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i3, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.MUL i1, i1, i2
LBB2_8:                                 // %for.body23
                                        // =>This Inner Loop Header: Depth=1
	LSU0.LD.32.F16.F32 i2, i3
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	LSU1.ST.16.F32.F16 i2, i0
	LSU0.LDIL i2, 0x02 ; fmt=0
	IAU.ADD i3, i3, i2
	IAU.ADD i0, i0, i2
	LSU0.LDIL i2, 0xFFFF ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i1, i2
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	IAU.ADD i2, i2, i4
	SAU.DIV.U32 i2, i4, i2
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
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB2_8
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
LBB2_9:                                 // %cleanup32
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end2:
	.size	e2, Lfunc_end2-e2
                                        // -- End function
	.section	.text.e3,"ax",@progbits
	.globl	e3                              // -- Begin function e3
	.type	e3,@function
e3:                                     // @e3
.Le3$local:
// %bb.0:                               // %entry
	LSU0.LDIL i1, 0xFFF0 ; fmt=0
	LSU1.LDIH i1, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i0, i18, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i0, i1
	LSU0.LDIL i2, 0xFFF1 ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0x01 ; fmt=0
	SAU.DIV.U32 i1, i1, i2
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
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB3_9
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
LBB3_1:                                 // %entry
	LSU0.LDO.32 i1, i18, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB3_9
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
LBB3_2:                                 // %for.body.preheader
	LSU0.LDIL i2, 0x03 ; fmt=0
	IAU.ADD i3, i1, i2
	LSU0.LDIL i4, 0x01 ; fmt=0
LBB3_3:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.AND i1, i4, i4
	LSU0.LDIL i2, 0xFFFD ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i3, i2
	LSU0.LDO.32 i2, i2, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i2, i4
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB3_9
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
LBB3_4:                                 // %cleanup
                                        //   in Loop: Header=BB3_3 Depth=1
	LSU0.LDIL i4, 0x10 ; fmt=0
	SAU.DIV.U32 i4, i4, i1
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
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i6, i2, i5
	IAU.ADD i4, i4, i5
	SAU.DIV.U32 i4, i4, i6
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
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB3_9
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
LBB3_5:                                 // %for.inc
                                        //   in Loop: Header=BB3_3 Depth=1
	LSU0.LDIL i4, 0x04 ; fmt=0
	IAU.ADD i3, i3, i4
	IAU.MUL i4, i2, i1
	LSU0.LDIL i5, 0xFFFF ; fmt=0
	LSU1.LDIH i5, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i0, i5
	LSU0.LDIL i5, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i0, i5
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
	BRU.BRA i5, LBB3_3
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
LBB3_6:                                 // %for.end
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i0, i4, i0
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB3_9
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
LBB3_7:                                 // %for.body23.preheader
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i3, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.MUL i1, i1, i2
LBB3_8:                                 // %for.body23
                                        // =>This Inner Loop Header: Depth=1
	LSU0.LD.32.F16.F32 i2, i3
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	LSU1.ST.16.F32.F16 i2, i0
	LSU0.LDIL i2, 0x02 ; fmt=0
	IAU.ADD i3, i3, i2
	IAU.ADD i0, i0, i2
	LSU0.LDIL i2, 0xFFFF ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i1, i2
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	IAU.ADD i2, i2, i4
	SAU.DIV.U32 i2, i4, i2
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
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB3_8
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
LBB3_9:                                 // %cleanup33
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end3:
	.size	e3, Lfunc_end3-e3
                                        // -- End function
	.section	.text.e4,"ax",@progbits
	.globl	e4                              // -- Begin function e4
	.type	e4,@function
e4:                                     // @e4
.Le4$local:
// %bb.0:                               // %entry
	LSU0.LDIL i1, 0xFFF0 ; fmt=0
	LSU1.LDIH i1, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i0, i18, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i0, i1
	LSU0.LDIL i2, 0xFFF1 ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0x01 ; fmt=0
	SAU.DIV.U32 i1, i1, i2
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
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB4_9
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
LBB4_1:                                 // %entry
	LSU0.LDO.32 i1, i18, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB4_9
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
LBB4_2:                                 // %for.body.preheader
	LSU0.LDIL i2, 0x03 ; fmt=0
	IAU.ADD i3, i1, i2
	LSU0.LDIL i4, 0x01 ; fmt=0
LBB4_3:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.AND i1, i4, i4
	LSU0.LDIL i2, 0xFFFD ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i3, i2
	LSU0.LDO.32 i2, i2, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i2, i4
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB4_9
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
LBB4_4:                                 // %cleanup
                                        //   in Loop: Header=BB4_3 Depth=1
	LSU0.LDIL i4, 0x10 ; fmt=0
	SAU.DIV.U32 i4, i4, i1
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
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i6, i2, i5
	IAU.ADD i4, i4, i5
	SAU.DIV.U32 i4, i4, i6
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
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB4_9
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
LBB4_5:                                 // %for.inc
                                        //   in Loop: Header=BB4_3 Depth=1
	LSU0.LDIL i4, 0x04 ; fmt=0
	IAU.ADD i3, i3, i4
	IAU.MUL i4, i2, i1
	LSU0.LDIL i5, 0xFFFF ; fmt=0
	LSU1.LDIH i5, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i0, i5
	LSU0.LDIL i5, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i0, i5
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
	BRU.BRA i5, LBB4_3
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
LBB4_6:                                 // %for.end
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i0, i4, i0
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB4_9
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
LBB4_7:                                 // %for.body23.preheader
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i3, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.MUL i1, i1, i2
LBB4_8:                                 // %for.body23
                                        // =>This Inner Loop Header: Depth=1
	LSU0.LD.32.F16.F32 i2, i3
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	LSU1.ST.16.F32.F16 i2, i0
	LSU0.LDIL i2, 0x02 ; fmt=0
	IAU.ADD i3, i3, i2
	IAU.ADD i0, i0, i2
	LSU0.LDIL i2, 0xFFFF ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i1, i2
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	IAU.ADD i2, i2, i4
	SAU.DIV.U32 i2, i4, i2
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
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB4_8
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
LBB4_9:                                 // %cleanup34
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end4:
	.size	e4, Lfunc_end4-e4
                                        // -- End function
	.section	.text.e5,"ax",@progbits
	.globl	e5                              // -- Begin function e5
	.type	e5,@function
e5:                                     // @e5
.Le5$local:
// %bb.0:                               // %entry
	LSU0.LDIL i1, 0xFFF0 ; fmt=0
	LSU1.LDIH i1, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i0, i18, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i0, i1
	LSU0.LDIL i2, 0xFFF1 ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0x01 ; fmt=0
	SAU.DIV.U32 i1, i1, i2
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
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB5_9
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
LBB5_1:                                 // %entry
	LSU0.LDO.32 i1, i18, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB5_9
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
LBB5_2:                                 // %for.body.preheader
	LSU0.LDIL i2, 0x03 ; fmt=0
	IAU.ADD i3, i1, i2
	LSU0.LDIL i4, 0x01 ; fmt=0
LBB5_3:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.AND i1, i4, i4
	LSU0.LDIL i2, 0xFFFD ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i3, i2
	LSU0.LDO.32 i2, i2, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i2, i4
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB5_9
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
LBB5_4:                                 // %cleanup
                                        //   in Loop: Header=BB5_3 Depth=1
	LSU0.LDIL i4, 0x10 ; fmt=0
	SAU.DIV.U32 i4, i4, i1
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
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i6, i2, i5
	IAU.ADD i4, i4, i5
	SAU.DIV.U32 i4, i4, i6
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
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB5_9
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
LBB5_5:                                 // %for.inc
                                        //   in Loop: Header=BB5_3 Depth=1
	LSU0.LDIL i4, 0x04 ; fmt=0
	IAU.ADD i3, i3, i4
	IAU.MUL i4, i2, i1
	LSU0.LDIL i5, 0xFFFF ; fmt=0
	LSU1.LDIH i5, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i0, i5
	LSU0.LDIL i5, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i0, i5
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
	BRU.BRA i5, LBB5_3
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
LBB5_6:                                 // %for.end
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i0, i4, i0
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB5_9
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
LBB5_7:                                 // %for.body23.preheader
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i3, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.MUL i1, i1, i2
LBB5_8:                                 // %for.body23
                                        // =>This Inner Loop Header: Depth=1
	LSU0.LD.32.F16.F32 i2, i3
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	LSU1.ST.16.F32.F16 i2, i0
	LSU0.LDIL i2, 0x02 ; fmt=0
	IAU.ADD i3, i3, i2
	IAU.ADD i0, i0, i2
	LSU0.LDIL i2, 0xFFFF ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i1, i2
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	IAU.ADD i2, i2, i4
	SAU.DIV.U32 i2, i4, i2
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
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB5_8
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
LBB5_9:                                 // %cleanup35
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end5:
	.size	e5, Lfunc_end5-e5
                                        // -- End function
	.section	.text.e6,"ax",@progbits
	.globl	e6                              // -- Begin function e6
	.type	e6,@function
e6:                                     // @e6
.Le6$local:
// %bb.0:                               // %entry
	LSU0.LDIL i1, 0xFFF0 ; fmt=0
	LSU1.LDIH i1, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i0, i18, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i0, i1
	LSU0.LDIL i2, 0xFFF1 ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0x01 ; fmt=0
	SAU.DIV.U32 i1, i1, i2
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
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB6_9
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
LBB6_1:                                 // %entry
	LSU0.LDO.32 i1, i18, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB6_9
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
LBB6_2:                                 // %for.body.preheader
	LSU0.LDIL i2, 0x03 ; fmt=0
	IAU.ADD i3, i1, i2
	LSU0.LDIL i4, 0x01 ; fmt=0
LBB6_3:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.AND i1, i4, i4
	LSU0.LDIL i2, 0xFFFD ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i3, i2
	LSU0.LDO.32 i2, i2, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i2, i4
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB6_9
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
LBB6_4:                                 // %cleanup
                                        //   in Loop: Header=BB6_3 Depth=1
	LSU0.LDIL i4, 0x10 ; fmt=0
	SAU.DIV.U32 i4, i4, i1
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
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i6, i2, i5
	IAU.ADD i4, i4, i5
	SAU.DIV.U32 i4, i4, i6
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
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB6_9
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
LBB6_5:                                 // %for.inc
                                        //   in Loop: Header=BB6_3 Depth=1
	LSU0.LDIL i4, 0x04 ; fmt=0
	IAU.ADD i3, i3, i4
	IAU.MUL i4, i2, i1
	LSU0.LDIL i5, 0xFFFF ; fmt=0
	LSU1.LDIH i5, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i0, i5
	LSU0.LDIL i5, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i0, i5
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
	BRU.BRA i5, LBB6_3
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
LBB6_6:                                 // %for.end
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i0, i4, i0
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB6_9
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
LBB6_7:                                 // %for.body23.preheader
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i3, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.MUL i1, i1, i2
LBB6_8:                                 // %for.body23
                                        // =>This Inner Loop Header: Depth=1
	LSU0.LD.32.F16.F32 i2, i3
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	LSU1.ST.16.F32.F16 i2, i0
	LSU0.LDIL i2, 0x02 ; fmt=0
	IAU.ADD i3, i3, i2
	IAU.ADD i0, i0, i2
	LSU0.LDIL i2, 0xFFFF ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i1, i2
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	IAU.ADD i2, i2, i4
	SAU.DIV.U32 i2, i4, i2
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
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB6_8
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
LBB6_9:                                 // %cleanup36
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end6:
	.size	e6, Lfunc_end6-e6
                                        // -- End function
	.section	.text.e7,"ax",@progbits
	.globl	e7                              // -- Begin function e7
	.type	e7,@function
e7:                                     // @e7
.Le7$local:
// %bb.0:                               // %entry
	LSU0.LDIL i1, 0xFFF0 ; fmt=0
	LSU1.LDIH i1, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i0, i18, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i0, i1
	LSU0.LDIL i2, 0xFFF1 ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0x01 ; fmt=0
	SAU.DIV.U32 i1, i1, i2
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
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB7_9
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
LBB7_1:                                 // %entry
	LSU0.LDO.32 i1, i18, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB7_9
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
LBB7_2:                                 // %for.body.preheader
	LSU0.LDIL i2, 0x03 ; fmt=0
	IAU.ADD i3, i1, i2
	LSU0.LDIL i4, 0x01 ; fmt=0
LBB7_3:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.AND i1, i4, i4
	LSU0.LDIL i2, 0xFFFD ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i3, i2
	LSU0.LDO.32 i2, i2, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i2, i4
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB7_9
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
LBB7_4:                                 // %cleanup
                                        //   in Loop: Header=BB7_3 Depth=1
	LSU0.LDIL i4, 0x10 ; fmt=0
	SAU.DIV.U32 i4, i4, i1
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
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i6, i2, i5
	IAU.ADD i4, i4, i5
	SAU.DIV.U32 i4, i4, i6
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
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB7_9
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
LBB7_5:                                 // %for.inc
                                        //   in Loop: Header=BB7_3 Depth=1
	LSU0.LDIL i4, 0x04 ; fmt=0
	IAU.ADD i3, i3, i4
	IAU.MUL i4, i2, i1
	LSU0.LDIL i5, 0xFFFF ; fmt=0
	LSU1.LDIH i5, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i0, i5
	LSU0.LDIL i5, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i0, i5
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
	BRU.BRA i5, LBB7_3
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
LBB7_6:                                 // %for.end
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i0, i4, i0
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB7_9
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
LBB7_7:                                 // %for.body23.preheader
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i3, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.MUL i1, i1, i2
LBB7_8:                                 // %for.body23
                                        // =>This Inner Loop Header: Depth=1
	LSU0.LD.32.F16.F32 i2, i3
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i2, i2, i1
	BRU.NOP
	BRU.NOP
	LSU1.ST.16.F32.F16 i2, i0
	LSU0.LDIL i2, 0x02 ; fmt=0
	IAU.ADD i3, i3, i2
	IAU.ADD i0, i0, i2
	LSU0.LDIL i2, 0xFFFF ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i1, i2
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i2, i1, i2
	IAU.ADD i2, i2, i4
	SAU.DIV.U32 i2, i4, i2
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
	CMU.CMZ.I32 i2
	BRU.BRA i2, LBB7_8
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
LBB7_9:                                 // %cleanup37
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end7:
	.size	e7, Lfunc_end7-e7
                                        // -- End function

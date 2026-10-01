	.text
	.file	"multi_fadd.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
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
	.size	controlled_act, Lfunc_end0-controlled_act
                                        // -- End function
	.section	.text.multi_b,"ax",@progbits
	.globl	multi_b                         // -- Begin function multi_b
	.type	multi_b,@function
multi_b:                                // @multi_b
.Lmulti_b$local:
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
	.size	multi_b, Lfunc_end1-multi_b
                                        // -- End function
	.section	.text.multi_c,"ax",@progbits
	.globl	multi_c                         // -- Begin function multi_c
	.type	multi_c,@function
multi_c:                                // @multi_c
.Lmulti_c$local:
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
	.size	multi_c, Lfunc_end2-multi_c
                                        // -- End function
	.section	.text.multi_d,"ax",@progbits
	.globl	multi_d                         // -- Begin function multi_d
	.type	multi_d,@function
multi_d:                                // @multi_d
.Lmulti_d$local:
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
	.size	multi_d, Lfunc_end3-multi_d
                                        // -- End function

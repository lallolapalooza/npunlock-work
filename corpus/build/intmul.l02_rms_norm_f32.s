	.text
	.file	"entry.c"
	.section	.text.strtof,"ax",@progbits
	.globl	strtof                          // -- Begin function strtof
	.type	strtof,@function
strtof:                                 // @strtof
.Lstrtof$local:
// %bb.0:                               // %entry
	LSU0.LDIL i0, 0x00 ; fmt=0
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end0:
	.size	strtof, Lfunc_end0-strtof
                                        // -- End function
	.section	.text.__truncdfsf2,"ax",@progbits
	.globl	__truncdfsf2                    // -- Begin function __truncdfsf2
	.type	__truncdfsf2,@function
__truncdfsf2:                           // @__truncdfsf2
.L__truncdfsf2$local:
// %bb.0:                               // %entry
	LSU0.LDIL i0, 0x00 ; fmt=0
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end1:
	.size	__truncdfsf2, Lfunc_end1-__truncdfsf2
                                        // -- End function
	.section	.text.__fixsfdi,"ax",@progbits
	.globl	__fixsfdi                       // -- Begin function __fixsfdi
	.type	__fixsfdi,@function
__fixsfdi:                              // @__fixsfdi
.L__fixsfdi$local:
// %bb.0:                               // %entry
	IAU.SHR.U32 i0, i18, 23 ; fmt=3
	LSU0.LDIL i1, 0xFF ; fmt=0
	IAU.AND i1, i0, i1
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i0, 0x7F ; fmt=0
	LSU0.LDIL i2, 0x01 ; fmt=0
	SAU.DIV.U32 i0, i1, i0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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
	BRU.BRA i0, LBB2_10
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
LBB2_1:                                 // %if.end
	LSU0.LDIL i0, 0xFFFF ; fmt=0
	LSU1.LDIH i0, 0x7F
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i0, i18, i0
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU1.LDIH i2, 0x80
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.OR i0, i0, i2
	LSU0.LDIL i2, 0x96 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.ADD i4, i1, i3
	IAU.ADD i2, i2, i3
	SAU.DIV.U32 i2, i2, i4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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
	BRU.BRA i2, LBB2_3
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
LBB2_2:                                 // %cond.true
	LSU0.LDIL i2, 0x96 ; fmt=0
	IAU.SUB i1, i2, i1
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i3, i1, i2
	CMU.CMZ.I32 i3
	LSU0.LDIL i3, 0x20 ; fmt=0
	LSU0.LDIL i4, 0x1F ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i5, i1, i5
	SAU.DIV.U32 i3, i3, i5
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i3
	IAU.SUB i2, i2, i3
	IAU.AND i1, i1, i4
	IAU.SHR.U32 i0, i0, i1
	IAU.AND i1, i0, i2
	BRU.BRA LBB2_4
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB2_3:                                 // %cond.false
	LSU0.LDIL i2, 0xB6 ; fmt=0
	LSU0.LDIL i2, 0xFF6A ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i1, i1, i2
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i3, i1, i2
	CMU.CMZ.I32 i3
	LSU0.LDIL i3, 0x1F ; fmt=0
	LSU0.LDIL i4, 0x20 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i5, i1, i5
	SAU.DIV.U32 i4, i4, i5
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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
	IAU.SUB i2, i2, i4
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.ADD i4, i3, i4
	SAU.DIV.U32 i4, i1, i4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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
	IAU.AND i1, i1, i3
	IAU.SHL i0, i0, i1
	IAU.AND i1, i0, i2
LBB2_4:                                 // %cond.end
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i2, 0x01 ; fmt=0
	LSU0.LDIL i3, 0x00 ; fmt=0
	LSU1.LDIH i3, 0x8000
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.XOR i4, i18, i3
	IAU.XOR i3, i0, i3
	IAU.ADD i2, i4, i2
	SAU.DIV.U32 i2, i3, i2
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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
	IAU.SUB i1, i0, i1
	BRU.BRA i2, LBB2_5
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB2_6
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB2_5:
	BRU.BRA LBB2_7
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB2_6:
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB2_7:
	LSU0.LDIL i3, 0x01 ; fmt=0
	IAU.XOR i0, i1, i0
	CMU.CMZ.I32 i0
	BRU.BRA i2, LBB2_8
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB2_9
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB2_8:
	BRU.BRA LBB2_10
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB2_9:
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB2_10:                                // %cleanup
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end2:
	.size	__fixsfdi, Lfunc_end2-__fixsfdi
                                        // -- End function
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
	BRU.BRA i0, LBB3_15
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
LBB3_1:                                 // %if.end.i
	LSU0.LDO.32 i0, i18, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i1, 0xFFF0 ; fmt=0
	LSU1.LDIH i1, 0xFFFF
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
	BRU.BRA i1, LBB3_15
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
LBB3_2:                                 // %if.end.i
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
	BRU.BRA i2, LBB3_15
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
LBB3_3:                                 // %for.body.i.preheader
	LSU0.LDIL i2, 0x03 ; fmt=0
	IAU.ADD i2, i1, i2
	LSU0.LDIL i12, 0x01 ; fmt=0
LBB3_4:                                 // %for.body.i
                                        // =>This Inner Loop Header: Depth=1
	LSU0.LDIL i3, 0xFFFD ; fmt=0
	LSU1.LDIH i3, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i3, i2, i3
	LSU0.LDO.32 i3, i3, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i3, i4
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB3_15
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
LBB3_5:                                 // %for.inc.i
                                        //   in Loop: Header=BB3_4 Depth=1
	LSU0.LDIL i4, 0x04 ; fmt=0
	IAU.ADD i2, i2, i4
	IAU.MUL i12, i3, i12
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i3, 0xFFFF ; fmt=0
	LSU1.LDIH i3, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i0, i3
	LSU0.LDIL i3, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i3, i0, i3
	IAU.ADD i3, i3, i4
	SAU.DIV.U32 i3, i4, i3
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i3
	BRU.BRA i3, LBB3_4
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
LBB3_6:                                 // %do.end
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i2, 0x01 ; fmt=0
	IAU.XOR i0, i12, i0
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB3_15
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
LBB3_7:                                 // %for.body.lr.ph
	LSU0.LDO.32 i13, i1, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i14, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i15, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i16, 0x00 ; fmt=0
	IAU.SHL i20, i13, 2 ; fmt=3
	CMU.CP.32.F32.U32S i21, i13 ; fmt=8
LBB3_8:                                 // %for.body
                                        // =>This Loop Header: Depth=1
                                        //     Child Loop BB3_12 Depth 2
                                        //     Child Loop BB3_14 Depth 2
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i1, 0x01 ; fmt=0
	IAU.XOR i0, i13, i0
	CMU.CMZ.I32 i0
	BRU.BRA i0, LBB3_13
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
LBB3_9:                                 // %for.body14.preheader
                                        //   in Loop: Header=BB3_8 Depth=1
	LSU0.LDIL i0, 0x00 ; fmt=0
	IAU.AND i2, i15, i15
	IAU.AND i1, i13, i13
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB3_12:                                // %for.body14
                                        //   Parent Loop BB3_8 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	LSU0.LDIL i3, 0x04 ; fmt=0
	IAU.ADD i3, i2, i3
	LSU0.LDO.32 i2, i2, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.MUL.F32 i2, i2, i2 ; fmt=2
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i0, i0, i2 ; fmt=2
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
	IAU.XOR i4, i1, i2
	CMU.CMZ.I32 i4
	IAU.AND i2, i3, i3
	BRU.BRA i4, LBB3_10
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB3_12
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB3_10:                                // %for.cond.cleanup13
                                        //   in Loop: Header=BB3_8 Depth=1
	SAU.DIV.F32 i0, i0, i21 ; fmt=1
	LSU0.LDIL i1, 0xC5AC ; fmt=0
	LSU1.LDIH i1, 0x3727
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.ADD.F32 i18, i0, i1 ; fmt=2
	IAU.SUB i19, i19, 8
	LSU1.STO.32 i30, i19, 0x4
	LSU0.LDIL i30, sqrtf
	LSU1.LDIH i30, sqrtf
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.SWP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i30, i19, 0x4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i19, i19, 8
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU1.LDIH i0, 0x3F80
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i1, 0x00 ; fmt=0
	LSU0.LDIL i2, 0x01 ; fmt=0
	IAU.XOR i1, i13, i1
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB3_13
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
LBB3_11:                                // %for.body23.preheader
                                        //   in Loop: Header=BB3_8 Depth=1
	SAU.DIV.F32 i0, i0, i18 ; fmt=1
	LSU0.LDIL i1, 0x00 ; fmt=0
	IAU.AND i2, i13, i13
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB3_14:                                // %for.body23
                                        //   Parent Loop BB3_8 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	IAU.ADD i3, i15, i1
	LSU0.LDO.32 i3, i3, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.MUL.F32 i3, i0, i3 ; fmt=2
	BRU.NOP
	IAU.ADD i4, i14, i1
	LSU1.ST.32 i3, i4
	LSU0.LDIL i3, 0x04 ; fmt=0
	IAU.ADD i1, i1, i3
	LSU0.LDIL i3, 0xFFFF ; fmt=0
	LSU1.LDIH i3, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i2, i3
	LSU0.LDIL i3, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i3, i2, i3
	CMU.CMZ.I32 i3
	BRU.BRA i3, LBB3_13
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB3_14
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB3_13:                                // %for.cond.cleanup22
                                        //   in Loop: Header=BB3_8 Depth=1
	IAU.ADD i14, i14, i20
	IAU.ADD i15, i15, i20
	IAU.ADD i16, i16, i13
	LSU0.LDIL i0, 0x01 ; fmt=0
	SAU.DIV.U32 i0, i16, i12
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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
	BRU.BRA i0, LBB3_8
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
LBB3_15:                                // %cleanup
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end3:
	.size	controlled_act, Lfunc_end3-controlled_act
                                        // -- End function

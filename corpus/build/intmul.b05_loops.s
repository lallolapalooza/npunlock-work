	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDO.32 i1, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i0, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i3, i2, i2
LBB0_1:                                 // %for.body
                                        // =>This Loop Header: Depth=1
                                        //     Child Loop BB0_5 Depth 2
                                        //     Child Loop BB0_7 Depth 2
	IAU.SHL i4, i2, 2 ; fmt=3
	IAU.ADD i4, i0, i4
	LSU0.LDO.32 i4, i4, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i5, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i5, i4, i5
	CMU.CMZ.I32 i5
	BRU.BRA i5, LBB0_9
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
LBB0_2:                                 // %if.end
                                        //   in Loop: Header=BB0_1 Depth=1
	LSU0.LDIL i5, 0x86A0 ; fmt=0
	LSU1.LDIH i5, 0x01
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i4, i4, i6
	IAU.ADD i5, i5, i6
	SAU.DIV.U32 i4, i5, i4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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
	BRU.BRA i4, LBB0_10
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
LBB0_3:                                 // %if.end6
                                        //   in Loop: Header=BB0_1 Depth=1
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i2, i4
	CMU.CMZ.I32 i4
	BRU.BRA i4, LBB0_7
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
LBB0_4:                                 // %while.body.preheader
                                        //   in Loop: Header=BB0_1 Depth=1
	IAU.AND i4, i2, i2
LBB0_5:                                 // %while.body
                                        //   Parent Loop BB0_1 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	LSU0.LDIL i5, 0xFFFF ; fmt=0
	LSU1.LDIH i5, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i3, i4, i3
	LSU0.LDIL i6, 0x00 ; fmt=0
	LSU1.LDIH i6, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i7, 0x01 ; fmt=0
	IAU.ADD i8, i3, i7
	IAU.ADD i6, i6, i7
	SAU.DIV.U32 i6, i6, i8
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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
	BRU.BRA i6, LBB0_7
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
LBB0_6:                                 // %while.body
                                        //   in Loop: Header=BB0_5 Depth=2
	IAU.ADD i5, i4, i5
	LSU0.LDIL i6, 0x01 ; fmt=0
	LSU0.LDIL i7, 0x01 ; fmt=0
	IAU.XOR i4, i4, i6
	IAU.ADD i4, i4, i7
	SAU.DIV.U32 i6, i7, i4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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
	IAU.AND i4, i5, i5
	BRU.BRA i6, LBB0_5
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
LBB0_7:                                 // %do.body
                                        //   Parent Loop BB0_1 Depth=1
                                        // =>  This Inner Loop Header: Depth=2
	IAU.SHL i4, i3, 3 ; fmt=3
	IAU.XOR i4, i4, i3
	LSU0.LDIL i3, 0xFFFF ; fmt=0
	LSU1.LDIH i3, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i3, i4, i3
	LSU0.LDIL i5, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	LSU0.LDIL i6, 0x00 ; fmt=0
	LSU1.LDIH i6, 0x8000
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.XOR i7, i3, i6
	IAU.XOR i5, i5, i6
	SAU.DIV.U32 i5, i7, i5
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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
	BRU.BRA i5, LBB0_7
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
LBB0_8:                                 // %for.cond15.preheader.preheader
                                        //   in Loop: Header=BB0_1 Depth=1
	LSU0.LDIL i3, 0x15 ; fmt=0
	IAU.ADD i3, i4, i3
LBB0_9:                                 // %for.inc26
                                        //   in Loop: Header=BB0_1 Depth=1
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.ADD i2, i2, i4
	LSU0.LDIL i4, 0x10 ; fmt=0
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.XOR i4, i2, i4
	IAU.ADD i4, i4, i5
	SAU.DIV.U32 i4, i5, i4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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
	BRU.BRA i4, LBB0_1
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
LBB0_10:                                // %cleanup
	LSU1.ST.32 i3, i1
	LSU0.LDIL i2, 0x04 ; fmt=0
	IAU.ADD i1, i1, i2
	LSU0.LDO.32 i0, i0, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
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

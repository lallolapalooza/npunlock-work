	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDIL i0, 0x5678 ; fmt=0
	LSU1.LDIH i0, 0x1234
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i1, 0x00 ; fmt=0
	LSU0.LDIL i2, 0x28 ; fmt=0
	IAU.ADD i6, i18, i2
	LSU0.LDIL i2, 0x29 ; fmt=0
	IAU.ADD i5, i18, i2
	LSU0.LDIL i2, 0x2A ; fmt=0
	IAU.ADD i4, i18, i2
	LSU0.LDIL i2, 0x2B ; fmt=0
	IAU.ADD i2, i18, i2
	LSU0.LDO.32 i3, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LD.8 i2, i2
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i2, i2, 24
	IAU.SHR.U32 i2, i2, 24
	LSU0.LD.8 i4, i4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i4, i4, 24
	IAU.SHR.U32 i4, i4, 24
	LSU0.LD.8 i5, i5
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i5, i5, 24
	IAU.SHR.U32 i5, i5, 24
	LSU0.LD.8 i6, i6
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i6, i6, 24
	IAU.SHR.U32 i6, i6, 24
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_2:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.ADD i7, i3, i1
	LSU0.LDO.32 i7, i7, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHR.U32 i8, i7, 2
	IAU.SUB i8, i0, i8
	LSU0.LDIL i9, 0x05 ; fmt=0
	IAU.MUL i9, i7, i9
	IAU.ADD i8, i8, i9
	LSU0.LDIL i9, 0xFF ; fmt=0
	LSU1.LDIH i9, 0xFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i9, i7, i9
	IAU.XOR i8, i8, i9
	LSU0.LDIL i9, 0xA5A5 ; fmt=0
	LSU1.LDIH i9, 0xA5A5
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.XOR i9, i7, i9
	IAU.OR i8, i8, i9
	LSU0.LDIL i9, 0xFFF8 ; fmt=0
	LSU1.LDIH i9, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SUB i9, i9, i7
	IAU.AND i9, i8, i9
	LSU0.LDIL i8, 0x00 ; fmt=0
	LSU0.LDIL i10, 0xFFFF ; fmt=0
	LSU1.LDIH i10, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i11, 0x01 ; fmt=0
	LSU0.LDIL i12, 0x00 ; fmt=0
	LSU1.LDIH i12, 0x8000
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.XOR i13, i9, i12
	IAU.XOR i12, i7, i12
	IAU.ADD i11, i13, i11
	SAU.DIV.U32 i11, i12, i11
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i11
	BRU.BRA i11, LBB0_3
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_4
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_3:                                 //   in Loop: Header=BB0_2 Depth=1
	BRU.BRA LBB0_5
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_4:                                 //   in Loop: Header=BB0_2 Depth=1
	IAU.AND i10, i8, i8
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_5:                                 //   in Loop: Header=BB0_2 Depth=1
	IAU.XOR i9, i9, i10
	LSU0.LDIL i10, 0x01 ; fmt=0
	IAU.ADD i10, i7, i10
	SAU.DIV.U32 i10, i9, i10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i10
	IAU.SUB i9, i9, i10
	LSU0.LDIL i10, 0x01 ; fmt=0
	IAU.ADD i10, i9, i10
	SAU.DIV.U32 i10, i7, i10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i10
	BRU.BRA i10, LBB0_6
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_7
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_6:                                 //   in Loop: Header=BB0_2 Depth=1
	BRU.BRA LBB0_8
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_7:                                 //   in Loop: Header=BB0_2 Depth=1
	IAU.AND i9, i7, i7
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_8:                                 //   in Loop: Header=BB0_2 Depth=1
	LSU0.LDIL i10, 0x01 ; fmt=0
	LSU0.LDIL i11, 0x00 ; fmt=0
	LSU1.LDIH i11, 0x8000
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.XOR i12, i9, i11
	IAU.XOR i11, i7, i11
	IAU.ADD i10, i11, i10
	SAU.DIV.U32 i10, i12, i10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i10
	BRU.BRA i10, LBB0_9
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_10
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_9:                                 //   in Loop: Header=BB0_2 Depth=1
	BRU.BRA LBB0_11
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_10:                                //   in Loop: Header=BB0_2 Depth=1
	IAU.AND i8, i7, i7
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_11:                                //   in Loop: Header=BB0_2 Depth=1
	LSU0.LDIL i7, 0x04 ; fmt=0
	IAU.ADD i10, i1, i7
	IAU.XOR i7, i8, i9
	LSU0.LDIL i8, 0x40 ; fmt=0
	LSU0.LDIL i9, 0x01 ; fmt=0
	IAU.XOR i8, i10, i8
	CMU.CMZ.I32 i8
	BRU.BRA i8, LBB0_1
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_2
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_1:                                 // %for.cond.cleanup
	IAU.SHL i0, i5, 8
	IAU.OR i0, i0, i6
	IAU.SHL i1, i4, 16
	IAU.OR i0, i0, i1
	IAU.SHL i1, i2, 24
	IAU.OR i0, i0, i1
	LSU0.ST.32 i7, i0
	LSU0.LDIL i1, 0x04 ; fmt=0
	IAU.ADD i0, i0, i1
	LSU0.LDIL i1, 0xAAAB ; fmt=0
	LSU1.LDIH i1, 0xAAAA
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.LMULH i1, i7, i1
	IAU.SHR.U32 i1, i1, 1
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

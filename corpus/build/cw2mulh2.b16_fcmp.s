	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDIL i0, 0x00 ; fmt=0
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
	LSU0.LDIL i8, 0xFFFF ; fmt=0
	LSU1.LDIH i8, 0x7FFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i9, i7, i8
	IAU.SHR.U32 i10, i9, 16
	LSU0.LDIL i11, 0x101 ; fmt=0
	IAU.MUL i12, i10, i11
	LSU0.LDIL i13, 0xFF ; fmt=0
	IAU.MUL i10, i10, i13
	IAU.SHR.U32 i14, i10, 16
	IAU.ADD i12, i12, i14
	LSU0.LDIL i14, 0x00 ; fmt=0
	LSU1.LDIH i14, 0x3F00
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i8, i14, i8
	LSU0.LDIL i15, 0xFFFF ; fmt=0
	IAU.AND i16, i14, i15
	IAU.MUL i17, i16, i11
	IAU.AND i18, i17, i15
	IAU.SHR.U32 i20, i8, 16
	IAU.MUL i21, i20, i13
	IAU.AND i22, i21, i15
	IAU.ADD i18, i22, i18
	IAU.MUL i20, i20, i11
	IAU.SHR.U32 i21, i21, 16
	IAU.ADD i20, i20, i21
	IAU.AND i21, i7, i15
	IAU.MUL i11, i21, i11
	IAU.SHR.U32 i22, i11, 16
	IAU.ADD i12, i12, i22
	IAU.AND i11, i11, i15
	IAU.AND i10, i10, i15
	IAU.ADD i10, i10, i11
	IAU.MUL i11, i16, i13
	IAU.MUL i13, i21, i13
	IAU.SHR.U32 i13, i13, 16
	IAU.ADD i10, i13, i10
	IAU.SHR.U32 i10, i10, 16
	IAU.ADD i10, i12, i10
	IAU.SHR.U32 i12, i17, 16
	IAU.ADD i12, i20, i12
	IAU.SHR.U32 i11, i11, 16
	IAU.ADD i11, i11, i18
	LSU0.LDIL i13, 0x1F ; fmt=0
	IAU.SHR.I32 i15, i7, i13
	LSU0.LDIL i16, 0x00 ; fmt=0
	LSU1.LDIH i16, 0x8000
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.OR i15, i15, i16
	IAU.XOR i15, i7, i15
	IAU.SHR.I32 i13, i14, i13
	IAU.OR i13, i13, i16
	IAU.XOR i13, i14, i13
	LSU0.LDIL i14, 0x01 ; fmt=0
	IAU.ADD i13, i13, i14
	SAU.DIV.U32 i13, i15, i13
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i13
	IAU.SUB i9, i9, i10
	IAU.SHR.U32 i9, i9, 1
	IAU.ADD i9, i9, i10
	IAU.SHR.U32 i10, i11, 16
	IAU.ADD i10, i12, i10
	IAU.SUB i8, i8, i10
	IAU.SHR.U32 i8, i8, 1
	IAU.ADD i8, i8, i10
	IAU.SHR.U32 i8, i8, 30
	LSU0.LDIL i10, 0x01 ; fmt=0
	IAU.SUB i8, i10, i8
	IAU.SHR.U32 i9, i9, 30
	IAU.SUB i9, i10, i9
	IAU.MUL i8, i9, i8
	IAU.MUL i8, i13, i8
	BRU.BRA i8, LBB0_4
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
LBB0_3:                                 // %if.then
                                        //   in Loop: Header=BB0_2 Depth=1
	SAU.ADD.F32 i10, i0, i7 ; fmt=2
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_6
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_4:                                 // %if.else
                                        //   in Loop: Header=BB0_2 Depth=1
	LSU0.LDIL i8, 0xFFFF ; fmt=0
	LSU1.LDIH i8, 0x7FFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i9, i7, i8
	IAU.SHR.U32 i10, i9, 16
	LSU0.LDIL i11, 0x101 ; fmt=0
	IAU.MUL i12, i10, i11
	LSU0.LDIL i13, 0xFF ; fmt=0
	IAU.MUL i10, i10, i13
	IAU.SHR.U32 i14, i10, 16
	IAU.ADD i12, i12, i14
	LSU0.LDIL i14, 0x00 ; fmt=0
	LSU1.LDIH i14, 0xBF00
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i8, i14, i8
	LSU0.LDIL i15, 0xFFFF ; fmt=0
	IAU.AND i16, i14, i15
	IAU.MUL i17, i16, i11
	IAU.AND i18, i17, i15
	IAU.SHR.U32 i20, i8, 16
	IAU.MUL i21, i20, i13
	IAU.AND i22, i21, i15
	IAU.ADD i18, i22, i18
	IAU.MUL i20, i20, i11
	IAU.SHR.U32 i21, i21, 16
	IAU.ADD i20, i20, i21
	IAU.AND i21, i7, i15
	IAU.MUL i11, i21, i11
	IAU.SHR.U32 i22, i11, 16
	IAU.ADD i12, i12, i22
	IAU.AND i11, i11, i15
	IAU.AND i10, i10, i15
	IAU.ADD i10, i10, i11
	IAU.MUL i11, i21, i13
	IAU.SHR.U32 i11, i11, 16
	IAU.ADD i10, i11, i10
	IAU.MUL i11, i16, i13
	IAU.SHR.U32 i10, i10, 16
	IAU.ADD i10, i12, i10
	IAU.SHR.U32 i12, i17, 16
	IAU.ADD i12, i20, i12
	IAU.SHR.U32 i11, i11, 16
	IAU.ADD i11, i11, i18
	LSU0.LDIL i13, 0x1F ; fmt=0
	IAU.SHR.I32 i15, i14, i13
	LSU0.LDIL i16, 0x00 ; fmt=0
	LSU1.LDIH i16, 0x8000
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.OR i15, i15, i16
	IAU.XOR i14, i14, i15
	IAU.SHR.I32 i13, i7, i13
	IAU.OR i13, i13, i16
	IAU.XOR i13, i7, i13
	LSU0.LDIL i15, 0x01 ; fmt=0
	IAU.ADD i13, i13, i15
	SAU.DIV.U32 i13, i14, i13
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i13
	IAU.SUB i9, i9, i10
	IAU.SHR.U32 i9, i9, 1
	IAU.ADD i9, i9, i10
	IAU.SHR.U32 i10, i11, 16
	IAU.ADD i10, i12, i10
	IAU.SUB i8, i8, i10
	IAU.SHR.U32 i8, i8, 1
	IAU.ADD i8, i8, i10
	IAU.SHR.U32 i8, i8, 30
	LSU0.LDIL i10, 0x01 ; fmt=0
	IAU.SUB i8, i10, i8
	IAU.SHR.U32 i9, i9, 30
	IAU.SUB i9, i10, i9
	IAU.MUL i8, i9, i8
	IAU.MUL i8, i13, i8
	IAU.AND i10, i0, i0
	BRU.BRA i8, LBB0_6
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
LBB0_5:                                 // %if.then4
                                        //   in Loop: Header=BB0_2 Depth=1
	SAU.SUB.F32 i10, i0, i7 ; fmt=2
	BRU.NOP
	BRU.NOP
LBB0_6:                                 // %if.end5
                                        //   in Loop: Header=BB0_2 Depth=1
	LSU0.LDIL i12, 0xFFFF ; fmt=0
	LSU1.LDIH i12, 0x7FFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i15, 0x00 ; fmt=0
	IAU.AND i16, i15, i12
	IAU.SHR.U32 i11, i16, 16
	LSU0.LDIL i9, 0x101 ; fmt=0
	IAU.MUL i13, i11, i9
	LSU0.LDIL i8, 0xFF ; fmt=0
	IAU.MUL i14, i11, i8
	IAU.SHR.U32 i11, i14, 16
	IAU.ADD i13, i13, i11
	LSU0.LDIL i11, 0xFFFF ; fmt=0
	IAU.AND i17, i7, i11
	IAU.MUL i18, i17, i9
	IAU.AND i20, i18, i11
	IAU.AND i21, i7, i12
	IAU.SHR.U32 i22, i21, 16
	IAU.MUL i23, i22, i8
	IAU.AND i24, i23, i11
	IAU.ADD i20, i24, i20
	IAU.MUL i22, i22, i9
	IAU.SHR.U32 i23, i23, 16
	IAU.ADD i22, i22, i23
	IAU.SHR.U32 i18, i18, 16
	IAU.ADD i18, i22, i18
	IAU.MUL i17, i17, i8
	IAU.SHR.U32 i17, i17, 16
	IAU.ADD i17, i17, i20
	IAU.AND i20, i15, i11
	IAU.MUL i22, i20, i9
	IAU.SHR.U32 i23, i22, 16
	IAU.ADD i13, i13, i23
	IAU.AND i22, i22, i11
	IAU.AND i14, i14, i11
	IAU.ADD i14, i14, i22
	IAU.MUL i20, i20, i8
	IAU.SHR.U32 i20, i20, 16
	IAU.ADD i14, i20, i14
	IAU.SHR.U32 i14, i14, 16
	IAU.ADD i20, i13, i14
	IAU.SHR.U32 i13, i17, 16
	IAU.ADD i17, i18, i13
	LSU0.LDIL i13, 0x1F ; fmt=0
	IAU.SHR.I32 i18, i15, i13
	LSU0.LDIL i14, 0x00 ; fmt=0
	LSU1.LDIH i14, 0x8000
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.OR i18, i18, i14
	IAU.XOR i18, i15, i18
	IAU.SHR.I32 i15, i7, i13
	IAU.OR i15, i15, i14
	IAU.XOR i15, i7, i15
	IAU.SUB i21, i21, i17
	IAU.SHR.U32 i21, i21, 1
	IAU.ADD i17, i21, i17
	IAU.SUB i16, i16, i20
	IAU.SHR.U32 i16, i16, 1
	IAU.ADD i16, i16, i20
	LSU0.LDIL i20, 0x01 ; fmt=0
	IAU.XOR i18, i15, i18
	IAU.ADD i18, i18, i20
	SAU.DIV.U32 i20, i20, i18
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i20
	IAU.SHR.U32 i18, i16, 30
	LSU0.LDIL i16, 0x01 ; fmt=0
	IAU.SUB i21, i16, i18
	IAU.SHR.U32 i17, i17, 30
	IAU.SUB i18, i16, i17
	IAU.MUL i17, i18, i21
	IAU.MUL i20, i20, i17
	SAU.ADD.F32 i17, i10, i1 ; fmt=3
	BRU.NOP
	BRU.NOP
	BRU.BRA i20, LBB0_7
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_8
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_7:                                 //   in Loop: Header=BB0_2 Depth=1
	BRU.BRA LBB0_9
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_8:                                 //   in Loop: Header=BB0_2 Depth=1
	IAU.AND i17, i10, i10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_9:                                 //   in Loop: Header=BB0_2 Depth=1
	IAU.AND i10, i17, i12
	IAU.SHR.U32 i12, i10, 16
	IAU.MUL i20, i12, i9
	IAU.MUL i12, i12, i8
	IAU.SHR.U32 i21, i12, 16
	IAU.ADD i20, i20, i21
	IAU.AND i21, i17, i11
	IAU.MUL i9, i21, i9
	IAU.SHR.U32 i22, i9, 16
	IAU.ADD i20, i20, i22
	IAU.AND i9, i9, i11
	IAU.AND i11, i12, i11
	IAU.ADD i9, i11, i9
	IAU.MUL i8, i21, i8
	IAU.SHR.U32 i8, i8, 16
	IAU.ADD i8, i8, i9
	IAU.SHR.U32 i8, i8, 16
	IAU.ADD i8, i20, i8
	IAU.SHR.I32 i9, i17, i13
	IAU.OR i9, i9, i14
	IAU.XOR i9, i17, i9
	LSU0.LDIL i11, 0x01 ; fmt=0
	SAU.DIV.U32 i9, i15, i9
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i9
	IAU.SUB i10, i10, i8
	IAU.SHR.U32 i10, i10, 1
	IAU.ADD i8, i10, i8
	IAU.SHR.U32 i8, i8, 30
	IAU.SUB i8, i16, i8
	IAU.MUL i8, i18, i8
	IAU.MUL i8, i9, i8
	BRU.BRA i8, LBB0_10
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_11
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_10:                                //   in Loop: Header=BB0_2 Depth=1
	BRU.BRA LBB0_12
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_11:                                //   in Loop: Header=BB0_2 Depth=1
	IAU.AND i7, i17, i17
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_12:                                //   in Loop: Header=BB0_2 Depth=1
	LSU0.LDIL i8, 0x04 ; fmt=0
	IAU.ADD i8, i1, i8
	LSU0.LDIL i9, 0x40 ; fmt=0
	LSU0.LDIL i10, 0x01 ; fmt=0
	IAU.XOR i8, i8, i9
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

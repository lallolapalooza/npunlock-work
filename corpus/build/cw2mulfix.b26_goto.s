	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDO.32 i0, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i1, i0, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x0F ; fmt=0
	IAU.AND i1, i1, i2
	LSU0.LDO.32 i3, i0, 0x4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i3, i3, i2
	IAU.ADD i1, i3, i1
	LSU0.LDO.32 i3, i0, 0x8
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i3, i3, i2
	IAU.ADD i1, i3, i1
	LSU0.LDO.32 i3, i0, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i3, i3, i2
	IAU.ADD i1, i3, i1
	LSU0.LDO.32 i3, i0, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i3, i3, i2
	IAU.ADD i1, i3, i1
	LSU0.LDIL i3, 0x28 ; fmt=0
	IAU.ADD i4, i18, i3
	LSU0.LDIL i3, 0x29 ; fmt=0
	IAU.ADD i3, i18, i3
	LSU0.LDIL i5, 0x2A ; fmt=0
	IAU.ADD i6, i18, i5
	LSU0.LDIL i5, 0x2B ; fmt=0
	IAU.ADD i7, i18, i5
	LSU0.LDO.32 i5, i0, 0x14
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i5, i5, i2
	IAU.ADD i1, i5, i1
	LSU0.LDO.32 i5, i0, 0x18
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i5, i5, i2
	IAU.ADD i1, i5, i1
	LSU0.LDO.32 i0, i0, 0x1c
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i0, i0, i2
	IAU.ADD i5, i0, i1
	LSU0.LD.8 i0, i7
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i0, i0, 24
	IAU.SHR.U32 i0, i0, 24
	LSU0.LD.8 i1, i6
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i1, i1, 24
	IAU.SHR.U32 i1, i1, 24
	LSU0.LD.8 i2, i3
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i2, i2, 24
	IAU.SHR.U32 i3, i2, 24
	LSU0.LD.8 i2, i4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i2, i2, 24
	IAU.SHR.U32 i4, i2, 24
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.XOR i6, i5, i2
	CMU.CMZ.I32 i6
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
LBB0_1:                                 // %if.end4
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.AND i6, i5, i6
	LSU0.LDIL i7, 0x00 ; fmt=0
	LSU0.LDIL i8, 0x01 ; fmt=0
	IAU.XOR i6, i6, i7
	IAU.ADD i6, i6, i8
	SAU.DIV.U32 i7, i8, i6
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	CMU.CMZ.I32 i7
	LSU0.LDIL i6, 0xAAAA ; fmt=0
	LSU1.LDIH i6, 0xAAAA
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.XOR i5, i5, i6
	LSU0.LDIL i6, 0x05 ; fmt=0
	BRU.BRA i7, LBB0_2
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.BRA LBB0_3
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_2:
	IAU.ADD i5, i5, i6
	BRU.BRA LBB0_4
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_3:
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_4:
	LSU0.LDIL i6, 0x03 ; fmt=0
	IAU.MUL i5, i5, i6
	BRU.NOP
	BRU.NOP
LBB0_5:                                 // %done
	IAU.SHL i3, i3, 8
	IAU.OR i3, i3, i4
	IAU.SHL i1, i1, 16
	IAU.OR i1, i3, i1
	IAU.SHL i0, i0, 24
	IAU.OR i0, i1, i0
	LSU0.ST.32 i2, i0
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

	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDIL i2, 0x00 ; fmt=0
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i1, 0x20 ; fmt=0
	LSU0.LDO.32 i3, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i4, i3, i1
	IAU.AND i5, i2, i2
	IAU.AND i1, i2, i2
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB0_2:                                 // %for.body
                                        // =>This Inner Loop Header: Depth=1
	IAU.ADD i6, i3, i5
	LSU0.LD.8 i6, i6
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i6, i6, 24
	LSU0.LDIL i7, 0x18 ; fmt=0
	IAU.SHR.I32 i6, i6, i7
	IAU.ADD i1, i1, i6
	LSU0.LDIL i6, 0xFFF0 ; fmt=0
	LSU1.LDIH i6, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i6, i4, i6
	LSU0.LD.16 i6, i6
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i6, i6, 16
	IAU.SHR.U32 i6, i6, 16
	IAU.XOR i1, i1, i6
	LSU0.LD.16 i6, i4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.SHL i6, i6, 16
	IAU.SHR.U32 i6, i6, 16
	IAU.ADD i1, i1, i6
	IAU.ADD i6, i0, i5
	IAU.ADD i7, i2, i1
	LSU0.ST.8 i7, i6
	LSU0.LDIL i6, 0x02 ; fmt=0
	IAU.ADD i4, i4, i6
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i2, i2, i6
	IAU.ADD i5, i5, i6
	LSU0.LDIL i6, 0x10 ; fmt=0
	LSU0.LDIL i7, 0x01 ; fmt=0
	IAU.XOR i6, i5, i6
	CMU.CMZ.I32 i6
	BRU.BRA i6, LBB0_1
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
LBB0_1:                                 // %for.cond14.preheader
	LSU0.LDIL i2, 0x2E ; fmt=0
	IAU.ADD i2, i0, i2
	LSU0.LDIL i3, 0x07 ; fmt=0
	IAU.MUL i3, i1, i3
	LSU0.ST.16 i3, i2
	LSU0.LDIL i2, 0x2C ; fmt=0
	IAU.ADD i2, i0, i2
	LSU0.LDIL i3, 0x06 ; fmt=0
	IAU.MUL i3, i1, i3
	LSU0.ST.16 i3, i2
	LSU0.LDIL i2, 0x2A ; fmt=0
	IAU.ADD i2, i0, i2
	LSU0.LDIL i3, 0x05 ; fmt=0
	IAU.MUL i3, i1, i3
	LSU0.ST.16 i3, i2
	LSU0.LDIL i2, 0x28 ; fmt=0
	IAU.ADD i2, i0, i2
	IAU.SHL i3, i1, 2
	LSU0.ST.16 i3, i2
	LSU0.LDIL i2, 0x26 ; fmt=0
	IAU.ADD i2, i0, i2
	LSU0.LDIL i3, 0x03 ; fmt=0
	IAU.MUL i3, i1, i3
	LSU0.ST.16 i3, i2
	LSU0.LDIL i2, 0x24 ; fmt=0
	IAU.ADD i2, i0, i2
	IAU.SHL i3, i1, 1
	LSU0.ST.16 i3, i2
	LSU0.LDIL i2, 0x22 ; fmt=0
	IAU.ADD i2, i0, i2
	LSU0.ST.16 i1, i2
	LSU0.LDIL i1, 0x20 ; fmt=0
	IAU.ADD i0, i0, i1
	LSU0.LDIL i1, 0x00 ; fmt=0
	LSU0.ST.16 i1, i0
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

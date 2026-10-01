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
	LSU0.LDO.32 i1, i0, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i2, 0x05 ; fmt=0
	IAU.ADD i2, i1, i2
	LSU0.LDO.32 i3, i0, 0x14
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i4, 0x06 ; fmt=0
	IAU.ADD i4, i3, i4
	LSU0.LDO.32 i5, i0, 0x8
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i6, 0x03 ; fmt=0
	IAU.ADD i6, i5, i6
	LSU0.LDO.32 i7, i0, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i8, 0x01 ; fmt=0
	IAU.ADD i8, i7, i8
	LSU0.LDO.32 i9, i0, 0x4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i10, 0x02 ; fmt=0
	IAU.ADD i10, i9, i10
	IAU.XOR i11, i10, i8
	LSU0.LDO.32 i12, i0, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i13, 0x04 ; fmt=0
	IAU.ADD i14, i12, i13
	IAU.XOR i15, i14, i6
	IAU.ADD i16, i15, i11
	IAU.XOR i17, i4, i2
	IAU.ADD i16, i16, i17
	LSU0.LDO.32 i19, i0, 0x18
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i20, 0x07 ; fmt=0
	IAU.ADD i20, i19, i20
	LSU0.LDO.32 i0, i0, 0x1c
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i21, 0x08 ; fmt=0
	IAU.ADD i22, i0, i21
	IAU.XOR i23, i22, i20
	IAU.ADD i16, i16, i23
	LSU0.LDO.32 i24, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i21, i24, i21
	LSU0.ST.32 i16, i21
	IAU.MUL i11, i15, i11
	IAU.MUL i15, i23, i17
	IAU.XOR i16, i15, i11
	IAU.ADD i17, i15, i11
	IAU.ADD i16, i17, i16
	IAU.MUL i16, i16, i17
	IAU.SHL i11, i11, 2
	IAU.SUB i11, i11, i15
	IAU.ADD i11, i11, i16
	LSU0.LDIL i15, 0x0C ; fmt=0
	IAU.ADD i15, i24, i15
	LSU0.ST.32 i11, i15
	IAU.ADD i8, i10, i8
	IAU.ADD i7, i9, i7
	IAU.ADD i6, i8, i6
	IAU.ADD i5, i7, i5
	IAU.ADD i5, i5, i12
	IAU.ADD i1, i5, i1
	IAU.ADD i1, i1, i3
	IAU.ADD i1, i1, i19
	IAU.ADD i0, i1, i0
	LSU0.ST.32 i0, i24
	IAU.ADD i0, i6, i14
	IAU.ADD i0, i0, i2
	IAU.ADD i0, i0, i4
	IAU.ADD i1, i24, i13
	IAU.ADD i0, i0, i20
	IAU.ADD i0, i0, i22
	LSU0.ST.32 i0, i1
	BRU.JMP i30
Lfunc_end0:
	.size	controlled_act, Lfunc_end0-controlled_act
                                        // -- End function

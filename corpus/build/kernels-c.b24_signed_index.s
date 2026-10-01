	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDIL i0, 0x04 ; fmt=0
	LSU0.LDO.32 i1, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i1, i0
	LSU0.LDO.32 i2, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i3, i2, 0x5c
	LSU0.LDO.32 i4, i2, 0x58
	LSU0.LDO.32 i5, i2, 0x54
	LSU0.LDO.32 i6, i2, 0x50
	LSU0.LDO.32 i7, i2, 0x4c
	LSU0.LDO.32 i8, i2, 0x48
	LSU0.LDO.32 i9, i2, 0x44
	LSU0.LDO.32 i10, i2, 0x40
	LSU0.LDO.32 i11, i2, 0x3c
	LSU0.LDO.32 i12, i2, 0x38
	LSU0.LDO.32 i13, i2, 0x34
	LSU0.LDO.32 i14, i2, 0x30
	LSU0.LDO.32 i15, i2, 0x2c
	LSU0.LDO.32 i16, i2, 0x28
	BRU.NOP
	LSU0.LDO.32 i17, i2, 0x24
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i2, i2, 0x20
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i19, 0x50 ; fmt=0
	LSU0.ST.32 i19, i0
	IAU.ADD i0, i17, i2
	IAU.ADD i0, i16, i0
	IAU.ADD i0, i15, i0
	IAU.ADD i0, i14, i0
	IAU.ADD i0, i13, i0
	IAU.ADD i0, i12, i0
	IAU.ADD i0, i11, i0
	IAU.ADD i0, i10, i0
	IAU.ADD i0, i9, i0
	IAU.ADD i0, i8, i0
	IAU.ADD i0, i7, i0
	IAU.ADD i0, i6, i0
	IAU.ADD i0, i5, i0
	IAU.ADD i0, i4, i0
	IAU.ADD i0, i3, i0
	IAU.XOR i0, i6, i0
	IAU.XOR i0, i7, i0
	IAU.XOR i0, i8, i0
	IAU.XOR i0, i9, i0
	IAU.XOR i0, i10, i0
	IAU.XOR i0, i11, i0
	IAU.XOR i0, i12, i0
	IAU.XOR i0, i13, i0
	LSU0.ST.32 i0, i1
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

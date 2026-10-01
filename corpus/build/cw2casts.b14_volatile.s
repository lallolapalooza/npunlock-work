	.text
	.file	"entry.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i1, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i2, i1, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.ST.32 i2, i0
	LSU0.LDO.32 i3, i1, 0x4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i3, i2
	LSU0.LDIL i3, 0x04 ; fmt=0
	IAU.ADD i3, i0, i3
	LSU0.ST.32 i2, i3
	LSU0.LDIL i3, 0x20 ; fmt=0
	IAU.ADD i3, i0, i3
	LSU0.LDIL i4, 0x1C ; fmt=0
	IAU.ADD i4, i0, i4
	LSU0.LDIL i5, 0x18 ; fmt=0
	IAU.ADD i5, i0, i5
	LSU0.LDIL i6, 0x14 ; fmt=0
	IAU.ADD i6, i0, i6
	LSU0.LDIL i7, 0x10 ; fmt=0
	IAU.ADD i7, i0, i7
	LSU0.LDIL i8, 0x0C ; fmt=0
	IAU.ADD i8, i0, i8
	LSU0.LDIL i9, 0x08 ; fmt=0
	IAU.ADD i0, i0, i9
	LSU0.LDO.32 i9, i1, 0x8
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i9, i2
	LSU0.ST.32 i2, i0
	LSU0.LDO.32 i0, i1, 0xc
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i0, i2
	LSU0.ST.32 i0, i8
	LSU0.LDO.32 i2, i1, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i2, i0
	LSU0.ST.32 i0, i7
	LSU0.LDO.32 i2, i1, 0x14
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i2, i0
	LSU0.ST.32 i0, i6
	LSU0.LDO.32 i2, i1, 0x18
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i2, i0
	LSU0.ST.32 i0, i5
	LSU0.LDO.32 i1, i1, 0x1c
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i0, i1, i0
	LSU0.ST.32 i0, i4
	LSU0.ST.32 i0, i3
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

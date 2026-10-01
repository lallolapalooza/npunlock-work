	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i21, i18, 0x28
	LSU0.LDO.32 i16, i18, 0x28
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	.byte 0x82 0x42 0x11 0x11
	.byte 0x82 0x43 0x00 0x00
	.byte 0x82 0xca 0x04 0x00
	.byte 0x82 0xcb 0x00 0x00
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	.byte 0x49 0x80 0x20 0x59 0x14
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	LSU1.ST.32 i8, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i9, i9, i9
	LSU1.ST.32 i9, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i9, i9, i9
	LSU1.ST.32 i0, i21
	IAU.ADD i21, i21, i25
	BRU.JMP i30

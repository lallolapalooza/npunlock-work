	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i21, i18, 0x28
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	.byte 0x82 0xca 0x04 0x00
	.byte 0x82 0xcb 0x00 0x00
	.byte 0x82 0x0a 0x11 0x11
	.byte 0x82 0x0b 0x00 0x00
	.byte 0x82 0x4a 0x22 0x22
	.byte 0x82 0x4b 0x00 0x00
	.byte 0x82 0x82 0x33 0x33
	.byte 0x82 0x83 0x00 0x00
	.byte 0x82 0xa2 0x44 0x44
	.byte 0x82 0xa3 0x00 0x00
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	.byte 0xc1 0x39 0x24 0x48 0x49
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	LSU1.ST.32 i20, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i9, i9, i9
	LSU1.ST.32 i9, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i9, i9, i9
	LSU1.ST.32 i1, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i9, i9, i9
	LSU1.ST.32 i16, i21
	IAU.ADD i21, i21, i25
	BRU.JMP i30

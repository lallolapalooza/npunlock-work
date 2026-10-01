	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i21, i18, 0x28
	LSU0.LDO.32 i2, i18, 0x28
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	.byte 0x82 0x0a 0x11 0x11
	.byte 0x82 0x0b 0x00 0x00
	.byte 0x82 0x1a 0x02 0x00
	.byte 0x82 0x1b 0x00 0x00
	.byte 0x82 0xca 0x04 0x00
	.byte 0x82 0xcb 0x00 0x00
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	.byte 0x80 0x11 0x09 0x00
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	LSU1.ST.32 i1, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i10, i10, i10
	LSU1.ST.32 i2, i21
	BRU.JMP i30

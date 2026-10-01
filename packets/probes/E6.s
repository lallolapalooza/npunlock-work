	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i21, i18, 0x28
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	.byte 0x82 0x02 0x11 0x11
	.byte 0x82 0x03 0x00 0x00
	.byte 0x82 0x42 0x22 0x22
	.byte 0x82 0x43 0x00 0x00
	.byte 0x82 0xb2 0x33 0x33
	.byte 0x82 0xb3 0x00 0x00
	.byte 0x82 0xca 0x04 0x00
	.byte 0x82 0xcb 0x00 0x00
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	.byte 0x33 0x00 0x01 0x1b
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	LSU1.ST.32 i0, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i10, i10, i10
	LSU1.ST.32 i8, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i10, i10, i10
	LSU1.ST.32 i22, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i10, i10, i10
	BRU.JMP i30

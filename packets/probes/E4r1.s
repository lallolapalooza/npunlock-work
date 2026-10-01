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
	.byte 0x82 0xca 0x04 0x00
	.byte 0x82 0xcb 0x00 0x00
	.byte 0x82 0xb2 0x01 0x55
	.byte 0x82 0xb3 0x00 0x00
	.byte 0x82 0xba 0x02 0x55
	.byte 0x82 0xbb 0x00 0x00
	.byte 0x82 0xc2 0x03 0x55
	.byte 0x82 0xc3 0x00 0x00
	.byte 0x82 0xea 0x04 0x55
	.byte 0x82 0xeb 0x00 0x00
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	.byte 0xc1 0x39 0x24 0x48 0x49
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	IAU.ADD i10, i10, i10
	LSU1.ST.32 i22, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i10, i10, i10
	LSU1.ST.32 i23, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i10, i10, i10
	LSU1.ST.32 i24, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i10, i10, i10
	LSU1.ST.32 i29, i21
	IAU.ADD i21, i21, i25
	IAU.ADD i10, i10, i10
	BRU.JMP i30

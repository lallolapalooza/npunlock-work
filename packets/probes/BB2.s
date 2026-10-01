	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i20, i18, 0x28
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
	LSU1.ST.32 i20, i20
	BRU.JMP i30

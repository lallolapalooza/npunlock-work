	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i1, i18, 0x28
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	.byte 0xa5 0xcd 0xab
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	IAU.ADD i9, i9, i9
	LSU1.ST.32 i20, i1
	BRU.JMP i30

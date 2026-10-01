	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i0, i18, 0x28
	.byte 0x50
	IAU.ADD i4, i0, 4
	.byte 0x50
	LSU1.ST.32 i4, i0
	BRU.JMP i30

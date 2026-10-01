	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i0, i18, 0x28
	.byte 0x50
	.byte 0x82 0x4a 0x00 0x00
	.byte 0x82 0x4b 0x34 0x12
	.byte 0x50
	.byte 0x50
	IAU.ADD i9, i9, 2
	.byte 0x50
	LSU1.ST.32 i9, i0
	BRU.JMP i30

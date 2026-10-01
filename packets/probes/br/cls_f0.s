	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i0, i18, 0x28
	.byte 0x50
	.byte 0xf0 0x82 0x64 0x51 0x00
	.byte 0x50
	LSU1.ST.32 i8, i0
	BRU.JMP i30

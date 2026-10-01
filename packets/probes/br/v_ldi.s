	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i0, i18, 0x28
	.byte 0x50
	.byte 0x82 0x02 0xef 0xbe
	.byte 0x82 0x03 0xad 0xde
	.byte 0x50
	.byte 0x50
	LSU1.ST.32 i1, i0
	BRU.JMP i30

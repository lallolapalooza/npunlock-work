	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDIL i17, sin
	LSU0.LDIH i17, sin
	BRU.JMP i30
	.size	controlled_act, .-controlled_act

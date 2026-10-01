	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDIL i17, rintl
	LSU0.LDIH i17, rintl
	BRU.JMP i30
	.size	controlled_act, .-controlled_act

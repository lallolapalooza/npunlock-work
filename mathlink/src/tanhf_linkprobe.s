	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDIL i18, tanhf
	LSU0.LDIH i18, tanhf
	BRU.JMP i18
	.size	controlled_act, .-controlled_act

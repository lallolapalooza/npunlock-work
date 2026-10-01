	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i0, i18, 0x28
	.byte 0x50
	IAU.ADD i3, i0, 2
	.byte 0x50
	CMU.CMII.U32 i0, i0
	BRU.BRA i0, TGT
	.byte 0xeb 0x30
	IAU.ADD i9, i9, i9
	LSU1.ST.32 i9, i0
	BRU.JMP i30
TGT:
	LSU1.ST.32 i0, i3
	BRU.JMP i30

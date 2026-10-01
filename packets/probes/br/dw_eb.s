	.text
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i0, i18, 0x28
	.byte 0x50
	.byte 0x82 0x0a 0x01 0x00
	.byte 0x82 0x0b 0x00 0x00
	.byte 0x50
	.byte 0x50
	CMU.CMII.U32 i0, i0
	BRU.BRA i0, TGT
	.byte 0xeb
	IAU.ADD i1, i1, i1
	LSU1.ST.32 i1, i0
	BRU.JMP i30
TGT:
	LSU1.ST.32 i1, i0
	BRU.JMP i30

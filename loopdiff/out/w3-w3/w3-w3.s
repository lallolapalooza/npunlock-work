	.text
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDIL i1, 0xBABE ; fmt=0
	LSU1.LDIH i1, 0xCAFE
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU1.ST.32 i1, i0
	LSU0.LDIL i3, 0x04 ; fmt=0
	IAU.ADD i0, i0, i3
	LSU1.ST.32 i1, i0

	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end0:
	.size	controlled_act, Lfunc_end0-controlled_act

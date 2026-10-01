	.text
	.globl	controlled_act
	.type	controlled_act,@function
controlled_act:
	LSU0.LDO.32 i13, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i1, 0x3344 ; fmt=0
	LSU1.LDIH i1, 0x1122
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i1, i18, 0x10
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU1.LDIL i2, 0x00 ; fmt=0
	LSU1.LDIL i3, 0x01 ; fmt=0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU1.ST.32 i1, i13   // witness i1 (seed 0x11223344)
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end0:
	.size	controlled_act, Lfunc_end0-controlled_act

	.text
	.file	"mulh.c"
	.section	.text.controlled_act,"ax",@progbits
	.globl	controlled_act                  // -- Begin function controlled_act
	.type	controlled_act,@function
controlled_act:                         // @controlled_act
.Lcontrolled_act$local:
// %bb.0:                               // %entry
	LSU0.LDO.32 i0, i18, 0x28
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i1, 0x1C ; fmt=0
	IAU.ADD i1, i0, i1
	LSU0.LDO.32 i2, i18, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i3, i2, 0x0
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDO.32 i2, i2, 0x4
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU0.ST.32 i4, i1
	LSU0.LDIL i1, 0x18 ; fmt=0
	IAU.ADD i1, i0, i1
	LSU0.ST.32 i4, i1
	LSU0.LDIL i1, 0x04 ; fmt=0
	IAU.ADD i1, i0, i1
	IAU.LMULH i4, i2, i3
	LSU0.ST.32 i4, i1
	LSU0.LDIL i1, 0x14 ; fmt=0
	IAU.ADD i1, i0, i1
	LSU0.ST.32 i2, i1
	LSU0.LDIL i1, 0x10 ; fmt=0
	IAU.ADD i1, i0, i1
	LSU0.ST.32 i3, i1
	LSU0.LDIL i1, 0x08 ; fmt=0
	IAU.ADD i1, i0, i1
	IAU.MUL i2, i2, i3
	LSU0.ST.32 i2, i1
	LSU0.ST.32 i2, i0
	LSU0.LDIL i1, 0x0C ; fmt=0
	IAU.ADD i0, i0, i1
	LSU0.LDIL i1, 0x01 ; fmt=0
	IAU.OR i1, i3, i1
	LSU0.LDIL i2, 0xFFFF ; fmt=0
	LSU1.LDIH i2, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	SAU.DIV.U32 i1, i2, i1
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
	BRU.NOP
	LSU0.ST.32 i1, i0
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end0:
	.size	controlled_act, Lfunc_end0-controlled_act
                                        // -- End function

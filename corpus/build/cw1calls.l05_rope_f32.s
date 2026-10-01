	.text
	.file	"entry.c"
	.section	.text.strtof,"ax",@progbits
	.globl	strtof                          // -- Begin function strtof
	.type	strtof,@function
strtof:                                 // @strtof
.Lstrtof$local:
// %bb.0:                               // %entry
	LSU0.LDIL i18, 0x00 ; fmt=0
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end0:
	.size	strtof, Lfunc_end0-strtof
                                        // -- End function
	.section	.text.__truncdfsf2,"ax",@progbits
	.globl	__truncdfsf2                    // -- Begin function __truncdfsf2
	.type	__truncdfsf2,@function
__truncdfsf2:                           // @__truncdfsf2
.L__truncdfsf2$local:
// %bb.0:                               // %entry
	LSU0.LDIL i18, 0x00 ; fmt=0
	BRU.JMP i30
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
Lfunc_end1:
	.size	__truncdfsf2, Lfunc_end1-__truncdfsf2
                                        // -- End function
	.section	.text.__fixsfdi,"ax",@progbits
	.globl	__fixsfdi                       // -- Begin function __fixsfdi
	.type	__fixsfdi,@function
__fixsfdi:                              // @__fixsfdi
.L__fixsfdi$local:
// %bb.0:                               // %entry
	LSU0.LDIL i0, 0xFF ; fmt=0
	IAU.SHR.U32 i1, i18, 23
	IAU.AND i2, i1, i0
	LSU0.LDIL i0, 0x00 ; fmt=0
	LSU0.LDIL i1, 0x7F ; fmt=0
	LSU0.LDIL i3, 0x01 ; fmt=0
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
	CMU.CMZ.I32 i1
	BRU.BRA i1, LBB2_10
	.byte 0x1b, 0x40
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
LBB2_1:                                 // %if.end
	LSU0.LDIL i1, 0xFFFF ; fmt=0
	LSU1.LDIH i1, 0x7F
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.AND i1, i18, i1
	LSU0.LDIL i3, 0x00 ; fmt=0
	LSU1.LDIH i3, 0x80
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.OR i1, i1, i3
	LSU0.LDIL i3, 0x96 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.ADD i5, i2, i4
	IAU.ADD i3, i3, i4
	SAU.DIV.U32 i3, i3, i5
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
	CMU.CMZ.I32 i3
	BRU.BRA i3, LBB2_3
	.byte 0x1b, 0x40
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
LBB2_2:                                 // %cond.true
	LSU0.LDIL i3, 0x96 ; fmt=0
	IAU.SUB i2, i3, i2
	LSU0.LDIL i3, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i4, i2, i3
	CMU.CMZ.I32 i4
	LSU0.LDIL i4, 0x20 ; fmt=0
	LSU0.LDIL i5, 0x1F ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i6, i2, i6
	SAU.DIV.U32 i4, i4, i6
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
	CMU.CMZ.I32 i4
	IAU.SUB i3, i3, i4
	IAU.AND i2, i2, i5
	IAU.SHR.U32 i1, i1, i2
	IAU.AND i3, i1, i3
	BRU.BRA LBB2_4
	.byte 0x1b, 0x40
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
LBB2_3:                                 // %cond.false
	LSU0.LDIL i3, 0xB6 ; fmt=0
	LSU0.LDIL i3, 0xFF6A ; fmt=0
	LSU1.LDIH i3, 0xFFFF
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.ADD i2, i2, i3
	LSU0.LDIL i3, 0x00 ; fmt=0
	LSU0.LDIL i4, 0x01 ; fmt=0
	IAU.XOR i4, i2, i3
	CMU.CMZ.I32 i4
	LSU0.LDIL i4, 0x1F ; fmt=0
	LSU0.LDIL i5, 0x20 ; fmt=0
	LSU0.LDIL i6, 0x01 ; fmt=0
	IAU.ADD i6, i2, i6
	SAU.DIV.U32 i5, i5, i6
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
	CMU.CMZ.I32 i5
	IAU.SUB i3, i3, i5
	LSU0.LDIL i5, 0x01 ; fmt=0
	IAU.ADD i5, i4, i5
	SAU.DIV.U32 i5, i2, i5
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
	CMU.CMZ.I32 i5
	IAU.AND i2, i2, i4
	IAU.SHL i1, i1, i2
	IAU.AND i3, i1, i3
LBB2_4:                                 // %cond.end
	LSU0.LDIL i1, 0x00 ; fmt=0
	LSU0.LDIL i2, 0x01 ; fmt=0
	LSU0.LDIL i4, 0x00 ; fmt=0
	LSU1.LDIH i4, 0x8000
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	BRU.NOP
	IAU.XOR i5, i18, i4
	IAU.XOR 
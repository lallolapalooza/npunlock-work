	.text
	.file	"entry.c"
	.section	.text.not_the_entry,"ax",@progbits
	.globl	not_the_entry                   // -- Begin function not_the_entry
	.type	not_the_entry,@function
not_the_entry:                          // @not_the_entry
.Lnot_the_entry$local:
// %bb.0:                               // %entry
	BRU.JMP i30
Lfunc_end0:
	.size	not_the_entry, Lfunc_end0-not_the_entry
                                        // -- End function

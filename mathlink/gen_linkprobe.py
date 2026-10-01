#!/usr/bin/env python3
"""Generate a linker-probe .s that roots one .text.<fn> per libm symbol.

The probe is NOT a functional kernel: it only materialises each symbol's absolute
address (LDIL/LDIH -> R_SHAVE_LO16/HI16) so --gc-sections keeps that function's
section and the linker must resolve it out of mlibm.a.  Used to measure which
symbols our toolchain can resolve and to exercise the vendor relocations at scale.
"""
import sys

HEADER = """\t.text
\t.section\t.text.controlled_act,"ax",@progbits
\t.globl\tcontrolled_act
\t.type\tcontrolled_act,@function
controlled_act:
"""

def emit(symbols, engine=None):
    lines = [HEADER]
    for s in symbols:
        lines.append(f"\tLSU0.LDIL i17, {s}")
        lines.append(f"\tLSU0.LDIH i17, {s}")
    lines.append("\tBRU.JMP i30")
    lines.append("\t.size\tcontrolled_act, .-controlled_act")
    return "".join(l + "\n" for l in lines)

if __name__ == "__main__":
    syms = [l.strip() for l in sys.stdin if l.strip() and not l.startswith("#")]
    sys.stdout.write(emit(syms))

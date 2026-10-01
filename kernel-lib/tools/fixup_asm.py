#!/usr/bin/env python3
"""kernel-lib asm fixup: force the register form of the fp32 ALU instructions.

The SHAVE compiler selects the *constant-selector* encoding of these
instructions when they carry register operands, so `a + b` executes as
`a + <hardware constant selected by the b operand index>` (measured: x + 2.0f
becomes x + 1.41494, x + x becomes x + 2.0).  The `; fmt=2` hint selects the
register form.

This is the same fixup ggml-shave applies in
llama.cpp/ggml/src/ggml-shave/tools/build_kernels.py; it must stay identical in
intent until llvm/lib/Target/SHAVE emits the register form itself.  Every
kernel in this library therefore keeps operand values in registers (never a
float literal in the arithmetic expression) so the rewrite is exact.

usage: fixup_asm.py < in.s > out.s
"""

import re
import sys

FIXUP = (
    "SAU.ADD.F32",
    "VAU.ADD.F32",
    "SAU.SUB.F32",
    "VAU.SUB.F32",
    "SAU.MUL.F32",
    "VAU.MUL.F32",
)


def fixup(text: str) -> str:
    out = []
    for line in text.splitlines():
        match = re.match(r"\s*([A-Z0-9.]+)\b(.*)$", line)
        if match and match.group(1) in FIXUP:
            body = re.sub(r";.*$", "", match.group(2)).rstrip()
            line = f"\t{match.group(1)}{body} ; fmt=2"
        out.append(line)
    return "\n".join(out) + "\n"


if __name__ == "__main__":
    sys.stdout.write(fixup(sys.stdin.read()))

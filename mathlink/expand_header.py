#!/usr/bin/env python3
"""Reproduce shavecc's expand_npu3720_kernel_include (src/shavecc/shavecc.c) so the
individual MOVI stages can be driven by hand for diagnostics."""
import pathlib, re, sys
REPO = pathlib.Path("/home/asdf/projects/npunlock-work/npunlock")
HEADER = (REPO / "include/npunlock/npu3720_kernel.h").read_bytes()
DIRECTIVE = "#include <npunlock/npu3720_kernel.h>"
FEATURE = "#define MLIBM_DEFINE_LINK_COMPAT 1"

def expand(source: bytes) -> bytes:
    off = 0
    lines = 1
    def skip_ws(off, lines):
        while off < len(source) and source[off:off+1] in b" \t\r\n":
            if source[off:off+1] == b"\n": lines += 1
            off += 1
        return off, lines
    off, lines = skip_ws(off, lines)
    if source[off:off+len(FEATURE)] == FEATURE.encode():
        off += len(FEATURE)
        off, lines = skip_ws(off, lines)
    include_start = off
    if source[off:off+len(DIRECTIVE)] != DIRECTIVE.encode():
        return source
    off += len(DIRECTIVE)
    off, lines = skip_ws(off, lines)
    marker = f"\n#line {lines} \"entry.c\"\n".encode()
    return source[:include_start] + HEADER + marker + source[off:]

if __name__ == "__main__":
    sys.stdout.buffer.write(expand(pathlib.Path(sys.argv[1]).read_bytes()))

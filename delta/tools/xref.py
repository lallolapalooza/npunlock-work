#!/usr/bin/env python3
"""Given a string literal, find where it lives in the vendor DLL, then find
8-byte little-endian pointers to it (the `s_<text>_<ptrVA>` label Ghidra uses),
and print those pointer VAs ready for grepping decompiled_all.c.

Usage: xref.py <exe> <needle> [--max N]
"""
import sys, struct
sys.path.insert(0, '/home/asdf/projects/npunlock-work/delta/tools')
from strva import pe_sections, va_from_off

def main():
    exe = sys.argv[1]
    needle = sys.argv[2].encode()
    data, imagebase, secs = pe_sections(exe)
    # 1. find string occurrences
    va_list = []
    start = 0
    while True:
        i = data.find(needle, start)
        if i < 0: break
        va, _ = va_from_off(secs, imagebase, i)
        if va: va_list.append(va)
        start = i + 1
    # 2. also allow the string to be a prefix (option desc may be one literal)
    for va in va_list:
        end = data.find(b'\0', va - imagebase + pe_sections(exe)[2][0][3] if False else 0)
    ptrs = []
    for va in va_list:
        pat = struct.pack('<Q', va)
        s = 0
        while True:
            j = data.find(pat, s)
            if j < 0: break
            pva, sec = va_from_off(secs, imagebase, j)
            ptrs.append((va, pva, sec))
            s = j + 1
    for va, pva, sec in ptrs:
        print('str=%x ptr=%x sec=%s' % (va, pva, sec))
    if not ptrs:
        print('no ptrs; str VA(s): ' + ','.join('%x' % v for v in va_list))

main()

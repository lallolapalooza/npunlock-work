#!/usr/bin/env python3
"""Interval-DP validation of encodings.json against mlibm objects.

Uses DWARF statement starts, branch-target addends and type-209
instruction-start bounds (instr_start = r_offset - old) as hard boundaries.
Packing variants: field order (lsb/msb) x byte order (le/be).
"""
import json, struct, subprocess, sys, pickle
from collections import Counter

DB = json.load(open('/home/asdf/projects/npunlock-work/shave-isa/encodings.json'))
INSNS = DB['instructions']


def runs_of(ins):
    m, f, width = ins['mask'], ins['fixed_bits'], ins['length_bits']
    runs = []
    p = 0
    mm = m
    while mm:
        if mm & 1:
            s = p
            while mm & 1:
                mm >>= 1
                p += 1
            runs.append((s, p - s, (f >> s) & ((1 << (p - s)) - 1)))
        else:
            mm >>= 1
            p += 1
    return runs, width


def enc(runs, width, variant):
    n = (width + 7) // 8
    val = 0
    for s, w, v in runs:
        pos = (width - s - w) if variant.startswith('msb') else s
        val |= v << pos
    order = 'little' if variant.endswith('le') else 'big'
    bs = val.to_bytes(n, order)
    mb = bytearray(n)
    for s, w, v in runs:
        pos = (width - s - w) if variant.startswith('msb') else s
        for k in range(w):
            bit = pos + k
            if variant.endswith('le'):
                bi, bj = bit // 8, bit % 8
            else:
                bi, bj = n - 1 - bit // 8, 7 - bit % 8
            mb[bi] |= 1 << bj
    return bs, bytes(mb)


def parse_elf32(path):
    data = open(path, 'rb').read()
    e_shoff = struct.unpack_from('<I', data, 0x20)[0]
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', data, 0x2e)
    def shdr(i):
        return struct.unpack_from('<IIIIIIIIII', data, e_shoff + i * e_shentsize)
    shstr = shdr(e_shstrndx)
    shstrtab = data[shstr[4]:shstr[4] + shstr[5]]
    secs = {}
    for i in range(e_shnum):
        s = shdr(i)
        nm = shstrtab[s[0]:shstrtab.index(b'\0', s[0])].decode()
        secs[nm] = dict(type=s[1], off=s[4], size=s[5])
    relas = {}
    for nm, s in secs.items():
        if s['type'] == 4:
            entries = []
            for i in range(s['size'] // 12):
                o = s['off'] + i * 12
                r_off, r_info, r_add = struct.unpack_from('<IIi', data, o)
                entries.append((r_off, r_info & 0xff, r_info >> 8, r_add))
            relas[nm] = entries
    return data, secs, relas


def dwarf_stmts(path):
    out = subprocess.run(['readelf', '--debug-dump=decodedline', path],
                         capture_output=True, text=True).stdout
    stmts = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0].endswith(('.c', '.cpp', '.h')):
            try:
                addr = int(parts[2], 16) if parts[2].startswith('0x') else int(parts[2])
            except ValueError:
                continue
            stmts.add(addr)
    return stmts


def validate(path, variant, verbose=0):
    data, secs, relas = parse_elf32(path)
    tsec = [n for n in secs if n.startswith('.text')][0]
    text = data[secs[tsec]['off']:secs[tsec]['off'] + secs[tsec]['size']]
    stmts = dwarf_stmts(path)
    our = relas.get('.rela' + tsec, [])
    bylen = {}
    for ins in INSNS:
        r, w = runs_of(ins)
        bs, msk = enc(r, w, variant)
        bylen.setdefault(ins['length_bytes'], []).append((msk, bs, ins))
    def match(pos):
        out = []
        for L, lst in bylen.items():
            if pos + L > len(text):
                continue
            chunk = text[pos:pos + L]
            for msk, bs, ins in lst:
                if all(((chunk[i] ^ bs[i]) & msk[i]) == 0 for i in range(L)):
                    out.append((ins, int.from_bytes(chunk, 'little')))
        return out
    bounds = set([0, len(text)]) | set(stmts)
    n209 = 0
    for o, t, si, add in our:
        if t == 209:
            old = int.from_bytes(text[o:o + 2], 'little')
            bounds.add(o - old)
            n209 += 1
        if t in (209, 210):
            bounds.add(add)
    bounds = sorted(b for b in bounds if 0 <= b <= len(text))
    solved = 0
    failed = []
    alldec = []
    ok209 = 0
    for a, b in zip(bounds, bounds[1:]):
        N = b - a
        dp = [None] * (N + 1)
        dp[0] = (0, None, None)
        for p in range(N):
            if dp[p] is None:
                continue
            for ins, v in match(a + p):
                q = p + ins['length_bytes']
                if q > N:
                    continue
                sc = dp[p][0] + ins['mask'].bit_count()
                if dp[q] is None or sc > dp[q][0]:
                    dp[q] = (sc, p, (ins, v))
        if dp[N] is None:
            failed.append((a, b))
        else:
            solved += 1
            seq = []
            p = N
            while p > 0:
                seq.append((a + dp[p][1], dp[p][2][0], dp[p][2][1]))
                p = dp[p][1]
            seq.reverse()
            alldec.extend(seq)
    bstarts = set(p for p, _, _ in alldec)
    for o, t, si, add in our:
        if t == 209:
            old = int.from_bytes(text[o:o + 2], 'little')
            if (o - old) in bstarts:
                ok209 += 1
    cov = sum(i['length_bytes'] for _, i, _ in alldec)
    print(f"{variant}: intervals {solved}/{len(bounds)-1} failed={len(failed)} "
          f"bytes={cov}/{len(text)} ({100*cov/len(text):.1f}%) "
          f"209-starts={ok209}/{n209}")
    return solved, alldec, text, failed


if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else '/tmp/mlibm_x/ef_sqrt.o'
    variants = ['lsb_le', 'lsb_be', 'msb_le', 'msb_be']
    best = None
    for v in variants:
        s, dec, text, failed = validate(path, v)
        if best is None or s > best[0]:
            best = (s, v, dec, text, failed)
    print("BEST:", best[1])
    s, v, dec, text, failed = best
    if len(sys.argv) > 2 or True:
        for p, i, val in dec[:50]:
            print(f"  {p:#06x}: {text[p:p+i['length_bytes']].hex():<12} {i['mnemonic']}")
        pickle.dump([(p, i['mnemonic'], i['format'], val) for p, i, val in dec],
                    open('/home/asdf/projects/npunlock-work/shave-isa/efsqrt_decode.pkl', 'wb'))

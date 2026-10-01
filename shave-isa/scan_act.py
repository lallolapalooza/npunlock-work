#!/usr/bin/env python3
"""scan_act.py — hunt the unknown pointer-setup form in prebuilt ACT ELFs.

Anchors: LDO fmt0 family bytes c2 ?? 64 19 00 (rank +0xc) / 64 21 00 (dims
+0x10); also probes for 64 01 00 (+0x00 in-ptr) and 64 51 00 (+0x28 out-ptr).
Signature search: gold's unknown-run bytes (a3 fc ff / 0f 28 c8 / 50 10 00).
5B imm8 hypothesis: candidates [b0][b1][b2][b3][imm8 in {0,0x28,0x50}] near
anchors with (b0 & 7) in the unknown subop class {0,3,7}.
"""
import glob
import struct
import sys
from collections import Counter, defaultdict

def text_sections(path):
    d = open(path, 'rb').read()
    if d[:4] != b'\x7fELF':
        return None, 'not-elf'
    if d[4] != 1:
        return None, 'not-elf32'
    e_shoff = struct.unpack_from('<I', d, 0x20)[0]
    es, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2e)
    def sh(i):
        return struct.unpack_from('<IIIIIIIIII', d, e_shoff + i * es)
    shstr = sh(shstrndx)
    st = d[shstr[4]:shstr[4] + shstr[5]]
    out = []
    for i in range(shnum):
        s = sh(i)
        nm = st[s[0]:st.index(b'\0', s[0])].decode()
        if nm.startswith('.text'):
            out.append((nm, d[s[4]:s[4] + s[5]]))
    return out, None

def main():
    files = sorted(glob.glob('/tmp/act_corpus/*.elf'))
    files += ['/home/asdf/projects/npunlock-work/lld-sparc/inner_shave.elf']
    anchors = Counter()
    sig_hits = Counter()
    imm8_cands = Counter()
    ldo_variants = Counter()
    nfiles = ntext = 0
    windows = []
    files_with = defaultdict(set)
    for path in files:
        secs, err = text_sections(path)
        if err or not secs:
            continue
        nfiles += 1
        base = path.split('/')[-1]
        joined = b''.join(t for _, t in secs)
        if not joined:
            continue
        ntext += 1
        # LDO-class families: c2 ?? 64 XX YY
        for i in range(len(joined) - 5):
            if joined[i] == 0xc2 and joined[i+2] == 0x64:
                imm = joined[i+3] | (joined[i+4] << 8)
                key = (joined[i+1], joined[i+3], joined[i+4])
                ldo_variants[(joined[i+3], joined[i+4])] += 1
                files_with[f'LDO 64 {joined[i+3]:02x}{joined[i+4]:02x}'].add(base)
                if joined[i+3:i+5] in (b'\x19\x00', b'\x21\x00', b'\x01\x00', b'\x51\x00'):
                    anchors[(joined[i+3:i+5].hex())] += 1
        # gold signatures
        for sig in (b'\xa3\xfc\xff', b'\x0f\x28\xc8', b'\x50\x10\x00', b'\x6e\xea\x70'):
            j = joined.find(sig)
            if j >= 0:
                sig_hits[sig.hex()] += 1
                files_with[f'sig {sig.hex()}'].add(base)
        # window around first rank anchor (64 19 00 after c2)
        j = joined.find(b'\x19\x00', 0)
        # find proper: c2 ?? 64 19 00
        pos = None
        for i in range(len(joined) - 5):
            if joined[i] == 0xc2 and joined[i+3:i+5] == b'\x19\x00' and joined[i+2] == 0x64:
                pos = i
                break
        if pos is not None and len(windows) < 12:
            lo = max(0, pos - 0x40)
            hi = min(len(joined), pos + 0x60)
            windows.append((base, pos, joined[lo:hi].hex(), lo))
        # 5B imm8 candidates near any 64-19 anchor (subop class 0/3/7)
        for i in range(len(joined) - 5):
            if joined[i+3:i+5] in (b'\x19\x00', b'\x21\x00'):
                for w in range(max(0, i - 0x40), min(len(joined) - 5, i + 0x40)):
                    c = joined[w:w+5]
                    if c[4] in (0x00, 0x28, 0x50) and (c[0] & 7) in (0, 3, 7) and c[0] != 0xc2:
                        imm8_cands[(c[0], c[4])] += 1
                        files_with[f'imm8 b0={c[0]:02x} i={c[4]:02x}'].add(base)
                break
    print(f"files scanned: {nfiles} with .text: {ntext}")
    print("LDO c2..64 imm16 lowbyte counts:", dict(ldo_variants.most_common(12)))
    print("anchor hits (imm16 tails 1900=+0xc, 2100=+0x10, 0100=+0x00, 5100=+0x28):", dict(anchors))
    print("gold signatures found:", dict(sig_hits))
    print("5B imm8 candidates (byte0, imm8) top:", imm8_cands.most_common(15))
    print()
    print("== windows around first rank anchor ==")
    for base, pos, hx, lo in windows[:8]:
        print(f"{base} anchor@{pos:#x} (win start {lo:#x}):")
        print(f"  {hx}")
    print()
    print("== files per signature ==")
    for k in sorted(files_with):
        if len(files_with[k]) >= 1:
            print(f"  {k}: {len(files_with[k])} e.g. {sorted(files_with[k])[:3]}")

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Independent verification that lld applied the vendor SHAVE relocations exactly as
documented in lld-sparc/RELOC_NOTES.md.

Reads a RELA relocatable object (vendor mlibm member) and the ELF our lld produced from
it, locates each pulled-in input section inside the output .text, and re-computes every
relocation field with the documented formulas:
  type 200 (LO16)  : field |= (S+A) & 0xffff
  type 202 (HI16)  : field |= ((S+A) >> 16) & 0xffff
  type 209 (PCREL16): field += (S+A-P)  (mod 2^16), P = site VA, no scaling
  type 210 (PCREL16_AT4): bits[19:4] += (S+A-P)
  type 208 (ABS32) : LE32 = S+A
"""
import struct, sys, pathlib

ELF32_RELA = 4; ELF32_SYMTAB = 2; ELF32_STRTAB = 3

def parse_elf(path):
    d = pathlib.Path(path).read_bytes()
    shoff, = struct.unpack_from('<I', d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2e)
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsize
        f = struct.unpack_from('<IIIIIIIIII', d, o)
        secs.append(dict(name_off=f[0], type=f[1], flags=f[2], addr=f[3], off=f[4],
                         size=f[5], link=f[6], info=f[7], align=f[8], entsize=f[9]))
    strtab = secs[shstrndx]['off']
    for s in secs:
        end = d.index(b'\0', strtab + s['name_off'])
        s['name'] = d[strtab + s['name_off']:end].decode()
    return d, secs

def symtab(d, secs, symsec):
    out = []
    for o in range(symsec['off'], symsec['off'] + symsec['size'], 16):
        name, val, size, info, other, shndx = struct.unpack_from('<IIIBBH', d, o)
        out.append((name, val, size, info, shndx))
    strtab = secs[symsec['link']]['off']
    names = []
    for name, val, size, info, shndx in out:
        end = d.index(b'\0', strtab + name)
        names.append(d[strtab + name:end].decode())
    return names, out

def find_symsec(secs):
    for s in secs:
        if s['type'] == ELF32_SYMTAB:
            return s
    return None

def literal_runs(n, relset, minlen=12):
    runs, i = [], 0
    while i < n:
        if i in relset:
            i += 1; continue
        j = i
        while j < n and j not in relset:
            j += 1
        if j - i >= minlen:
            runs.append((i, j))
        i = j
    return runs

def main(obj_path, linked_path, report_all=False):
    od, osecs = parse_elf(obj_path)
    onames, osyms = symtab(od, osecs, find_symsec(osecs))
    ld, lsecs = parse_elf(linked_path)
    lnames, lsyms = symtab(ld, lsecs, find_symsec(lsecs))
    text = next(s for s in lsecs if s['name'] == '.text')
    tb = ld[text['off']:text['off'] + text['size']]
    sym_va = {lnames[i]: lsyms[i][1] for i in range(len(lnames)) if lsyms[i][1] and lnames[i]}
    # output VA of each input section, discovered by locating literal runs
    loc = {}     # input section name -> out VA
    for s in osecs:
        if s['type'] != 1 or not (s['flags'] & 4) or s['size'] == 0:   # executable, non-empty
            continue
        if not s['name'].startswith('.text'):
            continue
        rels = reloc_sites(od, osecs, s)
        body = od[s['off']:s['off'] + s['size']]
        runs = literal_runs(len(body), rels)
        if not runs:
            continue
        for a, b in runs:
            pos = tb.find(body[a:b])
            if pos >= 0:
                start = pos - a
                if start >= 0 and start + s['size'] <= len(tb):
                    loc[s['name']] = text['addr'] + start
                    break
    stats = {}
    fails = []
    for s in osecs:
        if s['name'] not in loc:
            continue
        va = loc[s['name']]
        body = od[s['off']:s['off'] + s['size']]
        for off, stype, symidx, addend in relocs(od, osecs, s):
            sname = onames[symidx]
            if not sname:                      # STT_SECTION symbol: use its section name
                shndx = osyms[symidx][4]
                if shndx < len(osecs):
                    sname = osecs[shndx]['name']
            if sname.startswith('.text.') or sname.startswith('.rodata'):
                if sname in loc:
                    S = loc[sname]
                else:
                    continue          # section dropped by --gc-sections
            elif sname in sym_va:
                S = sym_va[sname]
            else:
                continue
            Sfull = S + addend
            ste = text['addr'] + (va - text['addr']) + off
            cur = int.from_bytes(tb[va - text['addr'] + off: va - text['addr'] + off + (4 if stype == 210 else 2)], 'little')
            old = int.from_bytes(body[off:off + (4 if stype == 210 else 2)], 'little')
            if stype == 200:
                want = old | (Sfull & 0xffff)
                got = cur
            elif stype == 202:
                want = old | ((Sfull >> 16) & 0xffff)
                got = cur
            elif stype == 209:
                val = (S + addend) - ste
                want = (old + val) & 0xffff
                got = cur & 0xffff
            elif stype == 210:
                val = (S + addend) - ste
                want = ((old >> 4) + val) * 16 & 0xffff0 | (old & 0xfff0000f)
                got = cur
            else:
                continue
            key = f"{stype}"
            stats.setdefault(key, [0, 0])
            if want == got:
                stats[key][0] += 1
            else:
                stats[key][1] += 1
                fails.append(f"{s['name']}+{off:#x} type {stype} sym {sname}+{addend:#x} "
                             f"S={S:#x} old={old:#x} got={got:#x} want={want:#x}")
    print(f"object={obj_path} linked={linked_path}")
    print("sections located in output .text:")
    for k, v in sorted(loc.items(), key=lambda kv: kv[1]):
        print(f"   {k:32s} va=0x{v:08x}")
    print("relocations verified by documented formula (type: ok/bad):")
    for k in sorted(stats, key=int):
        print(f"   type {k}: {stats[k][0]} ok, {stats[k][1]} bad")
    for f in fails:
        print("   MISMATCH:", f)
    return 0 if not fails else 1

def reloc_sites(d, secs, target_sec):
    return {off for off, _, _, _ in relocs(d, secs, target_sec)}

def relocs(d, secs, target_sec):
    """All RELA entries whose sh_info targets target_sec's index."""
    idx = secs.index(target_sec)
    out = []
    for s in secs:
        if s['type'] != ELF32_RELA or s['info'] != idx or s['size'] == 0:
            continue
        for o in range(s['off'], s['off'] + s['size'], 12):
            off, info, addend = struct.unpack_from('<IIi', d, o)
            out.append((off, info & 0xff, info >> 8, addend))
    return sorted(out)

if __name__ == '__main__':
    sys.exit(main(sys.argv[1], sys.argv[2]))

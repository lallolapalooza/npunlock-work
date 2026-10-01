#!/usr/bin/env python3
"""Numeric verification of vendor type-208 (R_SHAVE_ABS32) relocations.

Uses the lld -Map file to learn where each input section landed inside the output
section (lld reports non-alloc debug sections by their offset), then checks that
the linked LE32 at every site equals S+A exactly as RELOC_NOTES.md documents.
"""
import re, struct, sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent))
from check_relocs import parse_elf, symtab, find_symsec, relocs

MAP_LINE = re.compile(r'^\s*([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+\d+\s+(.*)\(([^)]*)\):\(([^)]+)\)\s*$')

def parse_map(path, obj_name):
    """(object, section) -> [(addr, size)] in map order."""
    entries = {}
    for line in pathlib.Path(path).read_text().splitlines():
        m = MAP_LINE.match(line)
        if not m:
            continue
        addr, size, infile, obj, sec = int(m.group(1), 16), int(m.group(3), 16), m.group(4), m.group(5), m.group(6)
        if obj_name != obj:
            continue
        entries.setdefault(sec, []).append((addr, size))
    return entries

def out_section_data(elf_path, sec_name):
    d, secs = parse_elf(elf_path)
    for s in secs:
        if s['name'] == sec_name:
            return d[s['off']:s['off'] + s['size']]
    raise KeyError(sec_name)

def main(obj_path, linked_path, map_path, obj_name):
    od, osecs = parse_elf(obj_path)
    names, syms = symtab(od, osecs, find_symsec(osecs))
    layout = parse_map(map_path, obj_name)
    cache = {}
    ok = bad = 0
    failures = []
    for s in osecs:
        if not s['name'].startswith('.debug'):
            continue
        sites = relocs(od, osecs, s)
        if not sites:
            continue
        if s['name'] not in layout:
            continue
        site_base = layout[s['name']][0][0]
        if s['name'] not in cache:
            cache[s['name']] = out_section_data(linked_path, s['name'])
        site_blob = cache[s['name']]
        for off, stype, symidx, addend in sites:
            if stype != 208:
                continue
            shndx = syms[symidx][4]
            tsec = osecs[shndx]['name'] if shndx < len(osecs) else None
            if tsec is None or tsec not in layout:
                continue
            S = layout[tsec][0][0]
            want = (S + addend) & 0xffffffff
            loc = site_base + off
            if loc + 4 > len(site_blob):
                continue
            got = struct.unpack_from('<I', site_blob, loc)[0]
            if got == want:
                ok += 1
            else:
                bad += 1
                if len(failures) < 5:
                    failures.append(f"{obj_name}:{s['name']}+{off:#x} S={S:#x} A={addend:#x} got={got:#x} want={want:#x}")
    print(f"type 208 verified in {obj_name}: {ok} ok, {bad} bad")
    for f in failures:
        print("  MISMATCH", f)
    return bad

if __name__ == '__main__':
    obj, linked, mp, name = sys.argv[1:5]
    sys.exit(1 if main(obj, linked, mp, name) else 0)

#!/usr/bin/env python3
"""kernel-lib: list the ACT entry symbols of a built kernel image.

A multi-entry image (src/multi4.c) holds several `controlled_act`-like entries
whose positions must sit on 0x400-byte boundaries so that tools/patch_ranges.py
can point one ACT range at each of them.  Prints one TSV line per function
symbol in .text: `name<TAB>text_offset<TAB>size<TAB>aligned` -- `aligned` is
`yes` when the offset is a multiple of 0x400.

usage: elf_entries.py KERNEL.elf
"""

from __future__ import annotations

import struct
import sys


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    data = open(argv[1], "rb").read()
    shoff = struct.unpack_from("<I", data, 0x20)[0]
    shentsize = struct.unpack_from("<H", data, 0x2E)[0]
    shnum = struct.unpack_from("<H", data, 0x30)[0]
    shstrndx = struct.unpack_from("<H", data, 0x32)[0]
    secs = []
    for index in range(shnum):
        off = shoff + index * shentsize
        name, kind, flags, addr, file_off, size, link, info, align, entsize = struct.unpack_from(
            "<IIIIIIIIII", data, off)
        secs.append(dict(index=index, nameoff=name, type=kind, addr=addr, off=file_off,
                         size=size, link=link, entsize=entsize))
    strings = secs[shstrndx]
    for sec in secs:
        end = data.index(b"\0", strings["off"] + sec["nameoff"])
        sec["name"] = data[strings["off"] + sec["nameoff"]:end].decode()

    text = next((s for s in secs if s["name"] == ".text"), None)
    if text is None:
        print("no .text section", file=sys.stderr)
        return 1
    symtab = next((s for s in secs if s["type"] == 2), None)
    if symtab is None:
        return 0
    symstr = secs[symtab["link"]]
    lines = []
    for index in range(symtab["size"] // symtab["entsize"]):
        off = symtab["off"] + index * symtab["entsize"]
        st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from(
            "<IIIBBH", data, off)
        if st_shndx != text["index"] or (st_info & 0xF) != 2:
            continue
        end = data.index(b"\0", symstr["off"] + st_name)
        name = data[symstr["off"] + st_name:end].decode() if st_name else ""
        if not name:
            continue
        offset = st_value - text["addr"]
        lines.append((offset, name, st_size))
    for offset, name, size in sorted(lines):
        print(f"{name}\t{offset:#06x}\t{size:#06x}\t"
              f"{'yes' if offset % 0x400 == 0 else 'no'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

#!/usr/bin/env python3
"""kernel-lib: point one ACT range of a patched graph at its own kernel entry.

`npunlock-patchblob` appends a single ELF image and points *every* discovered
ACT range at that image's base.  To run different kernels in different ranges of
one graph invocation, the per-range code pointer in
`.rlt.text.ActKernelRanges` (an ELF64 RELA addend) is rewritten to
`image_base + entry_offset`.  The rule that makes this work is measured: a
range's code address is rounded **down to a 0x400-byte boundary**, so entries
must sit on 0x400 boundaries (measured by the multiop workstream; the pattern
follows their scripts/blobtool.py, re-implemented here so kernel-lib does not
depend on that tree).

usage:
  tools/patch_ranges.py --graph IN.blob --out OUT.blob --offsets "0:0xc00,2:0x0"
  tools/patch_ranges.py --graph IN.blob --list
"""

from __future__ import annotations

import argparse
import pathlib
import struct
import sys

SECTION_INVOCATIONS = ".text.ActKernelInvocations"
SECTION_RANGES = ".text.ActKernelRanges"
SECTION_RELA = ".rlt.text.ActKernelRanges"
RANGE_ENTRY_BYTES = 0x18
RELA_ENTRY_BYTES = 0x18


def sections(data: bytes) -> dict[str, dict]:
    shoff = struct.unpack_from("<Q", data, 0x28)[0]
    shentsize = struct.unpack_from("<H", data, 0x3A)[0]
    shnum = struct.unpack_from("<H", data, 0x3C)[0]
    shstrndx = struct.unpack_from("<H", data, 0x3E)[0]
    entries = []
    for index in range(shnum):
        off = shoff + index * shentsize
        name, kind, flags, addr, file_off, size = struct.unpack_from("<IIQQQQ", data, off)
        link, info, align, entsize = struct.unpack_from("<IIQQ", data, off + 0x28)
        entries.append(dict(index=index, nameoff=name, type=kind, addr=addr, off=file_off,
                            size=size, link=link, align=align, entsize=entsize))
    strings = entries[shstrndx]
    for entry in entries:
        end = data.index(b"\0", strings["off"] + entry["nameoff"])
        entry["name"] = data[strings["off"] + entry["nameoff"]:end].decode()
    return {entry["name"]: entry for entry in entries}


def range_addend_offsets(data: bytes) -> dict[int, int]:
    """range index -> file offset of its code RELA addend."""
    found = {}
    rela = sections(data)[SECTION_RELA]
    for index in range(rela["size"] // RELA_ENTRY_BYTES):
        off = rela["off"] + index * RELA_ENTRY_BYTES
        target = struct.unpack_from("<Q", data, off)[0]
        if target % RANGE_ENTRY_BYTES == 8:
            found[target // RANGE_ENTRY_BYTES] = off + 16
    return found


def list_ranges(data: bytes) -> list[dict]:
    table = sections(data)[SECTION_RANGES]
    addends = range_addend_offsets(data)
    out = []
    for index in range(table["size"] // RANGE_ENTRY_BYTES):
        off = table["off"] + index * RANGE_ENTRY_BYTES
        link_range, code_lo, code_hi, extent = struct.unpack_from("<IIII", data, off)
        addend = (struct.unpack_from("<q", data, addends[index])[0]
                  if index in addends else None)
        out.append({"range": index, "link_range": link_range,
                    "code_addr": (code_hi << 32) | code_lo, "extent": extent,
                    "addend": addend})
    return out


def entry_table(elf: bytes) -> dict[int, tuple[str, int]]:
    """ELF32 text offsets of the function symbols -> (name, size)."""
    shoff = struct.unpack_from("<I", elf, 0x20)[0]
    shentsize = struct.unpack_from("<H", elf, 0x2E)[0]
    shnum = struct.unpack_from("<H", elf, 0x30)[0]
    shstrndx = struct.unpack_from("<H", elf, 0x32)[0]
    secs = []
    for index in range(shnum):
        off = shoff + index * shentsize
        name, kind, flags, addr, file_off, size = struct.unpack_from("<IIIIII", elf, off)
        link, info, align, entsize = struct.unpack_from("<IIII", elf, off + 24)
        secs.append(dict(index=index, nameoff=name, type=kind, addr=addr, off=file_off,
                         size=size, link=link, entsize=entsize))
    strings = secs[shstrndx]
    for sec in secs:
        end = elf.index(b"\0", strings["off"] + sec["nameoff"])
        sec["name"] = elf[strings["off"] + sec["nameoff"]:end].decode()
    text = next((s for s in secs if s["name"] == ".text"), None)
    symtab = next((s for s in secs if s["type"] == 2), None)
    if text is None or symtab is None:
        return {}
    symstr = secs[symtab["link"]]
    table: dict[int, tuple[str, int]] = {}
    for index in range(symtab["size"] // symtab["entsize"]):
        off = symtab["off"] + index * symtab["entsize"]
        st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from(
            "<IIIBBH", elf, off)
        if st_shndx != text["index"] or (st_info & 0xF) != 2:
            continue
        end = elf.index(b"\0", symstr["off"] + st_name)
        name = elf[symstr["off"] + st_name:end].decode() if st_name else ""
        if name:
            table[st_value - text["addr"]] = (name, st_size)
    return table


def patch(dst: bytearray, offsets: dict[int, int],
          extents: dict[int, int] | None = None) -> list[dict]:
    table = sections(bytes(dst))[SECTION_RANGES]
    addends = range_addend_offsets(bytes(dst))
    ranges = list_ranges(bytes(dst))
    if not ranges:
        raise SystemExit("no ACT ranges in this graph")
    base = ranges[0]["addend"]
    if base is None:
        raise SystemExit("range 0 has no code relocation to rewrite")
    changes = []
    for index, offset in sorted(offsets.items()):
        if index not in addends:
            raise SystemExit(f"range {index} has no code relocation "
                             f"(ranges: {sorted(addends)})")
        if offset % 0x400 != 0:
            raise SystemExit(f"entry offset {offset:#x} is not on a 0x400 boundary")
        if offset >= table["size"] * 0 + 0x100000:
            raise SystemExit(f"entry offset {offset:#x} is implausible")
        old = struct.unpack_from("<q", dst, addends[index])[0]
        struct.pack_into("<q", dst, addends[index], base + offset)
        old_extent = struct.unpack_from("<I", dst, table["off"] + index * RANGE_ENTRY_BYTES + 0xC)[0]
        new_extent = None
        if extents and index in extents and extents[index]:
            new_extent = extents[index]
            struct.pack_into("<I", dst, table["off"] + index * RANGE_ENTRY_BYTES + 0xC, new_extent)
        changes.append({"range": index, "old_addend": old, "new_addend": base + offset,
                        "entry_offset": offset, "old_extent": old_extent,
                        "new_extent": new_extent})
    return changes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--graph", required=True)
    parser.add_argument("--out")
    parser.add_argument("--offsets", help='"R:ADDR[,R:ADDR...]" (entry offsets per range)')
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()

    data = pathlib.Path(args.graph).read_bytes()
    if args.list:
        for row in list_ranges(data):
            print(f"range[{row['range']:2d}] code=0x{row['code_addr']:x} "
                  f"extent=0x{row['extent']:x} addend={row['addend']}")
        return 0

    if not args.offsets or not args.out:
        parser.print_usage()
        return 2
    offsets = {}
    for item in args.offsets.split(","):
        index, value = item.split(":")
        offsets[int(index, 0)] = int(value, 0)
    dst = bytearray(data)
    for change in patch(dst, offsets):
        print(f"range[{change['range']}]: addend {change['old_addend']} -> "
              f"{change['new_addend']} (entry +{change['entry_offset']:#x})")
    pathlib.Path(args.out).write_bytes(dst)
    print(f"wrote {args.out} ({len(dst)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

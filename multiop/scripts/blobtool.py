#!/usr/bin/env python3
"""blobtool.py - inspect SHAVE ELF32 kernels and NPU3720 graph blobs (ELF64),
and rewrite per-range ACT code pointers without touching npunlock sources.

Subcommands:
  syms  KERNEL.elf                 -> function symbols (name, text offset, size)
  graph GRAPH.blob                 -> sections + invocation/range tables + relocs
  patch --graph IN.blob --out OUT.blob --range-map R:ADDR,R:ADDR ... [--base BASE]
        Overwrite, for the given range indices, the range entry's code extent
        and the matching RELA addend in .rlt.text.ActKernelRanges.
        ADDR is the absolute code addend to write (i.e. KernelText-relative).
"""
import argparse
import struct
import sys

ELF32_SYMBOL = 16


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def u64(b, o):
    return struct.unpack_from("<Q", b, o)[0]


def elf32_sections(data):
    shoff = u32(data, 0x20)
    shentsize = u16(data, 0x2E)
    shnum = u16(data, 0x30)
    shstrndx = u16(data, 0x32)
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsize
        name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from(
            "<IIIIIIIIII", data, o
        )
        secs.append(dict(idx=i, nameoff=name, type=typ, flags=flags, addr=addr, off=off,
                         size=size, link=link, info=info, align=align, entsize=entsize))
    strtab = secs[shstrndx]
    for s in secs:
        end = data.index(b"\0", strtab["off"] + s["nameoff"])
        s["name"] = data[strtab["off"] + s["nameoff"]:end].decode()
    return secs


def elf32_symbols(data):
    secs = elf32_sections(data)
    symtab = next((s for s in secs if s["type"] == 2), None)
    if symtab is None:
        return []
    strtab = secs[symtab["link"]]
    out = []
    for i in range(symtab["size"] // symtab["entsize"]):
        o = symtab["off"] + i * symtab["entsize"]
        st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from(
            "<IIIBBH", data, o
        )
        end = data.index(b"\0", strtab["off"] + st_name)
        name = data[strtab["off"] + st_name:end].decode() if st_name else ""
        out.append(dict(name=name, value=st_value, size=st_size, info=st_info,
                        shndx=st_shndx, index=i))
    return out


def cmd_syms(args):
    data = open(args.elf, "rb").read()
    text = next(s for s in elf32_sections(data) if s["name"] == ".text")
    print(f".text addr={text['addr']:#x} size={text['size']:#x} file_off={text['off']:#x}")
    for s in elf32_symbols(data):
        if s["shndx"] == text["idx"] and s["name"] and (s["info"] & 0xF) == 2:
            print(f"  {s['name']:<20} text_offset={s['value'] - text['addr']:#06x} "
                  f"size={s['size']:#06x}")


GRAPH_SECTIONS = (".text.KernelText", ".text.KernelParams", ".text.ActKernelInvocations",
                  ".text.ActKernelRanges", ".rlt.text.ActKernelRanges")


def graph_sections(data):
    shoff = u64(data, 0x28)
    shentsize = u16(data, 0x3A)
    shnum = u16(data, 0x3C)
    shstrndx = u16(data, 0x3E)
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsize
        name, typ, flags, addr, off, size, link, info, align, entsize = struct.unpack_from(
            "<IIQQQQIIQQ", data, o)
        secs.append(dict(idx=i, nameoff=name, type=typ, flags=flags, addr=addr, off=off,
                         size=size, link=link, info=info, align=align, entsize=entsize,
                         hdr=o))
    strtab = secs[shstrndx]
    for s in secs:
        end = data.index(b"\0", strtab["off"] + s["nameoff"])
        s["name"] = data[strtab["off"] + s["nameoff"]:end].decode()
    return secs


def cmd_graph(args):
    data = open(args.graph, "rb").read()
    secs = graph_sections(data)
    by = {s["name"]: s for s in secs}
    for s in secs:
        if s["name"] in GRAPH_SECTIONS:
            print(f"{s['name']:<32} idx={s['idx']:<2} off={s['off']:#x} size={s['size']:#x} "
                  f"addr={s['addr']:#x} type={s['type']} link={s['link']}")
    inv = by[".text.ActKernelInvocations"]
    rng = by[".text.ActKernelRanges"]
    rlt = by[".rlt.text.ActKernelRanges"]
    print(f"invocations: {inv['size'] // 0x40}   ranges: {rng['size'] // 0x18}")
    for i in range(rng["size"] // 0x18):
        o = rng["off"] + i * 0x18
        link_range, code_lo, code_hi, extent = struct.unpack_from("<IIII", data, o)
        print(f"  range[{i}] link_range={link_range} code_addr={code_hi:#x}{code_lo:08x} "
              f"extent={extent} (@file {o:#x})")
    for i in range(rlt["size"] // 0x18):
        o = rlt["off"] + i * 0x18
        target = u64(data, o)
        info = u64(data, o + 8)
        addend = struct.unpack_from("<q", data, o + 16)[0]
        print(f"  rela[{i}] target_off={target:#x} type={info & 0xFFFFFFFF} "
              f"sym={info >> 32} addend={addend} (@file {o:#x}, addend@ {o + 16:#x})")


def patch_ranges(dst, range_map, base=None):
    """range_map: {range_index: code_addend}. Sets extent (range entry +0xc) and the
    range's code RELA addend. If base is not None, extent = kernel_image_size - offset."""
    secs = graph_sections(dst)
    by = {s["name"]: s for s in secs}
    rng = by[".text.ActKernelRanges"]
    rlt = by[".rlt.text.ActKernelRanges"]
    rela_by_range = {}
    for i in range(rlt["size"] // 0x18):
        o = rlt["off"] + i * 0x18
        target = u64(dst, o)
        if target % 0x18 == 8:
            rela_by_range[target // 0x18] = o + 16
    changes = []
    for ridx, addend in range_map.items():
        o = rng["off"] + ridx * 0x18
        old_extent = u32(dst, o + 0xC)
        struct.pack_into("<Q", dst, rela_by_range[ridx], addend)
        changes.append((ridx, old_extent, rela_by_range[ridx]))
    return changes


def set_extent(dst, ridx, extent):
    secs = graph_sections(dst)
    by = {s["name"]: s for s in secs}
    rng = by[".text.ActKernelRanges"]
    struct.pack_into("<I", dst, rng["off"] + ridx * 0x18 + 0xC, extent)


def range_addend_offset(dst, ridx):
    secs = graph_sections(dst)
    by = {s["name"]: s for s in secs}
    rlt = by[".rlt.text.ActKernelRanges"]
    for i in range(rlt["size"] // 0x18):
        o = rlt["off"] + i * 0x18
        target = u64(dst, o)
        if target // 0x18 == ridx and target % 0x18 == 8:
            return o + 16
    raise KeyError(ridx)


def cmd_patch(args):
    dst = bytearray(open(args.graph, "rb").read())
    range_map = {}
    for item in args.range_map.split(","):
        ridx, addr = item.split(":")
        range_map[int(ridx, 0)] = int(addr, 0)
    changes = patch_ranges(dst, range_map)
    for ridx, old_extent, addend_off in changes:
        print(f"range[{ridx}]: addend->{range_map[ridx]:#x} (rela addend @ {addend_off:#x}), "
              f"old extent {old_extent:#x}")
    open(args.out, "wb").write(dst)
    print(f"wrote {args.out} ({len(dst)} bytes)")


def main():
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("syms")
    s.add_argument("elf")
    s.set_defaults(func=cmd_syms)
    g = sub.add_parser("graph")
    g.add_argument("graph")
    g.set_defaults(func=cmd_graph)
    q = sub.add_parser("patch")
    q.add_argument("--graph", required=True)
    q.add_argument("--out", required=True)
    q.add_argument("--range-map", required=True, help="R:ADDR[,R:ADDR...]")
    q.set_defaults(func=cmd_patch)
    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    sys.exit(main())

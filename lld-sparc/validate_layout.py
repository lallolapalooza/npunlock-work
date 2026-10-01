#!/usr/bin/env python3
"""Validate the byte-level layout of the moviLLD-produced gold SHAVE ELF.

Gold fixture: npunlock/tests/fixtures/npu3720/add1-fp16.elf
  (828 bytes, sha256 3f9d52273870c2e911000da3c3bd474c0514bd297d29539a14e935b2b01de6d5)

This script encodes every invariant documented in LAYOUT_SPEC.md:
  * ELF header fields, program headers (PT_LOAD coverage of .text only,
    PT_GNU_STACK), the 4-phdr header reservation (0xb4) that positions the
    first section,
  * section table order/fields/offsets/alignments and the exact chain of
    file offsets,
  * shstrtab/symtab/strtab contents (symtab = null + controlled_act only),
  * the vendor .comment banner,
  * SHA-256 fingerprints of the byte-exact regions.

Exit status 0 = all invariants hold (expected on the gold fixture).

Optionally, `--banner STR` predicts the file layout that our patched lld
would produce for a different .comment banner (see LAYOUT_SPEC.md), printing
the resulting offsets without failing.
"""

import hashlib
import struct
import sys

GOLD = "/home/asdf/projects/npunlock-work/npunlock/tests/fixtures/npu3720/add1-fp16.elf"
GOLD_SHA256 = "3f9d52273870c2e911000da3c3bd474c0514bd297d29539a14e935b2b01de6d5"
GOLD_COMMENT = b"Linker: Movidius Linker (moviLLD) v3.0.9\nbased on LLD 14.0.0\x00"
GOLD_SHA_PREFIX = "5cac6e472348e458e90ce71b6c2156cb0c004a7f464b95396fcb8245a9bd0634"
GOLD_TEXT_SHA256 = "bfe23abd677c5594c2956d1912f6d87aa87c62a6119bd8c85ab2a5fa08aaffab"

# Planned program headers used by the writer *before* removeEmptyPTLoad():
#   PT_PHDR + PT_LOAD(headers: .arg.data joins this load) + PT_LOAD(.text)
#   + PT_GNU_STACK   ->   4 * 32 = 128 bytes reserved after the 52-byte
#                        ELF header => first section offset = 180 = 0xb4.
PLANNED_PHDRS = 4
HDR_RESERVE = 52 + PLANNED_PHDRS * 32  # 0xb4

# Exact section table of the gold file (name -> field tuple).
# fields: type, flags, addr, offset, size, link, info, addralign, entsize
GOLD_SECTIONS = [
    ("",          0, 0x0, 0x0,        0x0,   0,  0, 0, 0,  0),
    (".arg.data", 1, 0x2, 0x1e000000, 0xb4,  0,  0, 0, 1,  0),
    (".text",     1, 0x6, 0x1d000000, 0xc0,  0xc0, 0, 0, 16, 0),
    (".comment",  1, 0x30, 0x0,      0x180, 0x3d, 0, 0, 1, 1),
    (".symtab",   2, 0x0, 0x0,       0x1c0, 0x20, 6, 1, 4, 16),
    (".shstrtab", 3, 0x0, 0x0,       0x1e0, 0x34, 0, 0, 1, 0),
    (".strtab",   3, 0x0, 0x0,       0x214, 0x10, 0, 0, 1, 0),
]
GOLD_SHSTRTAB = b"\0.arg.data\0.text\0.comment\0.symtab\0.shstrtab\0.strtab\0"
GOLD_STRTAB = b"\0controlled_act\0"

failures = []


def check(cond, msg):
    if cond:
        print("  ok   %s" % msg)
    else:
        print("  FAIL %s" % msg)
        failures.append(msg)


def align_up(x, a):
    return (x + a - 1) & -a if a else x


def parse(path):
    f = open(path, "rb").read()
    e = {}
    e["ident"] = f[:16]
    (e["type"], e["machine"], e["version"], e["entry"], e["phoff"],
     e["shoff"], e["flags"], e["ehsize"], e["phentsize"], e["phnum"],
     e["shentsize"], e["shnum"], e["shstrndx"]) = struct.unpack_from(
        "<HHIIIIIHHHHHH", f, 16)
    phdrs = []
    for i in range(e["phnum"]):
        phdrs.append(struct.unpack_from("<8I", f, e["phoff"] + i * e["phentsize"]))
    shdrs = []
    for i in range(e["shnum"]):
        shdrs.append(struct.unpack_from("<10I", f, e["shoff"] + i * e["shentsize"]))
    str_off = shdrs[e["shstrndx"]][4]
    str_sz = shdrs[e["shstrndx"]][5]
    shstr = f[str_off:str_off + str_sz]

    def name(off):
        return shstr[off:shstr.index(b"\0", off)].decode()

    sections = []
    for s in shdrs:
        sections.append((name(s[0]),) + tuple(s[1:]))
    return f, e, phdrs, sections, shdrs


def predict_layout(comment_len):
    """File-offset chain model from LAYOUT_SPEC.md (offsets after .text)."""
    end = 0x180  # .text ends here
    comment = end
    end = comment + comment_len
    symtab = align_up(end, 4)
    end = symtab + 0x20
    shstrtab = end
    end = shstrtab + 0x34
    strtab = end
    end = strtab + 0x10
    shoff = align_up(end, 4)
    filesize = shoff + 7 * 40
    return dict(comment=comment, symtab=symtab, shstrtab=shstrtab,
                strtab=strtab, shoff=shoff, filesize=filesize)


def main():
    args = sys.argv[1:]
    banner = None
    path = GOLD
    if "--banner" in args:
        i = args.index("--banner")
        banner = args[i + 1].encode() + b"\0"
        if i + 2 < len(args):
            path = args[i + 2]
    elif args:
        path = args[0]

    if banner is not None:
        model = predict_layout(len(banner))
        print("banner: %r (%d bytes incl. NUL)" % (banner, len(banner)))
        print("predicted layout for %s:" % path)
        for k in ("comment", "symtab", "shstrtab", "strtab", "shoff", "filesize"):
            print("  %-9s = %5d (0x%x)" % (k, model[k], model[k]))
        print("gold      : comment=384(0x180) symtab=448(0x1c0) "
              "shstrtab=480(0x1e0) strtab=532(0x214) shoff=548(0x224) "
              "filesize=828")
        return 0

    f, e, phdrs, sections, shdrs = parse(path)
    print("validating %s (%d bytes)" % (path, len(f)))

    print("[ELF header]")
    check(hashlib.sha256(f).hexdigest() == GOLD_SHA256, "file sha256 matches gold")
    check(f[:16] == bytes.fromhex("7f454c46010101000000000000000000"),
          "e_ident: ELF32, little-endian, System V")
    check(e["type"] == 2 and e["machine"] == 2 and e["version"] == 1,
          "e_type=ET_EXEC, e_machine=EM_SPARC(2), e_version=1")
    check(e["entry"] == 0x1d000000, "e_entry = 0x1d000000 (.text / controlled_act)")
    check(e["phoff"] == 52 and e["ehsize"] == 52 and e["phentsize"] == 32,
          "program headers start right after the 52-byte ELF header")
    check(e["phnum"] == 2, "e_phnum = 2 (PT_LOAD + PT_GNU_STACK)")
    check(e["shentsize"] == 40 and e["shnum"] == 7 and e["shstrndx"] == 5,
          "e_shentsize=40, e_shnum=7, e_shstrndx=5")
    check(e["shoff"] == 548, "e_shoff = 548 (0x224)")
    check(e["flags"] == 0, "e_flags = 0")

    print("[header reservation / first section offset]")
    check(HDR_RESERVE == 0xb4, "reserved header = 52 + 4*32 = 0xb4 (4 planned phdrs)")
    check(sections[1][4] == HDR_RESERVE,
          ".arg.data offset == 0xb4 == reserved ELF+phdr area")
    check(f[0x74:0xb4] == bytes(0x40), "bytes [0x74,0xb4) are zero-filled padding")
    check(e["phoff"] + e["phnum"] * e["phentsize"] == 0x74,
          "emitted phdrs end at 0x74 (only 2 of the 4 planned phdrs survive: "
          "PT_PHDR dropped by allocateHeaders, empty .arg.data PT_LOAD dropped "
          "by removeEmptyPTLoad)")

    print("[program headers]")
    p0 = phdrs[0]
    check(p0[0] == 1 and p0[1] == 0xc0 and p0[2] == 0x1d000000 and p0[3] == 0x1d000000,
          "PT_LOAD: offset 0xc0, vaddr/paddr 0x1d000000")
    check(p0[4] == 0xc0 and p0[5] == 0xc0, "PT_LOAD filesz == memsz == 0xc0")
    check(p0[6] == 5 and p0[7] == 0x10, "PT_LOAD flags = R+X (5), align = 0x10")
    check((p0[1] - p0[2]) % p0[7] == 0, "PT_LOAD offset == vaddr (mod align)")
    text = sections[2]
    check(p0[1] == text[4] and p0[4] == text[5] and p0[2] == text[3],
          "PT_LOAD covers .text exactly")
    p1 = phdrs[1]
    check(p1[0] == 0x6474E551, "phdr[1] is PT_GNU_STACK (0x6474e551)")
    check(p1[1:5] == (0, 0, 0, 0) and p1[4] == 0 and p1[5] == 0,
          "PT_GNU_STACK offset/vaddr/paddr/filesz/memsz all zero")
    check(p1[6] == 6 and p1[7] == 0, "PT_GNU_STACK flags = RW (6), align = 0")

    print("[section table]")
    check([s[0] for s in sections] == [s[0] for s in GOLD_SECTIONS],
          "section order: NULL .arg.data .text .comment .symtab .shstrtab .strtab")
    for got, want in zip(sections, GOLD_SECTIONS):
        check(got == want, "shdr %r exact: %r" % (want[0], want))

    print("[file-offset chain]")
    check(sections[3][4] == sections[2][4] + sections[2][5],
          ".comment starts where .text ends (0x180)")
    check(sections[4][4] == align_up(sections[3][4] + sections[3][5], 4),
          ".symtab = align4(.comment end) = 0x1c0")
    check(sections[5][4] == sections[4][4] + sections[4][5],
          ".shstrtab follows .symtab (0x1e0)")
    check(sections[6][4] == sections[5][4] + sections[5][5],
          ".strtab follows .shstrtab (0x214)")
    check(e["shoff"] == align_up(sections[6][4] + sections[6][5], 4),
          "section headers follow .strtab (0x224)")
    check(e["shoff"] + e["shnum"] * e["shentsize"] == len(f),
          "file size == shoff + 7*40 == 828")

    print("[contents]")
    model = predict_layout(len(GOLD_COMMENT))
    check(model["symtab"] == 0x1c0 and model["shoff"] == 0x224 and
          model["filesize"] == 828,
          "offset model reproduces gold (symtab 0x1c0, shoff 0x224, size 828)")
    check(f[0x180:0x180 + len(GOLD_COMMENT)] == GOLD_COMMENT,
          ".comment == vendor banner %r" % GOLD_COMMENT[:-1].decode())
    shstr = f[sections[5][4]:sections[5][4] + sections[5][5]]
    check(shstr == GOLD_SHSTRTAB, ".shstrtab contents exact")
    strtab = f[sections[6][4]:sections[6][4] + sections[6][5]]
    check(strtab == GOLD_STRTAB, ".strtab == '\\0controlled_act\\0'")
    sym = f[sections[4][4]:sections[4][4] + sections[4][5]]
    check(len(sym) == 0x20, ".symtab holds exactly 2 entries (null + 1 global)")
    st_name, st_val, st_size, st_info, st_other, st_shndx = struct.unpack_from(
        "<IIIBBH", sym, 16)
    check(st_name == 1 and st_val == 0x1d000000 and st_size == 187 and
          st_info == 0x12 and st_other == 0 and st_shndx == 2,
          "sym[1] = controlled_act, st_value=0x1d000000, st_size=187, "
          "STB_GLOBAL|STT_FUNC, section .text")
    arg_data = f[sections[1][4]:sections[1][4] + sections[1][5]]
    check(sections[1][5] == 0 and arg_data == b"", ".arg.data present with size 0")

    print("[fingerprints]")
    check(hashlib.sha256(f[:0x180]).hexdigest() == GOLD_SHA_PREFIX,
          "sha256(bytes[0,0x180)) == 5cac6e47... (headers+pad+.text region)")
    check(hashlib.sha256(f[0xc0:0x180]).hexdigest() == GOLD_TEXT_SHA256,
          "sha256(.text) == bfe23abd...")

    if failures:
        print("\n%d CHECK(S) FAILED" % len(failures))
        return 1
    print("\nALL CHECKS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

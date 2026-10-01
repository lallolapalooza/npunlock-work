#!/usr/bin/env python3
"""Standalone re-implementation of npunlock's npunlock_parse_shave_elf contract.

Mirrors src/common/elf32.c check-for-check so the corpus driver can validate a
linked SHAVE ELF without building the npunlock tree.  Prints a one-line verdict:

    OK text=<size> off=<file_offset> entry=0x... addr=0x...
    FAIL <reason>

Exit status 0 on OK, 1 on FAIL.
"""
import struct
import sys

ELF32_HEADER_SIZE = 52
ELF32_SECTION_SIZE = 40
ELF32_PROGRAM_SIZE = 32
ELF32_SYMBOL_SIZE = 16
ELF_TYPE_EXEC = 2
ELF_MACHINE_SPARC = 2
SHT_PROGBITS = 1
SHT_SYMTAB = 2
SHT_STRTAB = 3
SHT_RELA = 4
SHT_NOBITS = 8
SHT_REL = 9
PT_LOAD = 1
SHN_UNDEF = 0
SHAVE_TEXT_ADDRESS = 0x1D000000
SHAVE_ARG_ADDRESS = 0x1E000000


def fail(msg):
    print("FAIL " + msg)
    sys.exit(1)


def main(path):
    with open(path, "rb") as handle:
        elf = handle.read()
    if len(elf) < ELF32_HEADER_SIZE or elf[:4] != b"\x7fELF" or elf[4:7] != b"\x01\x01\x01":
        fail("expected ELF32 little-endian version 1")
    (e_type, e_machine) = struct.unpack_from("<HH", elf, 16)
    e_version = struct.unpack_from("<I", elf, 20)[0]
    if (e_type, e_machine, e_version) != (ELF_TYPE_EXEC, ELF_MACHINE_SPARC, 1):
        fail("expected an executable SPARC/SHAVE ELF")
    entry, phoff, shoff = struct.unpack_from("<III", elf, 24)
    ehsize, phentsize, phnum, shentsize, shnum, shstrndx = struct.unpack_from(
        "<HHHHHH", elf, 40)
    if ehsize < ELF32_HEADER_SIZE or (phnum != 0 and phentsize < ELF32_PROGRAM_SIZE) or \
            shentsize < ELF32_SECTION_SIZE or shnum == 0 or shstrndx >= shnum or \
            shoff + shentsize * shnum > len(elf):
        fail("invalid ELF32 header or section table")
    if phnum != 0 and phoff + phentsize * phnum > len(elf):
        fail("invalid ELF32 program table")

    def section(index):
        off = shoff + index * shentsize
        name, stype, flags, addr, soff, size = struct.unpack_from("<IIIIII", elf, off)
        return {"name": name, "type": stype, "flags": flags, "addr": addr,
                "off": soff, "size": size}

    def string_at(table, offset):
        end = elf.find(b"\x00", table["off"] + offset)
        if end < 0:
            return None
        return elf[table["off"] + offset:end].decode("latin-1")

    strings = section(shstrndx)
    if strings["type"] != SHT_STRTAB or strings["off"] + strings["size"] > len(elf):
        fail("invalid ELF32 section-name table")

    text = arg_data = None
    for index in range(shnum):
        sec = section(index)
        if sec["type"] != SHT_NOBITS and sec["off"] + sec["size"] > len(elf):
            fail("ELF32 section is malformed or out of bounds")
        name = string_at(strings, sec["name"])
        if name is None:
            fail("ELF32 section is malformed or out of bounds")
        if sec["type"] in (SHT_REL, SHT_RELA) and sec["size"] != 0:
            fail("SHAVE ELF contains unresolved relocations")
        if sec["type"] == SHT_SYMTAB:
            count = sec["size"] // ELF32_SYMBOL_SIZE
            for sym in range(1, count):
                soff = sec["off"] + sym * ELF32_SYMBOL_SIZE
                if soff + ELF32_SYMBOL_SIZE > len(elf):
                    fail("ELF32 symbol lies outside the file")
                sname = struct.unpack_from("<I", elf, soff)[0]
                shndx = struct.unpack_from("<H", elf, soff + 14)[0]
                if sname != 0 and shndx == SHN_UNDEF:
                    fail("SHAVE ELF contains an undefined symbol")
                if sname != 0 and string_at(strings, sname) is None:
                    fail("ELF32 symbol name is invalid")
        if name == ".text":
            if text is not None:
                fail("SHAVE ELF has multiple .text sections")
            text = sec
        elif name == ".arg.data":
            if arg_data is not None:
                fail("SHAVE ELF has multiple .arg.data sections")
            arg_data = sec

    if text is None or text["type"] != SHT_PROGBITS or text["size"] == 0 or \
            (text["flags"] & 6) != 6 or text["addr"] != SHAVE_TEXT_ADDRESS or \
            entry != text["addr"]:
        fail("SHAVE .text or entry does not match the 3720xx ACT contract")
    if arg_data is None or arg_data["addr"] != SHAVE_ARG_ADDRESS or arg_data["size"] != 0:
        fail("SHAVE .arg.data must be empty at 0x1e000000")

    loadable = False
    for index in range(phnum):
        off = phoff + index * phentsize
        p_type, p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz, p_flags = \
            struct.unpack_from("<IIIIIII", elf, off)
        if p_filesz > p_memsz or p_offset + p_filesz > len(elf):
            fail("ELF32 program extent is invalid")
        if p_type == PT_LOAD and (p_flags & 5) == 5 and text["off"] >= p_offset and \
                text["addr"] >= p_vaddr:
            fo = text["off"] - p_offset
            mo = text["addr"] - p_vaddr
            if fo + text["size"] <= p_filesz and mo + text["size"] <= p_memsz:
                loadable = True
    if not loadable:
        fail("SHAVE .text is not covered by an executable load segment")

    print("OK text=%d off=%d entry=0x%08x addr=0x%08x" %
          (text["size"], text["off"], entry, text["addr"]))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: validate_elf.py ELF")
        sys.exit(2)
    main(sys.argv[1])

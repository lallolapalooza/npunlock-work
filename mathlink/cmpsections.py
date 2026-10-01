#!/usr/bin/env python3
"""Extract a named section's bytes from an ELF32 file (works for REL objects and linked EXELs)."""
import struct, sys, pathlib

def sections(path):
    d = pathlib.Path(path).read_bytes()
    shoff, = struct.unpack_from('<I', d, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x2e)
    raw = []
    for i in range(shnum):
        o = shoff + i*shentsize
        name, typ, flags, addr, off, size = struct.unpack_from('<IIIIII', d, o)
        raw.append(dict(name_off=name, type=typ, flags=flags, addr=addr, off=off, size=size))
    strtab_off = raw[shstrndx]['off']
    for s in raw:
        end = d.index(b'\0', strtab_off + s['name_off'])
        s['name'] = d[strtab_off + s['name_off']:end].decode()
    return d, raw

def get(path, name):
    d, secs = sections(path)
    for s in secs:
        if s['name'] == name:
            return d[s['off']:s['off']+s['size']], s
    raise KeyError(f"{name} not in {path}")

if __name__ == "__main__":
    path, name = sys.argv[1], sys.argv[2]
    data, s = get(path, name)
    print(f"{path}:{name} addr=0x{s['addr']:x} size=0x{s['size']:x} flags={s['flags']}")
    sys.stdout.buffer.write(data)

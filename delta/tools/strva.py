#!/usr/bin/env python3
"""Map vendor-DLL strings to virtual addresses (matches Ghidra s_<name>_<hexaddr> labels).
Usage:
  strva.py <exe> <needle>            # find all occurrences (bytes) of needle, print VA
  strva.py <exe> --addr <hexva>      # read C-string at VA
"""
import sys, struct

def pe_sections(path):
    f = open(path, 'rb')
    data = f.read()
    f.close()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe+4] == b'PE\0\0', 'not PE'
    nsec = struct.unpack_from('<H', data, pe+6)[0]
    optsz = struct.unpack_from('<H', data, pe+20)[0]
    opt = pe + 24
    magic = struct.unpack_from('<H', data, opt)[0]
    imagebase = struct.unpack_from('<Q', data, opt+24)[0] if magic == 0x20b else struct.unpack_from('<I', data, opt+28)[0]
    secoff = opt + optsz
    secs = []
    for i in range(nsec):
        o = secoff + i*40
        name = data[o:o+8].rstrip(b'\0').decode('latin1')
        vsize, va, rawsize, rawptr = struct.unpack_from('<IIII', data, o+8)
        secs.append((name, va, vsize, rawptr, rawsize))
    return data, imagebase, secs

def va_from_off(secs, imagebase, off):
    for name, va, vsize, rawptr, rawsize in secs:
        if rawptr <= off < rawptr + rawsize:
            return imagebase + va + (off - rawptr), name
    return None, None

def off_from_va(secs, imagebase, vaddr):
    rva = vaddr - imagebase
    for name, va, vsize, rawptr, rawsize in secs:
        if va <= rva < va + max(vsize, rawsize):
            return rawptr + (rva - va), name
    return None, None

def main():
    exe = sys.argv[1]
    data, imagebase, secs = pe_sections(exe)
    if sys.argv[2] == '--addr':
        va = int(sys.argv[3], 16)
        off, name = off_from_va(secs, imagebase, va)
        if off is None:
            print('unmapped'); return
        end = data.find(b'\0', off)
        print('%x [%s] %r' % (va, name, data[off:end][:300]))
        return
    needle = sys.argv[2].encode()
    start = 0
    n = 0
    while True:
        i = data.find(needle, start)
        if i < 0:
            break
        va, name = va_from_off(secs, imagebase, i)
        if va:
            end = data.find(b'\0', i)
            print('%x %s %r' % (va, name, data[i:min(end, i+120)]))
            n += 1
        start = i + 1
    if n == 0:
        print('not found')

main()

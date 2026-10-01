"""Shared helpers for parsing moviAsm64.dll."""
import struct

DLL = '/home/asdf/projects/r2gvp04w-re/extracted/code$GetExtractPath$/Driver/MVC_DEPEND/bin/moviAsm64.dll'

def load(path=DLL):
    return open(path, 'rb').read()

def sections(d):
    e = struct.unpack_from('<I', d, 0x3c)[0]
    coff = e + 4
    nsec = struct.unpack_from('<H', d, coff + 2)[0]
    optsz = struct.unpack_from('<H', d, coff + 16)[0]
    opt = coff + 20
    ib = struct.unpack_from('<Q', d, opt + 24)[0]
    stab = opt + optsz
    secs = []
    for i in range(nsec):
        o = stab + i * 40
        nm = d[o:o + 8].rstrip(b'\0').decode()
        vs, va, rs, ra = struct.unpack_from('<IIII', d, o + 8)
        secs.append((nm, va, vs, ra, rs))
    return secs, ib

def off2va(secs, ib, off):
    for nm, va, vs, ra, rs in secs:
        if ra <= off < ra + rs:
            return ib + va + (off - ra)
    return None

def va2off(secs, ib, va):
    rva = va - ib
    for nm, vaddr, vsize, raddr, rsize in secs:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            return raddr + (rva - vaddr)
    return None

def off2sec(secs, off):
    for nm, va, vs, ra, rs in secs:
        if ra <= off < ra + rs:
            return nm
    return '?'

def cstr(d, off):
    end = d.index(b'\0', off)
    return d[off:end].decode('latin1')

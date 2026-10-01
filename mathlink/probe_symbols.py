#!/usr/bin/env python3
"""Per-symbol resolvability probe for the vendor mlibm.a.

For every symbol: build a one-symbol linker probe (LDIL/LDIH -> R_SHAVE_LO16/HI16),
assemble it with the toolchain-root moviAsm, link it with the toolchain-root moviLLD
(+ the real shave_kernel.ld, --gc-sections, compat helper objects), then record the
link status, the resulting .text/.arg.data sizes and npunlock's validator verdict.
"""
import hashlib, json, pathlib, struct, subprocess, sys, time

ROOT = pathlib.Path("/home/asdf/projects/npunlock-work/mathlink")
TC = pathlib.Path("/home/asdf/projects/npunlock-work/shave-toolchain-root")
LD = pathlib.Path("/home/asdf/projects/npunlock-work/npunlock/src/shavecc/shave_kernel.ld")
OUT = ROOT / "out" / "symprobe"
OUT.mkdir(parents=True, exist_ok=True)
COMPAT = ROOT / "out" / "compat_helpers.o"

def sh(*args, **kw):
    return subprocess.run(args, **kw)

def sec_sizes(elf: bytes):
    if len(elf) < 0x34 or elf[:4] != b"\x7fELF":
        return None
    shoff, = struct.unpack_from('<I', elf, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', elf, 0x2e)
    secs = []
    for i in range(shnum):
        o = shoff + i * shentsize
        name, typ, flags, addr, off, size = struct.unpack_from('<IIIIII', elf, o)
        secs.append((name, typ, flags, addr, off, size))
    strtab = secs[shstrndx][4]
    res = {}
    for name, typ, flags, addr, off, size in secs:
        end = elf.index(b'\0', strtab + name)
        nm = elf[strtab + name:end].decode()
        if nm in ('.text', '.arg.data'):
            res[nm] = (addr, size)
    res['_nsec'] = shnum
    res['_reloc_sections'] = sum(1 for n, t, *_ in secs if t in (4, 9)
                                 and n not in ('.rela.dyn', '.rel.dyn'))
    return res

def probe(sym):
    s_path = OUT / f"{sym}.s"
    o_path = OUT / f"{sym}.o"
    e_path = OUT / f"{sym}.elf"
    s_path.write_text(f"""\t.text
\t.section\t.text.controlled_act,"ax",@progbits
\t.globl\tcontrolled_act
\t.type\tcontrolled_act,@function
controlled_act:
\tLSU0.LDIL i17, {sym}
\tLSU0.LDIH i17, {sym}
\tBRU.JMP i30
\t.size\tcontrolled_act, .-controlled_act
""")
    for attempt in (1, 2, 3):
        with open(s_path, 'rb') as fh, open(o_path, 'wb') as fo:
            r = sh(str(TC / "bin/moviAsm"), "--cv", "3720xx", "--noSPrefixing",
                   stdin=fh, stdout=fo, stderr=subprocess.PIPE)
        if r.returncode == 0 and o_path.stat().st_size:
            break
        # shave-isa/shave_asm.py is edited by a peer agent; retry transient breakage
        time.sleep(3)
    if r.returncode != 0:
        return dict(symbol=sym, stage="assemble", status=r.returncode,
                    stderr=r.stderr.decode()[:400])
    with open(o_path, 'rb') as fh, open(e_path, 'wb') as fo:
        r = sh(str(TC / "bin/moviLLD"), "-flavor", "gnu", "-EL", "-e", "controlled_act",
               "-z", "max-page-size=0x10", "--gc-sections", str(TC / "lib/mlibm.a"),
               "-T", str(LD), str(o_path), str(COMPAT), "-o", "-",
               stdin=subprocess.DEVNULL, stdout=fo, stderr=subprocess.PIPE)
    err = r.stderr.decode()
    if r.returncode != 0:
        return dict(symbol=sym, stage="link", status=r.returncode, stderr=err[:500])
    elf = e_path.read_bytes()
    sizes = sec_sizes(elf)
    return dict(symbol=sym, stage="ok", status=0, elf_bytes=len(elf),
                text_addr=sizes['.text'][0], text_size=sizes['.text'][1],
                argdata_size=sizes['.arg.data'][1],
                reloc_sections=sizes['_reloc_sections'], link_stderr=err[:200])

ASM = pathlib.Path("/home/asdf/projects/npunlock-work/shave-isa/shave_asm.py")

def main():
    syms = [l.strip() for l in sys.stdin if l.strip()]
    print("shave_asm.py md5 at start:", hashlib.md5(ASM.read_bytes()).hexdigest(), flush=True)
    results = []
    for i, s in enumerate(syms):
        results.append(probe(s))
        if (i + 1) % 25 == 0:
            print(f"...{i+1}/{len(syms)}", flush=True)
    print("shave_asm.py md5 at end:  ", hashlib.md5(ASM.read_bytes()).hexdigest(), flush=True)
    (ROOT / "out" / "symprobe.json").write_text(json.dumps(results, indent=1))
    ok = [r for r in results if r['stage'] == 'ok']
    print(f"probed {len(syms)}: {len(ok)} linked, "
          f"{len(results)-len(ok)} failed at "
          f"{sorted({r['stage'] for r in results if r['stage']!='ok'})}")

main()

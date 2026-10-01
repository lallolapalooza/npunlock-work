# shave-toolchain-root — native SHAVE toolchain for npunlock (wave 2)

Native (no wine, no Windows DLLs) replacement for the MoviTools SHAVE
toolchain that npunlock's POSIX stage consumes. Directory contract comes from
`npunlock/src/shavecc/shavecc.c` and `npunlock/ABI/MOVITOOLS.md`; provenance is
the r2gvp04w reverse-engineering effort. **No vendor code is redistributed
here** — every binary is either built from open sources in this workspace or a
small shell wrapper around them; the only vendor-derived input is `lib/mlibm.a`
(copy of the proprietary archive, referenced in place by upstream tooling,
kept out of all repos).

Layout (root must be absolute; supplied via `NPUNLOCK_SHAVE_TOOLCHAIN_DIR`,
`npurun --toolchain-dir`, or `shavecc_options.toolchain_directory_utf8`):

```text
shave-toolchain-root/
├── bin/
│   ├── moviCompile   # C -> Movidius-syntax SHAVE .s
│   ├── moviAsm       # .s -> ELF32 LE EM_SPARC REL object
│   └── moviLLD       # objects -> linked ELF32 EXEC (stdout)
└── lib/
    └── mlibm.a       # proprietary vendor math archive (link input; copied
                      # from r2gvp04w RE tree, never committed to a repo)
```

## bin/moviCompile

POSIX shell wrapper around the wave-1 LLVM 14 fork's clang
(`/home/asdf/projects/llvm-14/build/bin/clang`, target `shave`, CPU
`3720xx`).

Contract argv (what npunlock spawns, stdin = C source, stdout = .s,
stderr = diagnostics):

```text
moviCompile -cc1 -triple shave -target-cpu <cpu> -S -O2 -x c entry.c \
    -ffunction-sections -fdata-sections [-DNAME=DEC ...] -o -
```

clang cannot open the literal filename `entry.c` (the source arrives on
stdin), so the wrapper rewrites exactly that token to `-` and appends
`-main-file-name entry.c` so `.file` and diagnostics still name the TU
`entry.c` (matching the `#line "entry.c"` markers shavecc injects when it
inlines the bundled NPU3720 kernel header). All other argv pass through
verbatim; `-o -` streams assembly to stdout.

## bin/moviAsm

POSIX shell wrapper around the reference assembler's object writer:

```text
exec python3 /home/asdf/projects/npunlock-work/shave-isa/shave_asm.py obj "$@"
```

Contract argv: `moviAsm --cv 3720xx --noSPrefixing` — `.s` on stdin, ELF32 LE
ET_REL EM_SPARC object on stdout, diagnostics on stderr. `obj` mode accepts
and ignores `--cv <value>` / `--noSPrefixing` (documented by AsmObjWriter in
`shave-isa/VALIDATION.md`). Assembly implementation lives in
`npunlock-work/shave-isa/` (owned by the assembler stream, not this wrapper).

## bin/moviLLD

POSIX shell wrapper around the patched lld
(`/home/asdf/projects/llvm-14/build/bin/ld.lld`, lld-shave.patch applied:
SPARC32/SHAVE target, reloc types 200-210, Movidius-style banner).

Contract argv:

```text
moviLLD -flavor gnu -EL -e <entry> -z max-page-size=0x10 --gc-sections \
    [<lib/mlibm.a>] -T <script> <object> -o -
```

lld has no `-o -` (it would create a file named `-`), so the wrapper rewrites
the trailing `-o` pair to a `mktemp` file (under `$TMPDIR`), runs lld with the
argv otherwise untouched, `cat`s the temp file to stdout, and removes it via an
EXIT trap (exit code preserved; stderr passes through). `-flavor gnu` is
consumed by lld's front end even in `ld.lld` mode (verified).

## lib/mlibm.a

922,244 bytes, copied verbatim from
`r2gvp04w-re/extracted/code$GetExtractPath$/Driver/MVC_DEPEND/lib/mlibm.a`.
npunlock passes it to the linker only when the file is readable
(`shavecc.c` `file_readable`); with `--gc-sections` and a kernel that has no
undefined math references, no members are pulled in (verified: gold-shaped
link is byte-identical with and without the archive present in argv).

## Provenance / build

- LLVM+clang+lld tree: `/home/asdf/projects/llvm-14` (LLVM 14.0.6 fork with
  SHAVE target; `ninja -C build lld` applies `lld-sparc/lld-shave.patch`).
  See `llvm-14/TARGET_NOTES.md`, `llvm-14/BUILD_FIXES.md`, and
  `npunlock-work/lld-sparc/BUILD_NOTES.md`.
- Derived from the r2gvp04w MoviTools reverse-engineering effort; this root
  wires our own builds together and redistributes no vendor binaries except
  the flagged proprietary `lib/mlibm.a` (required link input, kept out of
  source repos).

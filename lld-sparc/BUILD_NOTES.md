# BUILD_NOTES.md — building the patched lld 14 (stream B)

## 0. Deliverables in this directory

| file | purpose |
|------|---------|
| `RELOC_NOTES.md` | relocation semantics (evidence + numeric validation) |
| `LAYOUT_SPEC.md` | byte-level gold layout, banner delta, gc/flags/script audit |
| `lld-shave.patch` | unified diff vs `/home/asdf/projects/llvm-14/lld` (7 files) |
| `validate_layout.py` | gold layout checker (exit 0 on gold; `--banner` predicts our layout) |
| `BUILD_NOTES.md` | this file |

## 1. Applying the patch

```sh
cd /home/asdf/projects/llvm-14
patch -p1 < /home/asdf/projects/npunlock-work/lld-sparc/lld-shave.patch
```

Already verified here: `patch -p1 --dry-run` → *DRYRUN_OK* for all 7 files
(as of sha256 `76652a2bb1012db10c4e332267e3677fa75d4416934097aa2f39766c936ee0b2`
of the patch — regenerate the checksum after any edit).

Files touched:

```
lld/ELF/Arch/SPARC32.cpp   (NEW ~330 lines: target + SHAVE reloc types 200-210)
lld/ELF/CMakeLists.txt     (+ Arch/SPARC32.cpp)
lld/ELF/Target.h           (+ getSPARC32TargetInfo decl)
lld/ELF/Target.cpp         (+ case EM_SPARC)
lld/ELF/Driver.cpp         (+ parseEmulation .Case("elf32-sparc", ELF32LE, EM_SPARC))
lld/ELF/ScriptParser.cpp   (+ parseBfdName  .Case("elf32-sparc", ELF32LE, EM_SPARC))
lld/ELF/SyntheticSections.cpp (banner: "Linker: npunlock shave-lld v0.1.0\nbased on " + getLLDVersion())
```

`EM_SPARC(2)` already exists in `llvm/include/llvm/BinaryFormat/ELF.h:135`;
`ELF32LEKind` already exists.  No LLVM-side changes are required.

## 2. Build

The tree already has a configured build dir (do **not** configure a second
one; keep jobs ≤ 12, Release, one tree — machine rules):

```sh
# configure — ALREADY DONE; only re-run if build/ is wiped:
cmake -G Ninja -S /home/asdf/projects/llvm-14/llvm -B /home/asdf/projects/llvm-14/build \
      -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON \
      -DLLVM_ENABLE_PROJECTS="clang;lld" -DLLVM_TARGETS_TO_BUILD=X86

# build the linker
ninja -C /home/asdf/projects/llvm-14/build lld          # -> build/bin/lld
# narrower alternatives:
#   ninja -C build lldELF        (library where all patched files live)
#   ninja -C build lld-options   (tablegen for ELF/Options.inc; needed before Driver.cpp compiles)
```

Binary: `/home/asdf/projects/llvm-14/build/bin/lld`
(the npunlock native stage will invoke it as `moviLLD`/`shave-lld`).

## 3. Pre-verified compile status (done by LinkerRelocRE, read-only)

Using the exact `compile_commands.json` command lines with `-fsyntax-only`:

| file | status |
|------|--------|
| `lld/ELF/Arch/SPARC32.cpp` (new) | **clean** (no warnings with `-Werror=bitwise-op-parentheses`) |
| `lld/ELF/Target.cpp` | **clean** |
| `lld/ELF/SyntheticSections.cpp` | **clean** |
| `lld/ELF/Driver.cpp`, `lld/ELF/ScriptParser.cpp` | not pre-checkable: `lld/ELF/Options.inc` (tablegen) is not generated in the current build dir yet.  Their edits are single-line `.Case(...)` insertions adjacent to existing cases; they compile with the first `ninja lld` (tablegen runs automatically). |
| `lld/ELF/CMakeLists.txt`, `Target.h` | one added line each; dry-run patch OK |

Note: `-fsyntax-only` was run with the *patched* headers via shadow include
paths; nothing in `/home/asdf/projects/llvm-14` was modified before the
patch is applied by stream B.

## 4. Smoke validation (after building)

### 4.1 Layout — link any SHAVE object with the kernel script

```sh
LD=/home/asdf/projects/llvm-14/build/bin/lld
cd /home/asdf/projects/npunlock-work/npunlock
$LD -flavor gnu -EL -e _EP_setheap -z max-page-size=0x10 --gc-sections \
    -T src/shavecc/shave_kernel.ld \
    /tmp/shave_objs/mlibcrt_mini/shave_malloc.o -o /tmp/smoke.elf
readelf -hWlS /tmp/smoke.elf      # expect EM_SPARC, ET_EXEC, phnum 2, .text at 0x1d000000
```

Then (once npunlock produces a real kernel object) reproduce the gold test:

```sh
# same argv as shavecc.c:312 but with -T instead of the input-buffer script
python3 /home/asdf/projects/npunlock-work/lld-sparc/validate_layout.py            # gold: exit 0
python3 /home/asdf/projects/npunlock-work/lld-sparc/validate_layout.py \
        --banner $'Linker: npunlock shave-lld v0.1.0\nbased on LLD 14.0.0'
cmp -n $((0x180)) tests/fixtures/npu3720/add1-fp16.elf /tmp/kernel.elf   # byte-exact prefix
```

### 4.2 Relocations — reproduce the RELOC_NOTES validation

```sh
# link a function that contains all three validated reloc kinds:
LD ... -e __ieee754_sqrtf ... /tmp/shave_objs/mlibm/ef_sqrt.o -o /tmp/sqrt.elf
# then verify: for every .rela.text.__ieee754_sqrtf entry
#   type 209:  LE16(out) == (LE16(raw) + (A - r_offset)) & 0xffff
#   type 202/200: LE16 pairs reconstruct S+A (see RELOC_NOTES.md §4)
```

Expected: 0 mismatches (formula table in `RELOC_NOTES.md` §3–§4).

## 5. Risks / open points

1. **`-m elf32-sparc` without a script**: upstream defaults emulation when
   neither `-m` nor `OUTPUT_FORMAT` is given; always pass `-T` (contract
   does) or `-m elf32-sparc`.
2. **Standard-SPARC dead paths** in `SPARC32.cpp::relocate` follow
   SPARCV9 formulas; the vendor's dead-path cases differ slightly in
   bit-preservation details (RELOC_NOTES §3.3).  Unreachable from MoviTools
   inputs (no type < 200 is ever emitted).
3. **PLT**: SHAVE PLT emission was vendor-specific (decompiled
   `FUN_1802c6060`/`FUN_1802c60e0` build 16/32-byte SHAVE entries) but is
   only reachable with dynamic symbols — out of scope for static kernel
   links; `SPARC32` does not override `writePlt` (lld only fills a PLT when
   PLT entries exist).
4. **Banner fixture**: full-ELF byte-compare against the 828-byte gold
   fixture changes (LAYOUT_SPEC §6, −7 B comment / −8 B file).  Decide with
   main: prefix-compare (`cmp -n 0x180`) vs regenerated fixture.
5. **`--strip-debug`**: see LAYOUT_SPEC §7.4 (gold unaffected).

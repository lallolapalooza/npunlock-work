# E2E report — native SHAVE toolchain root vs vendor gold (wave 2)

Toolchain root: `/home/asdf/projects/npunlock-work/shave-toolchain-root`
Consumed by npunlock's native stage (`src/shavecc/movi_stage_native.c`) via
`NPUNLOCK_SHAVE_TOOLCHAIN_DIR`.
Gold: `npunlock/tests/fixtures/npu3720/add1-fp16.elf` (828 bytes,
sha256 `3f9d52273870c2e9…`, layout per `lld-sparc/LAYOUT_SPEC.md`).
Our linked output: `/tmp/gate/out.elf` (2020 bytes,
sha256 `4b639ef1a14ff62f…`, reproducible byte-for-byte across reruns).

## 1. Toolchain root under test

| binary | role | implementation |
|---|---|---|
| `bin/moviCompile` | C stdin → SHAVE .s stdout | shell → `llvm-14/build/bin/clang` (`-cc1 -triple shave -target-cpu 3720xx -S -O2 -x c`, `entry.c`→`-` + `-main-file-name entry.c`) |
| `bin/moviAsm` | .s stdin → ELF32 REL stdout | shell → `python3 shave-isa/shave_asm.py obj --cv 3720xx --noSPrefixing` (AsmObjWriter) |
| `bin/moviLLD` | objects → linked ELF stdout | shell → `llvm-14/build/bin/ld.lld` (lld-shave.patch), `-o -` rewritten to mktemp + cat + EXIT trap |
| `lib/mlibm.a` | archive link input | 922,244 B copy of MVC_DEPEND vendor archive (not committed to any repo) |

Wrapper edge paths exercised: moviCompile success/`-DX=5`/error-diagnostic
(exit 1, stderr passthrough; diagnostics prefix `<stdin>` where vendor said
`entry.c` — cosmetic, `.file "entry.c"` is preserved in output); moviLLD
success (byte-identical to direct ld.lld), with/without `mlibm.a` in argv,
failure path (exit 1, empty stdout, `ld.lld: error: …` on stderr), `$TMPDIR`
honoured, temp always removed; moviAsm contract argv → object on stdout.

## 2. lld + wiring evidence (pre-gate)

- lld patch: `patch -p1` applied clean to wave-1 tree (7 files, patch sha256
  `76652a2b…` unchanged from BUILD_NOTES, **no conflicts → no BUILD_FIXES
  entries**); `ninja -C build lld` green; `ld.lld -flavor gnu --version` →
  `LLD 14.0.6 (compatible with GNU linkers)`.
- Link smoke with vendor-derived object (`/tmp/shave_objs/mlibm/ef_sqrt.o`)
  through `bin/moviLLD` with the full contract argv: 6624-byte ELF32 LE
  ET_EXEC EM_SPARC, e_entry 0x1d000000, phnum 2, PT_LOAD align 0x10 —
  byte-identical to a direct ld.lld run, byte-identical with and without
  `mlibm.a` in argv.
- First gated run before the assembler landed proved the plumbing: compile
  stage green, failure localized to `shavecc.assemble` with
  `shave_asm: invalid choice: 'obj'`.
- Layout validator baseline: `validate_layout.py` exit 0 on gold;
  `--banner $'Linker: npunlock shave-lld v0.1.0\nbased on LLD 14.0.6'`
  (54 B incl. NUL vs vendor 61 B) predicts comment=384 (0x180, unchanged),
  symtab=440, shstrtab=472, strtab=524, shoff=540, filesize=820 vs gold 828.

## 3. Manual E2E link gate (add1-fp16) — PASS on structure

Pipeline (all through the root's binaries, exact npunlock contract argv):

```sh
bin/moviCompile -cc1 -triple shave -target-cpu 3720xx -S -O2 -x c entry.c \
    -ffunction-sections -fdata-sections -o - \
    < tests/fixtures/npu3720/add1-fp16.c > /tmp/gate/add1.s      # 393 lines, rc=0
bin/moviAsm --cv 3720xx --noSPrefixing < /tmp/gate/add1.s > /tmp/gate/add1.o  # 1744 B, rc=0
bin/moviLLD -flavor gnu -EL -e controlled_act -z max-page-size=0x10 \
    --gc-sections lib/mlibm.a -T src/shavecc/shave_kernel.ld \
    /tmp/gate/add1.o -o - > /tmp/gate/out.elf                     # 2020 B, rc=0
```

Object: ELF32 LE ET_REL EM_SPARC, `.text.controlled_act` PROGBITS AX align 16,
size 1378; symtab = NULL + GLOBAL FUNC `controlled_act`; no relocation
sections (all branches resolved intra-section).

Linked output (`readelf -a` → `/tmp/gate/our_readelf.txt`):
ET_EXEC, ELF32 LE, EM_SPARC, e_entry 0x1d000000, phnum 2 (PT_LOAD +
PT_GNU_STACK), shnum 7, shstrndx 5, `.arg.data` @0x1e000000 size 0,
`.text` @0x1d000000 off 0xc0 size 0x570 (1392), `.comment` size 0x36 (54),
symtab = NULL + `controlled_act` (st_value 0x1d000000, st_size 1378,
GLOBAL FUNC .text).

**npunlock's own parser verdict: PASS** — `npunlock_parse_shave_elf` returns
`status=ok`, image `offset=0xc0 address=0x1d000000 entry=0x1d000000`
(standalone throwaway harness against `build/e2e/libnpunlock.so`; the same
call inside `shavecc_compile` also passed — see §4).

Inline-header case (test compile #3 source, `#line`-expanded as shavecc does)
through the same pipeline: asm rc=0, link rc=0 (2020 B ELF), parser
**status=ok**, symtab NULL + `controlled_act`. All three shavecc stages are
proven end-to-end for both test sources.

Determinism: full pipeline re-run → `/tmp/gate/outb.elf` byte-identical to
`/tmp/gate/out.elf`; `.s` byte-identical (so the test's internal ELF equals
the captured one).

### validate_layout.py on our output (captured: /tmp/gate/validate_out.txt)

12 of 44 checks FAIL — every failure accounted for, **all structural checks
pass**:

| check | result | class |
|---|---|---|
| e_ident / e_type / e_machine / e_version / e_entry=0x1d000000 | ok | — |
| phnum=2, shnum=7, shstrndx=5, e_flags, phdrs @0x34 | ok | — |
| header reservation 0xb4, .arg.data@0xb4, zero pad [0x74,0xb4), 2-of-4 phdr survival | ok | — |
| PT_LOAD off 0xc0 vaddr 0x1d000000, flags R+X, align 0x10, covers .text exactly | ok | — |
| PT_GNU_STACK all fields/flags/align | ok | — |
| section order NULL .arg.data .text .comment .symtab .shstrtab .strtab | ok | — |
| shdr NULL + `.arg.data` byte-exact (incl. addr 0x1e000000, size 0) | ok | — |
| `.shstrtab`/`.strtab` contents exact; symtab = exactly 2 entries; `.strtab`=`\0controlled_act\0` | ok | — |
| file sha256 vs gold | FAIL | consequence of all deltas below |
| e_shoff 0x6cc vs 0x224 | FAIL | derived (tail follows .text end + banner) |
| PT_LOAD filesz==memsz==0xc0 vs 0x570 | FAIL | ISel (code size) |
| shdr `.text` size 192 vs 1392 | FAIL | ISel |
| shdr `.comment` offset 384→1632 (ISel-derived), size 61→54 (banner) | FAIL | ISel + banner |
| shdr `.symtab`/`.shstrtab`/`.strtab` offsets (448/480/532 → 1640/1672/1724) | FAIL | derived |
| `.comment` content = ours (`npunlock shave-lld v0.1.0\nbased on LLD 14.0.6`) vs vendor (`Movidius Linker (moviLLD) v3.0.9\nbased on LLD 14.0.0`) | FAIL | banner (LAYOUT_SPEC §6, expected) |
| sym[1] st_size 1378 vs 187 (value/bind/type/section all match) | FAIL | ISel |
| sha256(bytes[0,0x180)) and sha256(.text) | FAIL | ISel (region contains .text + filesz/e_shoff fields) |

**Offset-model proof (no structure bug):** feeding the LAYOUT_SPEC offset
model our two known deltas reproduces every observed offset exactly:
comment = 0xc0+0x570 = 0x630 ✓; symtab = align4(0x630+54) = 0x668 ✓;
shstrtab = 0x668+32 = 0x688 ✓; strtab = 0x688+52 = 0x6bc ✓;
shoff = align4(0x6bc+16) = 0x6cc ✓; filesize = 0x6cc+7×40 = 2020 ✓
(observed). The layout engine behaves identically to gold given the same
inputs; only `.text` size and banner length differ.

### Byte diff vs gold

- `cmp -n 0x180 out.elf gold` → differs at byte 33 (1-based) = file offset
  **32** = `e_shoff[0]` (0xcc vs 0x24); then PT_LOAD filesz/memsz fields
  (0x44/0x48: 0x570 vs 0xc0); then all of `.text` [0xc0,0x180).
- `cmp -l` (first 828 B): **574 of 828 bytes differ**. Full dumps:
  `/tmp/gate/our_full.xxd`, `/tmp/gate/gold_full.xxd`,
  `/tmp/gate/our_readelf.txt`, `/tmp/gate/gold_readelf.txt`.

## 4. Gated ctest run

```sh
cmake -S . -B build/e2e -DCMAKE_BUILD_TYPE=Release -DNPUNLOCK_BUILD_TESTS=ON \
    -DNPUNLOCK_ENABLE_SHAVE_TOOLCHAIN_TESTS=ON …
NPUNLOCK_SHAVE_TOOLCHAIN_DIR=/home/asdf/projects/npunlock-work/shave-toolchain-root \
    ctest --test-dir build/e2e -R shave_add1 --output-on-failure
```

Result (**recorded, expected RED**):

```text
1/1 Test #15: shave_add1 .......................***Failed    0.46 sec
linked ELF differs from the retained repeatable build (2020 versus 828 bytes)
```

Interpretation: `shavecc_compile` itself **succeeded end-to-end** — compile,
assemble, link, and the internal `npunlock_parse_shave_elf` call all passed
(the failure message is the final in-memory byte-compare, which runs only
after `status == NPUNLOCK_STATUS_OK`). The test stops at compile #1's
byte-compare, so compiles #2/#3 are not reached by ctest; both were verified
manually in §3. Raw log: `/tmp/gate/ctest_run.txt`.

## 5. Difference classification (complete)

| # | difference | category | verdict |
|---|---|---|---|
| 1 | `.text` 192 B (st_size 187) vs 1392 B (st_size 1378); `.text` bytes all differ; PT_LOAD filesz/memsz 0xc0 vs 0x570 | **instruction selection** | expected gap: wave-1 clang ISel doesn't yet match vendor (no VLIW slot-pack match, LDIL-only constant materialization, branch/delay-slot folding — see llvm-14/TARGET_NOTES.md wave-1 encoding note: our raw encode 1682 B vs gold 192 B; assembler bundling brought it to 1392 B). Deferred to ISel iteration. |
| 2 | `.comment` 61 B vendor banner vs 54 B ours; tail offsets −8 B when .text equal | **banner** | expected per LAYOUT_SPEC §6 (documented; byte-identical [0,0x180) still holds for banner alone — see validator `--banner` model: gold-sized .text → filesize 820 vs 828). |
| 3 | e_shoff, `.comment`/`.symtab`/`.shstrtab`/`.strtab` offsets, file size 2020 vs 828 | **derived from 1+2** | not a bug: LAYOUT_SPEC offset model reproduces all of them exactly (§3 proof). |
| 4 | whole-file sha256, `sha256(bytes[0,0x180))` | **consequence of 1** | region [0,0x180) contains .text + size-carrying header fields. |
| 5 | everything else (structure/conventions) | **match** | validator structural checks all ok; npunlock parser `status=ok`. |

**Verdict: no layout/structure bug.** The only two root causes are the known
instruction-selection gap (class 1, fixes 3+4) and the known banner delta
(class 2). The gated `shave_add1` byte-compare will stay red until ISel
closes; everything npunlock itself validates (ELF structure) passes now.

## 6. Provenance

Native builds only: LLVM/clang/lld from `/home/asdf/projects/llvm-14` (wave-1
fork + `lld-sparc/lld-shave.patch`), assembler from `npunlock-work/shave-isa`
(reference implementation derived from the r2gvp04w RE effort), wrappers are
small shell scripts (source in `shave-toolchain-root/bin/` + README). Sole
vendor binary: `lib/mlibm.a` (proprietary link input, kept outside all repos).
No vendor code redistributed in llvm-14, shave-isa, or npunlock.

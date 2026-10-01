# LAYOUT_SPEC.md — byte-level layout of moviLLD output (gold = add1-fp16.elf)

Goal: reproduce `npunlock/tests/fixtures/npu3720/add1-fp16.elf`
(828 B, sha256 `3f9d52273870c2e911000da3c3bd474c0514bd297d29539a14e935b2b01de6d5`)
byte-for-byte with patched upstream lld 14, except for the `.comment` banner
(§6 quantifies the delta).

Everything below is validated by `validate_layout.py` (44 checks, exit 0 on
gold).  Upstream lld references are to `/home/asdf/projects/llvm-14/lld`
(`release/14.x`, f28c006a5) — the same base the vendor forked.

---

## 1. Gold file map

```
off      size  content
0x000    52    ELF header (see §2)
0x034    64    phdr[0] PT_LOAD  @0x034, phdr[1] PT_GNU_STACK @0x054
0x074    180   zero fill (0x74 .. 0xb4)          <- header reservation, see §3
0x0b4    0     .arg.data (empty section, offset only)
0x0c0    192   .text  = controlled_act machine code (sha256 bfe23abd677c5594c2956d1912f6d87aa87c62a6119bd8c85ab2a5fa08aaffab)
0x180    61    .comment = "Linker: Movidius Linker (moviLLD) v3.0.9\nbased on LLD 14.0.0\0"
0x1bd    3     pad to 4
0x1c0    32    .symtab  (2 entries: NULL + controlled_act)
0x1e0    52    .shstrtab = "\0.arg.data\0.text\0.comment\0.symtab\0.shstrtab\0.strtab\0"
0x214    16    .strtab = "\0controlled_act\0"
0x224    280   section headers (7 × 40)
0x33c          EOF (828)
```

ELF header (all values little-endian):

| field | value |
|-------|-------|
| e_ident | `7f 45 4c 46 01 01 01 00 00…` (ELF32, LE, System V) |
| e_type / e_machine / e_version | ET_EXEC(2) / EM_SPARC(2) / 1 |
| e_entry | 0x1d000000 (== `-e controlled_act`) |
| e_phoff / e_shoff | 52 / 548 (0x224) |
| e_flags | 0 |
| e_ehsize / e_phentsize / e_phnum | 52 / 32 / 2 |
| e_shentsize / e_shnum / e_shstrndx | 40 / 7 / 5 |

Section headers (order is the shdr order; shstrtab names follow it exactly):

| # | name | type | flags | addr | off | size | link | info | align | entsize |
|---|------|------|-------|------|-----|------|------|------|-------|---------|
| 0 | — | NULL | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 1 | .arg.data | PROGBITS | A(2) | 0x1e000000 | 0xb4 | 0 | 0 | 0 | 1 | 0 |
| 2 | .text | PROGBITS | A+X(6) | 0x1d000000 | 0xc0 | 0xc0 | 0 | 0 | 16 | 0 |
| 3 | .comment | PROGBITS | M+S(0x30) | 0 | 0x180 | 0x3d | 0 | 0 | 1 | 1 |
| 4 | .symtab | SYMTAB | 0 | 0 | 0x1c0 | 0x20 | 6 | 1 | 4 | 16 |
| 5 | .shstrtab | STRTAB | 0 | 0 | 0x1e0 | 0x34 | 0 | 0 | 1 | 0 |
| 6 | .strtab | STRTAB | 0 | 0 | 0x214 | 0x10 | 0 | 0 | 1 | 0 |

Program headers:

```
phdr[0] PT_LOAD    off 0xc0  vaddr=paddr 0x1d000000  filesz=memsz 0xc0  flags R+X(5)  align 0x10
phdr[1] PT_GNU_STACK (0x6474e551)  off=vaddr=paddr=filesz=memsz=0     flags RW(6)    align 0
```

## 2. Input → output (the invocation being reproduced)

Vendor invocation (npunlock `src/shavecc/shavecc.c:312`):

```
moviLLD -flavor gnu -EL -e controlled_act -z max-page-size=0x10 --gc-sections \
        [toolchain/lib/mlibm.a] [script as input buffer] <entry.o> -o -
```

Replacement invocation (documented contract):

```
shave-lld -flavor gnu -EL -e controlled_act -z max-page-size=0x10 --gc-sections \
          -T shave_kernel.ld [lib/mlibm.a] <entry.o> -o out.elf
```

Script: `npunlock/src/shavecc/shave_kernel.ld` — byte-identical to the
MoviTools-side script shipped in npu_compiler
(`sw_runtime_kernels/kernels/prebuild/shave_kernel.ld`), i.e. the gold file
was produced with exactly this script.  Audit in §7.

## 3. Header reservation: why the first section starts at 0xb4

Pure upstream-lld-14 behaviour, no vendor change needed:

1. `Writer::finalizeSections` creates the phdr list
   (`lld/ELF/Writer.cpp:2044-2056`) from `createPhdrs()`
   (`Writer.cpp:2263`):
   * `PT_PHDR` (2284) + `PT_LOAD` covering `elfHeader`/`programHeaders`
     (2285-2298); `.arg.data` (flags A→R) merges into that first load
     (2345-2355, `load->lastSec == Out::programHeaders`),
   * `PT_LOAD` for `.text` (flags A+X → R+X) — new segment (2354),
   * `PT_GNU_STACK` (2395).
   ⇒ **4 planned phdrs**, and `Out::programHeaders->size = 4 × 32 = 128`
   (`Writer.cpp:2056`).
2. `Script::allocateHeaders` (`LinkerScript.cpp:1245-1276`) with
   `hasSectionsCommand=true` computes `headerSize = 52+128 = 180` and
   `computeBase(min, false) = alignDown(0x1e000000, maxPageSize=0x10)
   = 0x1e000000` (`LinkerScript.cpp:1227`), so the fit test
   `180 <= 0x1e000000 − 0x1e000000` **fails** ⇒ headers are *not* placed in
   the image: `elfHeader->ptLoad = programHeaders->ptLoad = nullptr`,
   the first PT_LOAD's `firstSec` becomes `.arg.data`, and `PT_PHDR` is
   erased (`LinkerScript.cpp:1267-1275`).
3. `removeEmptyPTLoad` (`Writer.cpp:102-119`) drops the now-empty
   `.arg.data` PT_LOAD (size 0) and clears `.arg.data->ptLoad`.
   ⇒ **e_phnum = 2** (the two surviving phdrs), but the file-offset
   computation still starts after `Out::elfHeader->size +
   Out::programHeaders->size = 52 + 128 = 180 = 0xb4`
   (`assignFileOffsets`, `Writer.cpp:2560-2563`).
4. `computeFileOffset` (`Writer.cpp:2503-2527`):
   * `.arg.data`: `ptLoad == nullptr` → `alignTo(0xb4, sh_addralign=1)
     = 0xb4`, size 0 (offset recorded, no bytes consumed);
   * `.text`: first section of its PT_LOAD →
     `alignTo(0xb4, p_align, vaddr)` with `p_align = max(section
     alignments) = 16` (`PhdrEntry::add`, `Writer.cpp:1011`) and
     congruence `off ≡ vaddr (mod 16)` → **0xc0**; `.text` ends at 0x180;
   * non-alloc sections are laid out after alloc ones, each at
     `alignTo(off, alignment)` (`Writer.cpp:2594-2598`):
     `.comment`(1) → 0x180, `.symtab`(4) → 0x1c0, `.shstrtab`(1) → 0x1e0,
     `.strtab`(1) → 0x214;
   * `sectionHeaderOff = alignTo(0x224, wordsize=4) = 0x224`
     (`Writer.cpp:2593`), `fileSize = 0x224 + 7×40 = 828`.
5. `setPhdrs` (`Writer.cpp:2616+`) fills PT_LOAD from `.text`
   (off 0xc0, vaddr 0x1d000000, filesz=memsz=0xc0, flags 5, align 16);
   PT_GNU_STACK gets flags RW and no fields (align stays 0,
   `Writer.h:42` default).
   Bytes between emitted structures are zero (lld zero-fills its output
   buffer).

**Sanity cross-check:** an unrelated MoviTools product (npu_compiler LFS
prebuilt `activation_tanh.3720xx.elf`, linked by *moviLLD v5.0.0 / LLD
19.1.7*) has the identical skeleton: phoff 52, phnum 2 (PT_LOAD R+X
align 0x10 + GNU_STACK), `.arg.data@0x1e000000 off 0xb4 size 0`,
`.text@0x1d000000 off 0xc0`, `.comment off 0x4d0 size 0x3d`,
`.symtab/.shstrtab/.strtab`.  The layout algorithm is stable across vendor
versions ⇒ no vendor layout patch exists to replicate; upstream lld 14
already *is* the algorithm.

## 4. Section-selection rules (why gold has exactly these sections)

* `.arg.data` / `.text` come from the script's `SECTIONS` command; a
  script-created output section is kept even when empty (gold:
  `.arg.data` size 0 at 0x1e000000).
* `.text` content = `*(.text*)` + ALIGN(16) + `*(.rodata*)` + … — in gold
  only `entry.o`'s `controlled_act` section survives (see §5 for gc +
  archives).
* `.comment` is **always** added for non-relocatable output
  (`lld/ELF/Driver.cpp:2499-2501` → `createCommentSection`,
  `SyntheticSections.cpp:85`, flags `SHF_MERGE|SHF_STRINGS`, entsize 1).
* `.symtab` (info = index of first global = 1, link = .strtab index = 6),
  `.shstrtab`, `.strtab` are added in that order
  (`Writer.cpp:514-522`).
* No `.note.GNU-stack`, no `.dynamic`, no `.rela.*` (static, non-PIE,
  no `-r`, no `--emit-relocs`).

## 5. Symbol table rules (why null + controlled_act only)

Upstream lld 14, no vendor deviation:

* Section symbols are never copied to the output symtab
  (`Writer::shouldKeepInSymtab`, `Writer.cpp:640-642`: `sym.isSection() →
  false`); they are only added when `-r`/`--emit-relocs`
  (`Writer::run`, `Writer.cpp:530-531`).
* `STT_FILE` symbols are stored as `sourceFile` and, being SHN_ABS-local,
  become `Undefined` local symbols that `copyLocalSymbols` drops
  (`InputFiles.cpp:1053`, `Writer.cpp:696-714`).
* Other locals would be kept — gold has none, so `entry.c` compiled to a
  single global function; `--gc-sections` additionally excludes locals
  whose section died (`includeInSymtab`, `Writer.cpp:675-690`).
* Globals: `controlled_act` — st_value 0x1d000000, st_size 187
  (moviCompile emitted `.size`), st_info 0x12 (GLOBAL FUNC), st_shndx 2.

## 6. `.comment` banner — decision and byte delta

Vendor string (gold, v3.0.9; built by `FUN_18025e860` at
decompiled_all.c:461568 + `FUN_1816841e0` at 4339872):

```
"Linker: Movidius Linker (moviLLD) v3.0.9\nbased on LLD 14.0.0" + NUL   = 61 B (0x3d)
```

upstream hook: `lld/ELF/SyntheticSections.cpp:75`
(`saver().save(Twine("Linker: ") + getLLDVersion())`), env override
`LLD_VERSION` preserved.

**Decision (patched lld):**

```
"Linker: npunlock shave-lld v0.1.0\nbased on LLD 14.0.0" + NUL          = 54 B (0x36)
```

truthful (we *are* lld 14.0.0-derived; `getLLDVersion()` supplies the
`LLD 14.0.0` tail), keeps the vendor's `\n`-separated two-line shape.
Delta = **−7 bytes**.

Layout with our banner (model: `validate_layout.py --banner …`):

| item | gold | ours (banner −7) | shift |
|------|------|------------------|-------|
| .comment offset | 0x180 | 0x180 | 0 |
| .comment size | 0x3d (61) | 0x36 (54) | −7 |
| .symtab offset | 0x1c0 | 0x1b8 | −8 |
| .shstrtab offset | 0x1e0 | 0x1d8 | −8 |
| .strtab offset | 0x214 | 0x20c | −8 |
| e_shoff | 0x224 (548) | 0x21c (540) | −8 |
| file size | 828 | 820 | −8 |

What stays **byte-exact** with our banner:

* `[0x000, 0x180)` — ELF header, both phdrs, zero reservation gap, and the
  whole `.text` (sha256 `5cac6e472348e458e90ce71b6c2156cb0c004a7f464b95396fcb8245a9bd0634`
  of that range; `.text` itself `bfe23abd…`).  ⇒
  `cmp -n $((0x180)) gold ours` succeeds; `.text` hex-compares succeed;
  `controlled_act` symbol bytes identical.
* `.shstrtab`, `.strtab`, `.symtab`, section-header *contents* are
  identical (only their file offsets move).

What needs **fixture regeneration**:

* full-file `sha256`/`cmp` of the 828-byte gold ELF,
* `e_shoff` in the header, and every section header's `sh_offset` from
  `.symtab` onward,
* the `.comment` bytes themselves.

npunlock's byte-compare test (`tests/integration/test_shavecc.c`,
`NPUNLOCK_ENABLE_MOVITOOLS_TESTS`) must therefore either (a) compare only
`[0,0x180)` + section contents, or (b) get a regenerated full-ELF fixture
once, generated by the patched lld itself.  Decide with the main agent;
this doc's model makes either choice mechanical
(`validate_layout.py --banner "Linker: npunlock shave-lld v0.1.0\nbased on LLD 14.0.0"`).

## 7. Flags & script audit (Q3, Q4)

### 7.1 `--gc-sections` with archives

Vendor behaviour = **upstream lld 14 `MarkLive.cpp`**, no deviation found:

* option texts identical to upstream (`moviLLD64.ascii.txt`:
  `--gc-sections`, `--print-gc-sections`, `-r and --gc-sections may not be
  used together` — upstream `Driver.cpp:1338-1341`; `Dump garbage
  collector data` — `Options.td`; `-z nostart-stop-gc` message —
  `Relocations.cpp:781`).
* Roots (`MarkLive.cpp:243-259`): `config->entry` (the `-e` symbol),
  `--init/--fini`, `-u` symbols, script-referenced symbols; plus
  `SHF_GNU_RETAIN`; script `KEEP(...)` forces liveness; non-SHF_ALLOC
  sections (`.comment`, `.debug_*`) are never gc'ed; eh_frame special-cased.
* Archives stay **lazy**: members are extracted only when an undefined
  symbol requires them (`LinkerDriver::addFile`/`markLive` interplay,
  `Driver.cpp` archive handling).  Observed: gold links `mlibm.a` yet
  `.text` contains only `controlled_act` (no member was ever needed); the
  prebuilt kernels pull exactly the CRT/C++ pieces they reference
  (e.g. `shaveCRT1.o`, throw helpers) and nothing else.
* Deviations from upstream: **none detected**
  `[INFERENCE — based on string identity of all gc diagnostics plus the
  observed keep/drop patterns in 376 linked ELFs]`.

### 7.2 Flag-by-flag

| flag | vendor support | upstream lld 14 handling | note |
|------|----------------|--------------------------|------|
| `-flavor gnu` | yes | `lld/Common/Args.cpp` (shared front end) | unchanged |
| `-EL` | yes | `Options.td:162 def el` → `config->optEL` (`Driver.cpp:1184`), only selects the *3rd* arg of `OUTPUT_FORMAT(a,b,little)` | harmless; no effect with our script |
| `-e <sym>` | yes | entry symbol; also a gc root | unchanged |
| `-z max-page-size=0x10` | yes | `Driver.cpp` `getMaxPageSize` → `config->maxPageSize` | drives `alignTo` congruence; with `.text` align 16 the result is identical either way |
| `--gc-sections` | yes | §7.1 | unchanged |
| script as 2nd positional input (no `-T`) | yes | `LinkerDriver::addFile` → `case file_magic::unknown: readLinkerScript(mb)` (`Driver.cpp` ~line 211) — stock lld treats a non-ELF input file as a linker script | npunlock POSIX path uses `-T` instead (`shavecc.c:319` comment); **both are stock lld**, vendor used the input-file form |

### 7.3 `shave_kernel.ld` syntax audit

| construct | standard? | lld-14 support |
|-----------|-----------|----------------|
| `OUTPUT_FORMAT("elf32-sparc")` | GNU ld standard | **vendor extension**: `parseBfdName` lacks `elf32-sparc` upstream (`ScriptParser.cpp:412-440`); the patch adds `.Case("elf32-sparc", {ELF32LEKind, EM_SPARC})`.  This is the *only* script-related vendor extension. (Vendor also patched `parseBfdName` — decompiled `FUN_180231c90` line 426893 — and *not* `parseEmulation`; the patch additionally adds `-m elf32-sparc` for convenience.) |
| `OUTPUT_ARCH(sparc)` | GNU ld standard | parsed and **ignored** (`ScriptParser.cpp:405-406`) |
| `SECTIONS { . = ADDR; name : { … } }` | standard | `LinkerScript.cpp` |
| `KEEP(*(.arg.data))` | standard | `ScriptParser.cpp:749-753` |
| `. = ALIGN(16);` | standard | `readAlign` |
| `*(.text*)` wildcards | standard | `readInputSectionSpec` |

No other vendor extensions.  (Both scripts — npunlock's and npu_compiler's
— are byte-identical, so there is exactly one script dialect in play.)

### 7.4 Debug sections caveat

`--gc-sections` never removes non-alloc `.debug_*`, so if a kernel pulls an
archive member that carries debug info (all MoviTools libs do), output gains
`.debug_*` sections.  MoviTools' own kernel link lines pass `--strip-debug`
(npu_compiler `kernels/CMakeLists.txt:417`); npunlock's shavecc line does
not.  Gold is unaffected (no extracted member, `entry.o` without `-g`).
Recommendation for NpunlockNativeStage: add `--strip-debug` for parity with
MoviTools kernel builds — behaviour identical either way on gold.

## 8. Version note

`activation_tanh.3720xx.elf` carries `.comment "…v5.0.0\nbased on LLD
19.1.7"` — the prebuilt kernels were linked by a *newer* moviLLD.  They are
used in RELOC_NOTES.md only to validate relocation formulas (identical fixup
engine — 0 failures over 600+ relocations), **not** for byte-layout
authority.  Layout authority is the v3.0.9 gold fixture plus the structural
identity of the v5 file (§3), which follows the same upstream-ld algorithm.

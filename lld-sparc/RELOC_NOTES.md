# RELOC_NOTES.md — moviLLD64 (moviLLD) SHAVE relocation semantics

Reverse-engineered from the proprietary MoviTools linker **moviLLD64.dll**
(LLVM/lld 14 fork, git rev `97ebc3a512d681dcfc6b2b626e09e2f2711ed54b`,
banner "Movidius Linker (moviLLD) v3.0.9 / based on LLD 14.0.0") and
validated numerically against MoviTools-linked SHAVE ELFs.

All decompile line numbers refer to
`r2gvp04w-re/re_decompiled/moviLLD64/decompiled_all.c`.
The DLL itself was only *read* (PE section extraction), never executed.

---

## 1. Relocation inventory (input objects)

ELF32 LE, `e_machine = EM_SPARC(2)`, **RELA** relocations only
(`.rela.*`, `Elf32_Rela`, entsize 12). Enumerated with `readelf -r` over
every member of every MoviTools SHAVE archive under
`extracted/code$GetExtractPath$/Driver/MVC_DEPEND/lib/`
(162 objects total; copy extracted to `/tmp/shave_objs/`):

| type | hex | total (all archives) | in mlibm.a | in mlibcxx | in mlibcrt | in mlibc_lite | in mlibc_lite_ext | in mlibcrt_mini |
|------|-----|----------------------:|-----------:|-----------:|-----------:|--------------:|------------------:|----------------:|
| 200  | 0xc8 | 4677 | 828 | 3474 | 30 | 238 | 104 | 3 |
| 201  | 0xc9 | 4    | 0   | 4    | 0  | 0   | 0   | 0 |
| 202  | 0xca | 4705 | 828 | 3498 | 30 | 242 | 104 | 3 |
| 203  | 0xcb | 4    | 0   | 4    | 0  | 0   | 0   | 0 |
| 208  | 0xd0 | 111124 | 18953 | 83649 | 3442 | 4287 | 716 | 77 |
| 209  | 0xd1 | 8880 | 1131 | 6665 | 296 | 739 | 38 | 11 |
| 210  | 0xd2 | 81   | 17 | 52 | 3 | 9 | 0 | 0 |

* 208 appears **only** in `.rela.debug_*` sections (section-symbol + addend).
* 200/202 always come in pairs 3 bytes apart in `.text`, same symbol, addend 0
  (or per-symbol absolute address).
* 209 targets are the *owning `.text.*` section symbol* with addend = target
  byte offset inside that section.
* 204/205/206/207 (see §3) are implemented by the vendor linker but never
  emitted by any MoviTools library; 211–214 are accepted by the vendor
  `getRelExpr` but rejected by its `relocate()` dispatch and never emitted.

## 2. Where the semantics live in the DLL

| what | decompiled symbol | address | lines |
|------|-------------------|---------|-------|
| `getRelExpr` (type → R_ABS/R_PC/…) | `FUN_1802c61e0` | 0x1802c61e0 | 540520–540770 |
| `relocate` (fixup) | `FUN_1802c6780` | 0x1802c6780 | 540808–541857 |
| fixup dispatch table | `UINT_1802c7324` (RVA 0x2c7324, file 0x2c6724) | — | read at 540914 |
| emulation/BFD name "elf32-sparc" | `FUN_180231c90` = `parseBfdName` | 0x180231c90 | 426893 |
| `.comment` banner builder | `FUN_18025e860` = `createCommentSection`/`getVersion` | 0x18025e860 | 461568–461660 ("Linker: " at 461624) |
| vendor version string | `FUN_1816841e0` | 0x1816841e0 | 4340270 ("\nbased on LLD "), 4339872 (header) |

The dispatch table is a byte array indexed by `type - 3`
(`bVar1 = *(byte*)(&UINT_1802c7324 + (rel.type - 3))`, line 540914).  Rows
relevant to this project (extracted from the DLL, `.text` section):

```
type:  3   6   7   9  10  12  13  15  16  17  18  23  32  34  35  36  50  51  52  54  72  73
case:  0   1   2   3   4   5   6   7   6   7   2   0   8   9  10   7  12  13  14   8  15  16
type: 200 201 202 203 204 205 206 207 208 209 210        (all others 3..210 not listed => 32 = default)
case:  17  18  19  20  21  22  23  24  25  26  27
```

`case 32`/default is unreachable in practice: `getRelExpr`'s `default:`
already errors with `"unknown relocation (" + type + ") against symbol " + name"`
(line 540598–540601, message format copied verbatim from upstream
`lld/ELF/Arch/SPARCV9.cpp:82`).

### 2.1 Value evaluation (S, A, P)

The vendor `getRelExpr` returns stock lld-14 `RelExpr` enum values
(`lld/ELF/Relocations.h:31-44`, numbering `R_ABS=0, R_GOT_OFF=4, R_NONE=11,
R_PC=12, R_PLT_PC=14, R_TPREL=28`).  Therefore the *value* passed to
`relocate()` is computed by **unmodified upstream**
`lld/ELF/InputSection.cpp::getRelocTargetVA` / `Relocations.cpp`:

* `R_ABS`  → `val = S + A`
* `R_PC`   → `val = S + A - P` where **`P` = virtual address of the
  relocation site itself** (`sec->getVA(r_offset)`), no shift, no rounding.

Exact vendor grouping (lines 540717–540765):

```
R_NONE (0xb) : { 0 }
R_ABS  (0)   : { 3, 9, 12, 23, 32, 34, 35, 36, 50, 51, 52, 54,        // SPARC_32 HI22 LO10 UA32
                                                              64 HH22 HM10 LM22 H44 M44 L44 UA64
                 200, 201, 202, 203, 206, 207, 208, 211, 212, 213, 214 }
R_PC   (0xc) : { 6, 7, 16, 17, 204, 205, 209, 210 }                    // DISP32 WDISP30 PC10 PC22 + SHAVE
R_GOT_OFF(4) : { 13, 15 }                                              // GOT10 GOT22 (14=GOT13 NOT handled!)
R_PLT_PC(0xe): { 18 }                                                  // WPLT30
R_TPREL(0x1c): { 72, 73 }                                              // TLS_LE_HIX22/LOX10
default       : error "unknown relocation (N) against symbol ..."      // incl. 1,2,4,5,8,10,11,14,19..31,33,37..49,53,55..199
```

Note: standard `R_SPARC_WDISP22 (8)` and `R_SPARC_22 (10)` are **not**
handled by the vendor `getRelExpr` either (dead paths of its `relocate()`
exist for them, but the error fires first).  The MoviTools toolchain never
emits them.

## 3. Per-type fixup formulas (vendor `relocate`, `FUN_1802c6780`)

Notation: `loc` = output byte address of `r_offset`; all multi-byte reads
and writes are **little-endian** (host-order stores in the x64 DLL);
`w = *(uint32_le*)loc`, `h = *(uint16_le*)loc`; `val` = S+A or S+A−P per §2.1.

### 3.1 SHAVE types — numerically validated

#### type 200 (0xc8) — absolute low 16 bits  → patch: `R_SHAVE_LO16`

* expr `R_ABS` (540731).  dispatch: table[200] = 17 = `case 0x11`
  (541014) → shared label (541845–541847):
  ```
  *(uint16_le*)loc |= (uint16)val;
  ```
* **Formula:** `LE16(loc) ← LE16(loc) | ((S+A) & 0xffff)`.
  moviAsm zeroes the field (`00 00` observed in objects), so the result is
  simply the low half of the 32-bit absolute address.

#### type 202 (0xca) — absolute high 16 bits  → patch: `R_SHAVE_HI16`

* expr `R_ABS`.  dispatch: table[202] = 19 = `case 0x13` (541019):
  `val >>= 16; goto caseD_11`.
* **Formula:** `LE16(loc) ← LE16(loc) | ((S+A) >> 16)`.

**Why the pair sits 3 bytes apart.**  moviAsm64 emits a 32-bit absolute
address as `[HI16 LE][1 opcode/reg byte][LO16 LE]` inside the instruction:
`r_offset(ca) = x`, untouched byte at `x+2`, `r_offset(c8) = x+3`.
The linker reconstructs nothing — each half is an independent LE16 write.
Reconstructed address: `((uint16_t)mem[x]) << 16 | (uint16_t)mem[x+3]`.
This is the SHAVE analogue of an LDIL/LDIH pair.

#### type 209 (0xd1) — PC-relative branch displacement  → patch: `R_SHAVE_PCREL16`

* expr `R_PC` (540750) ⇒ `val = S + A − P`, `P` = site VA.
* dispatch: table[209] = 26 = `case 0x1a` (541213–541261):
  ```
  if (val & ~0xffff && val != ~0xffff...)  // isInt<16>
      error("target symbol outside of the +/- 32K bytes range " + <loc>);  // 541224
  *(uint16_le*)loc += (int16_t)val;                                       // 541259
  ```
* **Formula:** `LE16(loc) ← (LE16(loc) + (S+A−P)) mod 2^16`, range
  `val ∈ [−32768, +32767]` (else error).  **Read-modify-write (+=), not OR,
  not overwrite.**  For the usual intra-section branch (`S` = owning
  section start, `A` = target offset, `P` = section start + `r_offset`):
  `val = A − r_offset`, hence
  `final_field = old_field + A − r_offset`.

* **What moviAsm pre-encodes:** `old_field = r_offset − instruction_start`
  (small positive u16: 1, 5, 6, 7 … observed).  Therefore
  `final_field = A − instruction_start = target − instruction_start`:
  after the fixup the field is the byte displacement **from the start of the
  instruction** to the branch target — regardless of where inside the
  variable-length instruction the field sits.  Verified in §4.2.
* There is **no branch scaling** (no `>>2`, no alignment constraint on
  `r_offset`); SHAVE branch displacements are byte quantities.

#### type 210 (0xd2) — PC-relative, field at bit 4  → patch: `R_SHAVE_PCREL16_AT4`

* expr `R_PC`.  dispatch: table[210] = 27 = `case 0x1b` (541262–541306):
  same ±32 KiB check (541277), then
  ```
  *(uint32_le*)loc = (((w >> 4) + val) * 16 & 0xffff0) | (w & 0xfff0000f);   // 541317..541320
  ```
* **Formula:** 16-bit field at LE32 bits **[19:4]** `+= val`; bits [31:20]
  and [3:0] preserved.  `val = S+A−P`, range ±32 KiB.
* 81 instances in the libraries; never present in any linked artifact we
  could obtain → **decompile-only** (no numeric oracle).

#### type 208 (0xd0) — absolute LE32 (debug sections)  → patch: `R_SHAVE_ABS32`

* expr `R_ABS` (540737).  dispatch: table[208] = 25 = `case 0x19` (541211)
  → label `caseD_19` (540930): `*(uint32_le*)loc = (uint32)val;`
* **Formula:** plain little-endian 32-bit write, **no range check, no byte
  swap** (contrast: standard `R_SPARC_32` goes through `case 0` at 540921
  which byte-swaps — see §3.2).  Used exclusively against `.debug_*`
  sections with section-symbol+addend (111124 instances), i.e. DWARF
  data4/data8-style offsets that are little-endian by definition.
* No numeric oracle (MoviTools link lines pass `--strip-debug`:
  npu_compiler `sw_runtime_kernels/kernels/CMakeLists.txt:417`) →
  **decompile-only**, but the semantics are unambiguous.

### 3.2 SHAVE types implemented but never emitted — decompile-only

All exact, from the same dispatch:

| type | patch name | field | formula (per vendor) | evidence |
|------|-----------|-------|----------------------|----------|
| 201 (0xc9) | `R_SHAVE_LO16_AT4` | LE32 bits [19:4] | `w |= (val<<4) & 0xffff0` (val=S+A) | case 0x12 @541016 |
| 203 (0xcb) | `R_SHAVE_HI16_AT4` | LE32 bits [19:4] | `w |= (val>>12) & 0xffff0` (val=S+A; bits31:16) | case 0x14 @541022 |
| 204 (0xcc) | `R_SHAVE_PCREL14_AT2` | LE32 bits [15:2] | ±8 KiB check (`0x3fff`, msg @541043); `w = ((w>>2)+val)*4 & 0xfffc \| w & 0xffff0003` | case 0x15 @541025 |
| 205 (0xcd) | `R_SHAVE_PCREL14_AT6` | LE32 bits [19:6] | ±8 KiB check (@541114); `w = ((w>>6)+val)*64 & 0xfffc0 \| w & 0xfff0003f` | case 0x16 @541096 |
| 206 (0xce) | `R_SHAVE_WINDOW_AT3` | LE32 bits [18:3] | `(val & 0xfffe0000)` must be ∈ {0, 0x00ff0000, 0x1c000000, 0x1d000000, 0x1e000000, 0x1f000000} else error `"Invalid WINDOW target for 0x"` (@541166); `w = (w & 0xfff00007) \| (val<<3 & 0xffff8)` | case 0x17 @541151 |
| 207 (0xcf) | `R_SHAVE_WINDOW_AT7` | LE32 bits [22:7] | same base check else error `" relocation does overflow 0x"` (@541190); `w = (w & 0xff00007f) \| (val<<7 & 0xffff80)` | case 0x18 @541179 |
| 211–214 | accepted as `R_ABS` by getRelExpr, dispatch `32` = error | — | — | 540738–540741, table |

### 3.3 Standard SPARC types (dead paths for this project)

SHAVE objects never contain types < 200.  The vendor nevertheless kept a
SPARCV9-shaped `relocate()` (byte-**big**-endian writes, e.g. `case 0`
@540921 byte-swaps `val` before storing).  `lld-shave.patch` implements
these cases with the upstream `SPARCV9.cpp` formulas; they are documented
as **unvalidated** and are not reachable through MoviTools inputs.
Cross-check of dispatch vs. upstream formulas (all match SPARCV9 semantics):

```
case 0  (3, 23)  R_SPARC_32/UA32:    checkUInt32, write32be
case 1  (6)      R_SPARC_DISP32:     checkInt32,  write32be
case 2  (7, 18)  WDISP30/WPLT30:     (val>>2) & 0x3fffffff
case 3+4 (9,10)  HI22 / R_SPARC_22:  (val[>>10]) & 0x3fffff
case 5  (12)     LO10:               val & 0x3ff (13-bit field)
case 6  (13, 16) GOT10/PC10:         val & 0x3ff
case 7  (15,17,36) GOT22/PC22/LM22:  (val>>10) & 0x3fffff
case 8  (32, 54) R_SPARC_64 / UA64:  write64be
case 9  (34)     HH22:               (val>>42) & 0x3fffff
case 10 (35)     HM10:               (val>>32) & 0x3ff
case 11 (41)     WDISP19:            (val>>2) & 0x7ffff   [getRelExpr rejects 41]
case 12 (50)     H44:                (val>>22) & 0x3fffff
case 13 (51)     M44:                (val>>12) & 0x3ff
case 14 (52)     L44:                val & 0xfff
case 15 (72)     TLS_LE_HIX22:       (~val>>10) & 0x3fffff
case 16 (73)     TLS_LE_LOX10:       (val & 0x3ff) | 0x1c00
```

## 4. Numeric validation

Oracle artifacts (linked by MoviTools, obtained read-only):

1. `/tmp/opencode/linux-src/linux-npu-driver/...` carved firmware ELFs
   `fw_elf1.elf` / `fw_elf2.elf`; the latter is byte-identical to the inner
   SHAVE ELF carved from `.shaveAppElfs` of `fw_elf1.elf`
   (copy kept at `lld-sparc/inner_shave.elf`).  `.dyn.text` at
   `0x1c000000` (0xd80 bytes) with a full symbol table.
2. 375 prebuilt kernel ELFs from the npu_compiler LFS
   (`sw_runtime_kernels/{kernels,rt_kernel}/prebuild/**/*3720xx*.elf`,
   downloaded to `/tmp/prebuilt3720/`), linked by moviLLD v5.0.0
   (".comment: Linker: Movidius Linker (moviLLD) v5.0.0 / based on LLD 19.1.7")
   — same relocation engine, see LAYOUT_SPEC §7 for the version note.

Method: locate an input section inside the linked `.text` by matching all
bytes *not* covered by relocation writes, then apply the formulas above to
the object bytes and compare with the linked bytes.

### 4.1 type 200/202 pairs

Aggregate over all matched sections (input sections with relocations from
all 162 archive objects × all oracle ELFs):

```
sections_matched        : 302
200 pair verified       : 299 + 8 (inner ELF, see below)   FAIL 0
202 pair verified       : 299 + 8                          FAIL 0
unpaired (partner outside matched window): 5+5 (edges)     —
```

Coherence checks that every pair passed: reconstructed
`S = ((hi16<<16)|lo16) − A` is 4-byte aligned and lies inside the linked
image's SHF_ALLOC ranges.

Exact cross-check against a symbol table (`inner_shave.elf`, `.dyn.text`
section `shave_malloc.o:.text._EP_setheap` placed at `0x1c000c30`):

```
reloc  type  sym (object)              linked halves -> S        linked symtab value
@0x02  202   __stackMaximumExtent      0x1d00 / --   -> 0x1d000000   __stackMaximumExtent = 0x1d000000  ok
@0x05  200   __stackMaximumExtent      --   / 0x0000                same                              ok
@0x16  202   error_handler             0x1d00 / --   -> 0x1d00000c   error_handler = 0x1d00000c        ok
@0x19  200   error_handler             --   / 0x000c                same                              ok
@0xaa  202   error_handler             0x1d00 / --                                            ok
@0xad  200   error_handler             --   / 0x000c                                        ok
@0xe9  202   firstFreeBlock            0x1d00 / --   -> 0x1d000020   firstFreeBlock = 0x1d000020      ok
@0xec  200   firstFreeBlock            --   / 0x0020                same                              ok
... (16 abs halves total: 8 hi + 8 lo, all ok)
```

Independent pattern evidence (no symbol table needed): in
`fw_elf2 .dyn.text`, 82 locations decode as `[hi16][op][lo16]` whose
reconstructed address equals a real symbol, e.g. `.dyn.text+0x1bb` =
`a9 f3 00 1c f2 bc 00` → `0x1c0000bc` = `memcpy_specific65_asm`;
`+0x1fe` → `0x1c000090` = `packSHVCtrlResponse`; `+0x0b` → `0x1d000024` =
`fifoSendAddrs`.

### 4.2 type 209

Only linked section containing type-209 relocations that could be matched:
`shave_malloc.o:.text._EP_setheap` (19 relocations: 8×200, 8×202, 3×209)
inside `inner_shave.elf`.  For each 209:
`val = A − r_offset` (S cancels), `expected = old + val (mod 2^16)`:

| r_offset | addend A (target) | old (pre-link LE16) | expected | linked LE16 | verdict |
|---------:|------------------:|--------------------:|---------:|------------:|---------|
| 0x22 | 0xbc | 0x0006 | 0x006+0x9a = 0x00a0 | 0x00a0 | ok |
| 0xb7 | 0xdc | 0x0001 | 0x001+0x25 = 0x0026 | 0x0026 | ok |
| 0xc0 | 0xd9 | 0x0001 | 0x001+0x19 = 0x001a | 0x001a | ok |

3/3 exact.  Cross-check of the `old = r_offset − instruction_start`
invariant: `0x22 − 0xa0 → start = 0x1c (=0x22−6)`; `0xb7 − 0x26 → start =
0xb6 (=0xb7−1)`; `0xc0 − 0x1a → start = 0xbf (=0xc0−1)` — i.e. the final
field always equals `target − instruction_start`, so the pre-encoded `old`
is exactly the field's position within the instruction.

Further data points (same file): `.text._EP_set_heap_error_handler`
(4 abs relocations) and `stack_checks.o:.text..crt._EP_get_stackhighwater`
(2) all verified the same way; total 19/19 relocations in this section
reproduce byte-for-byte.

**Not numerically validated** (no linked oracle contains them — confirmed by
scanning every matched section of all 375 prebuilt kernels and the firmware
ELFs for all 490 type-209-bearing library sections: 0 matches; kernels never
link the big mlibm math functions and `--strip-debug` kills type-208 input):
types 208, 210, 201, 203, 204, 205, 206, 207 — formulas in §3 are taken
verbatim from the decompiled dispatch and reproduced 1:1 in
`lld-shave.patch` (`SPARC32.cpp`).

### 4.3 Reproducing the validation

A machine-readable dump of every mlibm relocation (file, section, offset,
symbol index, addend) produced by the enumeration step is kept next to this
document as `relocs_raw.json`.

```sh
# re-enumerate inventory
for a in '/home/asdf/projects/r2gvp04w-re/extracted/code$GetExtractPath$/Driver/MVC_DEPEND/lib/'*.a; do ... readelf -r ... ; done
# extract dispatch table from the DLL (read-only PE parsing)
python3 - <<'PY'
import struct
f=open('/home/asdf/projects/r2gvp04w-re/extracted/code$GetExtractPath$/Driver/MVC_DEPEND/bin/moviLLD64.dll','rb').read()
# .text: VA 0x1000, raw 0x400; table VA 0x1802c7324, image base 0x180000000
off = 0x2c7324 - 0x1000 + 0x400
tab = f[off:off+208]
print([tab[t-3] for t in range(200,211)])   # [17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27]
PY
```

## 5. Summary for the reimplementation

`lld-shave.patch` adds `lld/ELF/Arch/SPARC32.cpp`:

* `getRelExpr` reproduces the vendor grouping of §2.1 case-for-case.
* `relocate()` implements §3.1 exactly (validated) and §3.2 exactly
  (decompile-only), using little-endian read-modify-write; standard SPARC
  cases use SPARCV9 formulas (§3.3, dead paths).
* No instruction decoding, no bundle awareness, no alignment assumptions —
  exactly like the vendor: `.text` is an opaque byte stream and `r_offset`
  may be odd.
* Endianness: SHAVE data and instruction fields are little-endian
  (`.rodata4.one = 00 00 80 3f` = 1.0f in the object file;
  address halves LE in linked output).

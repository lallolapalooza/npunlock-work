# ENCODING_NOTES — how the SHAVE instruction encodings were recovered from moviAsm64.dll

Work dir: `/home/asdf/projects/npunlock-work/shave-isa/`. All inputs read-only from
`/home/asdf/projects/r2gvp04w-re/`. Companion artifacts: `encodings.json`,
`shave_asm.py`, `VALIDATION.md`, plus RE scripts (`peutil.py`, `extract_defs.py`,
`bulk_extract.py`, `gen_encodings.py`, `validate_efsqrt.py`).

## 1. PE layout of moviAsm64.dll

7,488,264 B, PE32+ (machine 0x8664), ImageBase `0x180000000`, 7 sections:

| section | vaddr | raw | raw size |
|---|---|---|---|
| .text | 0x1000 | 0x400 | 0x689800 |
| .rdata | 0x68b000 | 0x689c00 | 0x72c00 |
| .data | 0x6fe000 | 0x6fc800 | 0x7200 |
| .pdata | 0x718000 | 0x703a00 | 0x1a400 |

Mnemonic strings live in .rdata (file 0x689c00..0x6fc800): 2,488 unique
`UNIT.OP…` names (regex over `(IAU|LSU0|LSU1|VAU|CMU|BRU|PEU|SAU)\.…`),
2,553 occurrences incl. .text thunks. Class-token block at VA `0x180692ea8`:
`VR VW IW IR I V` (4-byte slots) + `<regNo>.<subregNo>`/`<regNo>` format
templates at `0x180692ec0`. Other class strings: `D` @ `0x1806a5d44`
(struct `{char 'D'; u32 …; u32 3}`), `X` @ `0x1806a5de4`, `0x` @ `0x18068ebb0`.

## 2. Where the instruction definition tables are built

There are **no static pointer tables** (scan of the whole image for u64/u32
absolute pointers or rel32s into mnemonic strings found none usable): the
instruction definitions are **constructed at process init** by ~350 builder
functions. Key evidence chains:

* `FUN_18002cb90` (decompiled_all.c:19794) = chip-version → platform factory.
  Per-platform table builders (decompile lines ~83083–83604):

| platform ctor | table builder(s) |
|---|---|
| FUN_18007eed0 → `CV3600xxPlatform` | FUN_18042d4a0 + **FUN_18042d4c0** (calls 156 family builders) |
| FUN_18007f050 | FUN_18042eff0 + FUN_18042f010 |
| FUN_18007f1d0 | FUN_18042f070 + FUN_18042f160 |
| FUN_18007f350 → `CV5000xxPlatform` | FUN_18042f070 + FUN_1804304d0 |
| FUN_18007f4d0 → `MA2x8xPlatform` | FUN_180431840 + FUN_180431930 |
| FUN_18007f650 | FUN_18042eff0 + FUN_180432ae0 |
| FUN_18007f7d0 | FUN_18042d4a0 + FUN_180432b400 |
| FUN_18007f950 → `Myriad2Platform` | FUN_180434010 + **FUN_180434100** (1,495 inline defs) |

* `--cv 3720xx` maps into the ChipVersion group strings (`3700xx…3740xx`,
  `3600xx(-nn)`, `4000xx`, `3010xx`, `5000xx`, `ma2x5x/ma2x8x` …) present in
  .rdata; RTTI classes: `CV3600xxPlatform CV3600xx_nnPlatform CV4000xxPlatform
  CV5000xxPlatform MA2x8xPlatform CV3010xx(_nn)Platform Myriad2Platform`.
  Exact string→ctor table not fully pinned (deferred); empirically the
  mlibm/gold corpus behaves like a Myriad2-family encoding set (see §6).

* Two record **schemas** are built:

  **Schema A (16-byte records) = the encoder/format table** — record =
  `{const char* class; u32 a; u32 b}`. Built either inline (mov-stores to a
  stack buffer + `FUN_180028fe0` alloc + `FUN_180619fb0` memcpy) or finalized
  by `FUN_180433f50(obj, name, span)` (divides span by 16: `lVar2 = lVar2>>4`).
  Final def object = `{std::string name (32B), vector<record> (begin,end,cap)}`.
  7,099 schema-A defs extracted across 344 functions (script:
  `extract_defs.py` + `bulk_extract.py`); 5,523 distinct instruction forms
  after platform/duplicate merge (`encodings.json`: 5,525).

  **Schema B (20-byte records) = analyzer/latency + parse-table**, built via
  `FUN_1800ca810(obj, name, flags, fmt, span)` (decompile: span/0x14 ⇒ 20 B
  records; `fmt` = pointer to a format template string used by the error
  "The %s doesn't match the \"%s\" format from the instruction table").
  9,696 schema-B defs — *not* used for encoding.

* Record class semantics (cross-checked against decompiled source, e.g.
  FUN_1804e4460 LDIH = `{'0x',1,0x10},{'IW',0,5},{'D',0x382,0xb}`):
  - `D` = literal (`a`=value, `b`=width bits); `X` = vendor don't-care slot
    (recorded as literal 0; binaries show it can be nonzero ⇒ roundtrip caveat);
    `BS` = 8-bit literal-like field in branch forms;
  - `IW/IR` = integer-reg operand, write/read (`a`=operand index, `b`=width=5);
    `VW/VR`, `VRR/VRW`, `IRR/IRW`, `VE`, `F`, `T`, `E`, `Av/As` = vector /
    sub-register / element classes (same layout);
  - `0x` = immediate operand (`0X<n>` = scaled immediate with scale n decimal,
    e.g. `0X4` in `LSU0.LDO.32` ⇒ byte-addressed ×4; `0X` in BRU ⇒ relative);
  - `P` = predicate/condition operand; operands may appear in several fields
    (bitfields list in encodings.json).

## 3. Bit layout (model `msb_le`) — validated

* Fields are packed **first record at the TOP bits** of the N-bit word
  (N = Σ widths), zero-padded above to a byte multiple, emitted **little-endian**.
  Equivalently: records reversed + LSB-first + LE bytes.
* Validation examples (all fixed bits equal, real bytes from mlibm/gold):
  - `CMU.CMII.U32` = `{X,0,4},{D,0,1},{D,0,2},{D,1,1},{D,0,1},{IR,1,5},{IR,0,5},{D,129,13}`
    ⇒ `81 90 10 00` (byte0 = 129, byte1 = reg bits) ✓
  - `IAU.AND` = `{IR,2,5},{IW,0,5},{IR,1,5},{D,4230,13}` ⇒ `86 50 26 03` ✓
  - `LSU0.LDO.64` `{…,{D,706,13}}` ⇒ byte0 = 706 = 0xc2 ✓, LSU1 variant
    D=713 ⇒ 0xc9 ✓ (per-unit opcodes differ by design: 706/713, 898/905 …).
  - `LSU0.LDIL` `{0x,1,16},{IW,0,5},{D,642,11}` ⇒ `82 4a 00 00` — exact match
    at ef_sqrt.o+0x9c (reg=i9: byte1 = (642>>8)|9<<3 = 0x4a) ✓
* Variable instruction length: forms are 1–6 bytes (length distribution in
  encodings.json: 1B×2, 2B×22, 3B×451, 4B×3207, 5B×1837, 6B×4).
* Branch displacement: signed 16 (also 14/5-bit forms) relative to
  **instruction start** (not field, not next-packet).

## 4. Reloc semantics (from LinkerRelocRE / lld-sparc RELOC_NOTES.md)

* type 200 (0xc8): `*(u16_le*)loc |= S+A` (LO16 abs), pre-link field = 0.
* type 202 (0xca): same with `(S+A)>>16` (HI16 abs). Pairs appear 3 bytes
  apart: `[HI16][opcode byte][LO16]`.
* type 209 (0xd1): expr `S+A-P`; `*(u16_le)loc += (int16)val` (RMW); range ±32K;
  **pre-link `old = r_offset - instruction_start`** ⇒ in .o files
  `field = off - instr_start`, after link `field = target - instr_start`
  (validated 3/3 on linked firmware; used here to bound instruction starts).
* type 210/201/203/204–207: 16-bit fields at bits [19:4] / [15:2] / [19:6] /
  [18:3] / [22:7] (no linked oracle; decompile-only, see their notes).

## 5. Bundles / VLIW

* Source syntax: `slot0 || slot1 || slot2` (`||` = "pipe"; diagnostics:
  "BRU unit is used more than once in the same pipe", "Invalid NOP in pipe",
  "Double pipe found"). Assembled output = plain instruction concatenation;
  vendor options `-noFinalSlotCompression`, `--noCompressedNOPs`,
  `--noSlotAllPromo`, `-forceDelaySlots` show NOP-fill/slot-compression and
  delay-slot handling live in the vendor assembler.
* Stall limit strings: "… might generate a stall (> 128 bits)" ⇒ packet ≤128
  bits, ≤3 slots. `-L` aligns labels to 128 bits.
* `.64`/`.128`-style suffixes are *data* directives (ERR_64_BIT_BLOCK,
  ERR_128_OUTSIDE_CODE_SECTION), not instruction suffixes.
* **Packet boundary markers are not visible in the byte stream** (no header
  found; lld treats .text as opaque). Instruction boundaries are byte-granular;
  branch targets (type-209 addends) always land on instruction starts — used
  as hard constraints in `validate_efsqrt.py` (interval DP over DWARF stmt
  starts + 209 starts/targets).

## 6. What is confirmed vs inferred (confidence labels in encodings.json)

* `table-derived` (current default): record list extracted from builder code.
* `confirmed-by-roundtrip`: forms whose full fixed bits are proven against
  real bytes (the §2/§3 examples + everything the interval-DP decoded:
  CMU.CMII*, CMZ, IAU.AND/OR/INCS/ADD/SHL, LSU0.LDO/LDI*, SAU.SUB, CMU.CEX …).
* `inferred`: forms reconstructed from corpus bytes where the DLL form was
  missing/ambiguous (see §7).

## 7. Open gaps (blockers for ≥95 % coverage) — status after patch_encodings.py

1. **LSU1.LDIH hi-half pair at ef_sqrt+0x79e** (`a9 4b 00 00`, reloc 202 at
   +0x7a0) — *operationally closed*: carried as the corpus-derived form
   `LSU1.LDIH fmt=100` (`fixed=0x3a9 = 0x389|0x20`, `mask=0x7ff`,
   reg@bits15-11, imm@bits31-16 ⇒ imm at bytes 2-3 — exactly the vendor
   `HI at x, LO at x+3` pair geometry; LO16 half = the 3-byte
   `LSU0.LDIL fmt=100` at bytes 1-2, cf. ef_sqrt+0x7a2 `4a 00 00` with
   reloc 200 at +0x7a3).  Corpus: 102 occurrences of the 11-bit LDIH form
   vs ≈3 expected from chance.  **Root cause still open**: exhaustive search
   of all 7,099 schema-16 defs finds *no* `D=937` literal — the bit-5
   overlay is not derivable from this DLL revision's tables.
2. **24-bit branch `[b5][rel16]`** — *closed*: real layout is
   `[0xb5 @byte0][rel16 @bytes1-2][pred8 @byte3]`, i.e. record list
   `[P, 0X, D=181]` packed first-at-top (the predicate behaves as an 8-bit
   reg: gate1's `BRU.BRA i16, label` writes 0x10 at byte3).  Evidence:
   4 gold sites + 8 ef_sqrt type-209 rela sites (`r_offset = start+1`;
   addends land on instruction starts — 0xb7 = the `BRU.JMP i30` epilogue,
   0x4a = loop-head LDO, 0x98 = loop body).  `BRU.BRA fmt=102` repaired in
   patch_encodings.py (fixed `0xb5`, mask = fixed bits only, pred@[31,24],
   rel@[23,8]).  **No schema-16 def has D=181** (searched) — corpus-derived.
   The unconditional branch is `[0xc7][rel16][01]` (records `[BS=1, 0X16,
   D=199]` first-at-top), anchored at ef_sqrt+0xb5 with a type-209 rela;
   gen packed it byte-reversed (`fixed=0xc7000001`), which matches no real
   bytes — the first-at-top repack supplies the correct twin.
3. **Prologue sequence** `a9 53 ff 7f 52 ff ff 5e …` — *partially closed*:
   the first pair decodes as `LSU1.LDIH i10, 0x7fff` + `LSU0.LDIL i10,
   0xffff` (fmt100 fixed-bit checks pass), then `5e a8` = `PEU.PC1S 7`,
   then 1 byte `.byte 0x43` undecoded (ef prologue: 25/32 bytes).
4. **`X` don't-care bits** — *closed*: the `raw=` annotation carries every
   bit outside the chosen form's mask through the disasm→asm roundtrip;
   gold roundtrip is 192/192 byte-identical (VALIDATION.md (b)).
5. **Remaining gold `.byte` runs (42 B / 19 runs)** — attempts, all negative:
   (a) first-at-top repack of every schema-16 record list (+5,459 forms);
   (b) both packings of all 32 `ok=False` defs (BRU.RPIM/SWIC/SWIH,
   LSU*.LDO/LDA/STA/STO.8/.16, SAU.ADDCI) tested at every offset of every
   run — 0 matches; (c) names outside the canonical mnemonic set — 0 (all
   schema-16 names canonical).  Patterns like `a3 fc`, `acad`, `2b 40`,
   `1b 50 e9 9d 24 01` have no definition in this DLL revision's extracted
   tables.  Coverage: gold 78.1 %, ef_sqrt 71.8 %.
6. `.rdata` string dump cross-refs: `moviAsm64.ascii.txt` lines 1414–6853
   (mnemonics), 2557–2641 (pipe/slot diagnostics), 2739–2760 (CLI incl.
   `--cv <chipVersion>`); `moviCompile64.ascii.txt` 78385–78387 (LDIL/LDIH
   abort stub templates), 82746–83622 (ISel opcode names LSU_LDIH_Label…).

## 8. Platform note for npunlock (`--cv 3720xx`)

npunlock's gold was assembled by *this* DLL revision; the mlibm archive
carries `moviCompile v00.114.11 Build 4207` in `.debug_str`. Divergences in
opcode constants between the corpus and this DLL's tables (§7.1) suggest the
corpus was assembled by a slightly different vendor build; per-platform builder
families exist in the DLL (§2) and are all extracted — picking the exact
init-chain that matches `--cv 3720xx` (probably `Myriad2Platform` →
FUN_180434100, already extracted) plus the §7 rules is the path to full
coverage.

## 9. Reloc type summary (for stream R cross-check)

Counts across all SHAVE archives (from LinkerRelocRE): 200:4677, 202:4705,
208:111124, 209:8880, 210:81; mlibm subset: 200:828, 202:828, 208:18953,
209:1131, 210:17. Type-209 starts used as decode constraints: see
`validate_efsqrt.py` (interval DP over DWARF statement starts ∪ 209 starts ∪
209/210 addends).

## 10. patch_encodings.py — record order, repack, encode evidence

**Discovery.** §3's validated model is *first record at the TOP bits* of the
word.  `gen_encodings.py` packs the extracted record list *first-at-bottom*
(bitpos grows from 0).  Extraction preserved the vendor list order
inconsistently across builder functions (provenance is in `defs_extracted.pkl`
`func` field: e.g. `FUN_180434100` = Myriad2Platform builder yields
`[0x, IW, D]` for LDIL and `[BS, 0X, D]` for BRU.BRA), so the generated json
was correct for some forms and byte-reversed for others:

* reversed in gen: `LSU0.LDO.64` (fixed D@bits39-28; real bytes have
  D706 at bits11-0), the `[c7][rel16][01]` branch (`fixed=0xc7000001`),
  the first-at-bottom `BRU.BRA` rel14 form;
* correct in gen: forms whose extracted list was already reversed
  (e.g. CMU.CMII.U32 `81 90 10 00`).

**What patch_encodings.py does** (run after gen_encodings.py; idempotent):

1. appends a **first-at-top repack** of every distinct schema-16 record list
   (+5,459 forms, deduped by fixed/mask/operand positions);
2. repairs `BRU.BRA fmt=102` → `[b5][rel16][pred8]`;
3. drops two corpus-absent encode-poison forms (`LSU0.LDIL fmt=1` 5-byte,
   `BRU.BRA fmt=0` rel14 — 0 matches in 7,394 corpus bytes; before the drop
   they won `resolve_form` by mask specificity and emitted nonsense);
4. normalises all BRU `0X` operands to kind `rel` (+ dedup);
5. stamps **`corpus_hits`** (matches over all mlibm `.text` + gold `.text`
   windows) and **`encode_priority`** (0 = reloc/DWARF-anchored: LDIH pair
   fmt100, LDIL fmt100, BRU.BRA fmt102, BRU `[c7]` form).

**resolve_form ordering (encode):** `encode_priority` → forms present in the
vendor corpus (`corpus_hits>0`) → mask specificity.  Decode uses
specificity only (unchanged), with `raw=` guaranteeing roundtrip fidelity
even when a twin form ties.

**Empirical form anchors (this DLL + corpus):** LDIH fmt100 (`a9 4b 00 00`
family, 102 corpus hits), LDIL fmt100 (`4a 00 00` family; reloc-anchored),
b5-branch (gold×4 + ef×8 rela), c7-branch (ef+0xb5 rela).
`D=937` and `D=181` literals do not occur in any schema-16 def — those two
encodings are corpus-derived only.

**STT_FILE note:** upstream lld-14 (default discard policy) *keeps* FILE
symbols in the output symtab — verified live.  Gold's linked symtab is
NULL + `controlled_act` only, so vendor entry.o carried none; `obj` mode
therefore parses `.file` but does not emit STT_FILE rows (LAYOUT_SPEC §5's
"lld drops FILE syms" claim is wrong for lld-14; SECTION syms *are* dropped).

## 10. Calibrated/inferred forms added (corpus-driven, evidence-backed)

Recovered from real binaries when no DLL def matched (all in encodings.json
with `confidence: confirmed-by-roundtrip` / `inferred`):

* `LSU1.LDIH` fmt=100: canonical layout `[imm16][reg5][subop3=3][op8=0xa9]`
  (value bytes: `[0xa9][3|reg<<3][HI16 LE]`). Evidence: all 739 type-202 sites
  across mlibm .o: byte0=0xa9 in ~700 (rest are heuristic-start artifacts),
  byte1&7==3 in 100% of primary-family sites; matches moviCompile's
  `LSU0.LDIL iN, lo || LSU1.LDIH iN, hi` pairing (HI on LSU1).
* `LSU0.LDIL` fmt=100/101: 3-byte `[subop3|reg<<3][LO16 LE]`, subop=2 (LSU0)
  / 6 (LSU1 variant, 695 instances). byte0&7 == subop in all type-200 sites.
* `BRU.BRA` fmt=102 (`inferred`): `[op8=0xb5? no — value bytes:
  [0xb5][rel16 LE][cond/operand@byte3]` i.e. canonical
  `[operand8][rel16][lit 0xb5]`. Matches every type-209 old=1 site in gold
  (0x53, 0x73, 0x84, 0xa1) and ef_sqrt (0x62, 0x73, 0x96, 0xa7). No matching
  def exists in this DLL revision (exhaustive search of all schema-16 defs for
  a form with fixed low byte 0xb5 or a 2-record {rel16, op8} pair: zero) —
  the DLL's only branch defs are `BRU.BRA {BS,1,8},{0X,16},{D,199,8}` (matches
  the `[0xc7][rel][bs]` site at ef_sqrt+0xb5 exactly) and the 14-bit-rel fmt0.
* Root-cause note: byte0 for the LDIH emit = 0xa9 = (def 0x389) | 0x20;
  LDIL subop byte0&7 == def>>8 (2) exactly. The +0x20 overlay on LSU1.LDIH
  (or equivalently a def-constant delta 0x389 -> 0x3a9) is consistent across
  every corpus instance but not derivable from this DLL's tables; alternates
  tried and rejected are listed in §7.1.
* Don't-care (`X`) handling: vendor binaries carry nonzero X-slot bits
  (gold 0x45/0x63/0x7b/0x99/0xb0). `disasm` prints `; fmt=N raw=<hex>`;
  `asm` restores those bits (`v |= raw & ~mask`) — roundtrip is byte-exact by
  construction; disasm chooses not to guess X values (they are printed only
  as part of `raw`).

Generation pipeline (order matters; run all three to regenerate encodings.json):

    python3 gen_encodings.py       # DLL defs + table entries
    python3 patch_encodings.py     # corpus stamps (corpus_hits/encode_priority), BRU dedupe
    python3 cal_fix.py             # calibrated/inferred canonical layouts (LDIL/LDIH/BRU)

Running gen_encodings.py alone wipes the corpus stamps (resolve_form then picks
5-byte over 3/4-byte forms) and the calibrated canonical fixes — always re-run
the full chain. See cal_fix.py's docstring for the exact remap math.

## 11. Fused f16 family addressing (hardware-triage evidence)

* The DLL's f16 fused memory family has NO base+displacement variant:
  `STO.16.F32*` / `LDO.16.F32*` are absent from all 10,967 entries. Shapes:
  `LSU0.ST.16.F32.F16` = 2-op (value, addrReg); `LSU0.STI.16.F32.F16` =
  3-op (value, addrReg, increment); `LSU0.LD.32.F16.F32` = 2-op
  (dst, addrReg). Base+imm addressing exists only in the integer families
  (LDO.32/LDO.64/STO.64: `d/s, base, off`).
* No .text relocation lands on any fused ST/LD site in halfmath/ef_sqrt
  (nearby relocs are type-209 branches and type-200/202 LDI pairs at
  `one`/`tiny`) => operand is a plain register (address-in-register),
  not a relocated field.
* `[iN]` and `iN` spellings encode identical bytes (the bracket form parses
  to a mem operand that resolves to the same reg field).
* Corpus anchors (raw mask-scan over all mlibm .text + decode lines):
  `ST.16.F32.F16` fmt1 = 1 raw hit = halfmath 0x0030 (`42a43a00`,
  decode+re-encode exact); `LD.32.F16.F32` fmt0 = 12 raw hits + gold
  `.text+0x98` = `42701200` exact; `STI.16.F32.F16` fmt1 (5B) matches the
  gold 0x8e window `8264510030` modulo a don't-care byte, decode boundary
  contested with a SAU.EXP match at 0x8d (see VALIDATION).
* OPEN GAP: gold `.text+0xaa` (`3c e0 9c 30 10`, 5B, the f32-add site of
  `in[i] + 1.0f`) mask-matches NO entry in encodings.json (all SAU.ADD.F32
  fmts checked: False; full-candidates scan over 3/4/5B: empty). An earlier
  decode listing's `SAU.ADD.F32 i7, i7, i1` at 0xaa was a stale state.
  `SAU.ADD.F32` fmt2 (raw 5 hits; patch stamp 32) is the least-verified
  corpus anchor in the default emit set.

## 12. Value-space ("first-at-top-repacked") entries — the double-remap bug

patch_encodings.py appends 5,459 forms tagged `confidence:
first-at-top-repacked`.  Their fixed/mask/bitfields are stored directly in
VALUE space (record0 already at the top bits), but shave_asm.Insn remapped
every entry as if it were canonical-LSB — flipping these a second time.  Both
decode (`(v & mask) == fixed` never hit) and encode (operands placed at
mirrored positions) were wrong for the whole class.  Fixed in
`Insn.__init__`: entries tagged `first-at-top-repacked` now bypass the remap
(fixed/mask used as-is, field bitpos = bitfields lo).

Consequences (all verified after the fix):

* gold `.text+0xaa` — the unresolved f32-add site — now decodes:
  `0x00aa: 3ce09c30 SAU.ADD.F32 i7, i7, i1 ; fmt=3 raw=3ce09c30`.
  Candidate scan over ALL 10,967 forms at 0xaa (LE, lengths 3/4/5B): exactly
  one match = SAU.ADD.F32 fmt3 (mask 17, corpus 10).  Byte-reversed scan
  yields only low-information noise (CMU.CEX.I32 mask16, VAU.LOG mask13).
  Hintless `SAU.ADD.F32 i7, i7, i1` now re-encodes `3ce09c30` byte-exact
  (cal_fix pins fmt3 encode_priority=-1; fmt2 is the other repack variant,
  subop nibble 2 vs 3).
* Resolved gold instruction boundaries (file 0x160..0x17f):

  ```
  0xa4-a7  BRU.BRA  i14, 0x98        fmt102
  0xa8     (1 byte, no form: eb)
  0xa9     BRU.INC                  fmt0  (07)
  0xaa-ad  SAU.ADD.F32 i7, i7, i1    fmt3  (3ce09c30)   <- was .byte
  0xae     (1 byte, no form: 10)
  0xaf-b2  LSU1.STI.16.F32.F16 i7, i8 fmt6 (49741008)   <- the out[i] store;
                                                               was 'CMU.MAXMINR8'
  0xb3-b6  IAU.ADD i9, i9, 2         fmt1  (c6202581)
  0xb7-b9  BRU.JMP i30               fmt0  (47f101)     <- epilogue, now its own line
  ```
* gold `.text+0x8e` now decodes as the STI store `8264510030 LSU0.STI.16.F32.F16
  i11, i20, i0 ; fmt=1` (the earlier SAU.EXP anchor at 0x8d was a pre-fix
  artifact; the STI reading matches out[i] store+increment semantics).
* Fresh gates: gold roundtrip 192/192 identical; gold decode 132/192 (68.8%);
  mlibm ef_j0 82.2%, halfmath 75.0%, ef_sqrt 66.8%, math_lite 66.7%,
  shave_fenv 34.6%.  Percent movement is boundary-composition change (fake
  LDIL matches lost, repacked real forms gained/lost) — roundtrip exactness
  unaffected.
* Corollary for emit: before this fix any form chosen among repacked entries
  encoded MIRRORED bytes (e.g. `SAU.ADD.F32 i0,i0,i1` produced `1200e001`;
  now `3c008030`).  Re-encode anything emitted before the fix.

## 13. Pointer-read hunt + fused-f16 probe re-verification (post double-remap fix)

### Q1: gold has NO LDO read of params+0x00 or params+0x28

Method: every LDO* form in encodings.json (any unit, any fmt, mask >= 4)
mask-matched against gold .text at EVERY offset 0x00..0x4f, operands inverted
through the calibrated fmt0 field map (fields: dst@12, base@17, imm@25 width15;
calibration: 0x00 -> (i10, i18, 12) and 0x05 -> (i7, i18, 16) = the known
rank/dims reads). Result:

```
0x00 c2a2641900  LDO.32 fmt0/3  (i10, i18, 0xc)   <- rank, known
0x05 c272642100  LDO.32 fmt0/3  (i7,  i18, 0x10)  <- dims, known
0x4a c2524efbff  LDO.32 fmt0/3  (i5, i7, 32765)   <- artifact (wild imm,
                    base i7); overlaps the decoded CMVVABS@0x49; not a
                    descriptor read
```

No other LDO-family byte pattern exists in gold .text: substring searches for
the fmt0 tails `640100` (imm=0) and `645100` (imm=0x28) find only 0x8f,
which is inside the STI store `82 64 51 00 30` (no `c2` opcode prefix — not an
LDO).  A full LD-family scan (LDO/LD/LDI/LDX, 1,957 forms) over the undecoded
runs 0x0a, 0x1e-0x39, 0x3a-0x49 finds only:

```
0x27 420100d2  LSU0.LDXV fmt0 (i0, i0)             mask13
0x44 421000e0  LSU0.LD.32.F16.F32 fmt0 (i1, i0)    mask18
                (identical bytes also match LSU0.LD.64.H fmt0 — see Q2 ties)
```

MAJOR FINDING: the gold binary contains exactly two descriptor reads
(+0xc, +0x10).  There is NO LDO-family load of params+0x00 (input addr) or
params+0x28 (output addr) anywhere in .text, while add1-fp16.c and
KERNEL_ELF.md both require them.  Either (a) the fixture source and the gold
binary genuinely disagree about where in/out pointers are read, or (b) the
reads use an instruction family absent from the DLL's def tables (precedent:
BRU.BRA fmt102).  Byte evidence: an `0x28` literal byte sits at 0x22 inside the
9-byte run 0x1e..0x26 (`a3 fc ff 0f 28 c8 50 10 00`) with no known-form
instruction consuming it; the `+0` read has no matching pattern at all.

### Q2: fused-f16 probe re-verification

Anchor re-encodes AFTER the value-space fix (fresh):

```
CMU.CP.32 i6, i9           -> 819c0c08   gold .text+0x59, byte-exact
CMU.CP.32 i2, i1           -> 811c0408   (their probe; same form)
LSU0.ST.16.F32.F16 i21, i14-> 42a43a00   halfmath 0x0030, byte-exact
LSU0.ST.16.F32.F16 i0, i1  -> 42040400   (their probe; same form)
LSU0.LD.32.F16.F32 i7, i9  -> 42701200   gold .text+0x98, byte-exact
LSU0.LD.32.F16.F32 i0, i0  -> 42000000   (their probe; same form)
```

Raw presence in gold .text: 42701200 x1, 819c0c08 x1, 42040400 x0,
42000000 x0, 811c0408 x0, 42a43a00 x0 (that one lives in halfmath).
Byte-pattern (mask) counts over mlibm+gold: CP.32 fmt1 = 610 hits,
LD.32.F16.F32 fmt0 = 14, ST.16.F32.F16 fmt1 = 1 (the halfmath anchor).
Zero-hit rows are operand-specific strings — normal; the FORMS all anchor.

Encoding-identity ties (same mask+fixed+lenB, different mnemonic — the
encoding cannot distinguish them):

* `LD.32.F16.F32 fmt0` == `LSU0.LD.64.H fmt0` (mask18) — if hardware semantics
  were the .64.H variant, LD probes would perform a 64-bit access (alignment
  sensitive).  Gold's loop context (f16->f32 convert feeding the SAU add)
  supports the f16-convert reading.
* `STI.16.F32.F16 fmt6` == `LSU0.STI.64.H fmt6` (mask18) — same caveat for the
  gold out[i] store at 0xaf.
* ST.16.F32.F16 fmt1, CMU.CP.32 fmt1, SAU.ADD.F32 fmt3: unique, no ties.

Verdict: no fmt/field-selection error remains in these four probes; remaining
fault suspects are ABI/semantic (which register holds what, alignment under
the .64.H tie), not byte encoding.  Decode-side note: gold's 0x44
LD.32.F16.F32 (i1, i0) is currently hidden because greedy decode takes the
weak `LDIL` match at 0x43 (mask3) first — emit-side unaffected (raw restore).

## 14. Pointer-setup region 0x0a-0x49: exhaustive decode attempt

Method (per Main): masks >= 2, ALL units, lengths 1-6B, every start offset
0x0a..0x49; operand inversion; targeted scans for an imm field covering the
literal 0x28 at 0x22 with values {40, -4, 16, -1, 0xffff}; forward decode from
the rank/dims anchors and backward from the 0x3a/0x44/0x49 window.

### Instruction table (confirmed)

```
0x0a  40                 .byte (no form, any length 1-6)
0x0b  0e88ac             LSU0.LDIL fmt101 (i1, 0xac88)      conf=confirmed (weak mask3)
0x0e  0a0832             LSU0.LDIL fmt100 (i1, 0x3208)      conf=confirmed (weak mask3)
0x11  00 00              .byte 2 (no form)
0x13  6eea70             LSU0.LDIL fmt101 (i13, 0x70ea)     conf=confirmed (weak mask3)
0x16  0c 01              .byte 2 (no form)  [would be an i13 pair partner]
0x18  420f00             LSU0.LDIL fmt100 (i8, 15)          = rank limit (rank > 15)
0x1b  320100             LSU0.LDIL fmt100 (i6, 1)           = count init (count = 1)
0x1e  a3 fc ff 0f 28 c8 50 10 00   .byte 9 — NO form matches (see below)
0x27  420100d2           LSU0.LDXV fmt0 (i0, i0)            table-derived, mask13
0x2b  18 53 ac ad        .byte 4 — no form
0x2f  1221b4             LSU0.LDIL fmt100 (i2, 0xb421)      weak mask3
0x32  03                 .byte 1 — no form
0x33  aa4a01             LSU0.LDIL fmt100 (i21, 0x14a)      weak mask3
0x36  00 10 21 03        .byte 4 — no form
0x3a  81801205           CMU.CMII.I8 fmt4/5 (i8, i9)  (repacked tie with
                         CMIIABS/CMII.I16 fmt2 — mask tie resolved by fmt4)
0x3e  95 f1 01 eb 50     .byte 5 — no form
0x43  ea                 .byte 1 (competing: weak LDIL fmt100 i29,0x1042)
0x44  421000e0           LSU0.LD.32.F16.F32 fmt0 (i1, i0) mask18 — competes
                         with the LDIL@0x43 (greedy decode takes LDIL; the
                         mask18 read is almost certainly the real one)
0x48  9c                 .byte 1 (would be the tail if LD@0x44 is real)
0x49  81c2524e           CMU.CMVVABS.I8 fmt1 (i22, i20)     table-derived mask18
0x4d  fb ff 50           .byte 3 — no form
```

Loose-scan NEGATIVE result (the hard part): across ALL 10,967 forms, ALL
units, lengths 1-6, masks >= 2, the runs 0x0a, 0x1e-0x26, 0x2b-0x2e, 0x32,
0x36-0x39, 0x3e-0x42, 0x46-0x48, 0x4d-0x4f contain ZERO matches at any start
offset — not even 2-bit-mask forms.  The targeted imm scans (imm covering
0x22 equal to 40/-4/16/-1) return zero rows too.

### THE ANSWER: neither in- nor out-ptr producer can be named

Register provenance visible around the region: the loop (0x98..0xb6) walks
the INPUT pointer in i9 (`LD.32.F16.F32 (i7, i9)` at 0x98, `IAU.ADD i9, i9, 2`
at 0xb3 = f16 stride) and stores through i8/i20/i11 (`STI (i7, i8)` at 0xaf,
`STI (i11, i20, i0)` at 0x8e); `LD.32.F16.F32 (i1, i0)` at 0x44 and
`LDXV (i0, i0)` at 0x27 implicate i0 as a base too.  The INITIALIZATIONS of
i0/i8/i9 (and any `load32(params+0x00)` / `load32(params+0x28)` that must
precede them) are not expressible with any instruction in encodings.json and
do not appear anywhere in gold .text.  LOUD FLAG: fixture add1-fp16.c /
KERNEL_ELF.md require two descriptor reads (+0x00, +0x28); the gold binary
has exactly two LDO-family reads (+0xc rank, +0x10 dims) and no other memory
operand carrying 0x00 or 0x28 — the fixture and the gold binary disagree, OR
the pointer-setup uses an instruction family missing from the DLL tables
(precisely like BRU.BRA fmt102 was).

### Minimal unknown-form hypotheses (for hand-assembly)

The literal 0x28 sits at 0x22 = byte4 of a candidate starting 0x1e.  Two
readings survive:

* 5B @0x1e: `[b0=0xa3 (reg/subop byte)][b1=fc][b2=ff][b3=0x0f (reg byte)]
  [b4=0x28 = imm8 = the +0x28 offset]` followed by a 4B tail `c8 50 10 00`.
  This is the ONLY layout in which 0x28 is an immediate.  If it is the
  out-pointer read, the template is `[op/reg 4B][reg 1B][imm8]` — needs a
  memory base field, i.e. a LDO-class instruction with imm8 at byte4 and
  opcode != 0xc2.
* Three 3B instructions `[reg5|subop3@b0][imm16 LE]` with subop in {3, 7, 0}
  (the known LDIL family has only subop 2/6; LDIH is a9-prefixed):
  0x1e `a3 fc ff` = (i20, 0xfffc=-4), 0x21 `0f 28 c8` = (i1, 0xc828),
  0x24 `50 10 00` = (i10, 0x0010=16 — plausibly the `16/count` limit).
  Under this split 0x22 is NOT an immediate and no +0x28 read exists even in
  the unknown family.

Other run templates observed but unassigned: 4B `[b0][b1][b2][b3]`
(0x2b `18 53 ac ad`, 0x36 `00 10 21 03`), 5B (0x3e `95 f1 01 eb 50`),
3B (0x46 `00 e0 9c`, 0x4d `fb ff 50`), 1B (0x0a `40`, 0x32 `03`).

## 15. Cross-binary ACT corpus scan (187 prebuilt kernels) — family found

Corpus: all 187 `*.3720xx.elf` from openvinotoolkit/npu_compiler
(npu_ud_2026_38_rc1, media.githubusercontent LFS host), 1.5 MB total, in
/tmp/act_corpus/ (scan script: scan_act.py).

### The descriptor-read family (cross-binary, thousands of instances)

```
5 bytes, 40 bits:  [op8][dst4@12-15 | unit4@8-11][tag@16,22-23][base5@17-21][1@24][imm15@25-39]
  byte0 = opcode class {01, c2, f9, fa, f0, fc, 08, bb, b0, c3, bc, cf, 70, c9(store tag)}
  byte1 = (dst << 4) | unit        unit nibble 2 or 6 (LSU0/LSU1-style, cf. LDIL subop 2/6)
  byte2 = (base << 1) | size-tag   size-tag: bit0=0/1 = 32/64-bit, bit6 = class bit
  imm16 LE encoding = (imm << 1) | 1  (bit24 = fixed 1 parity)
  dst observed 0..15 (4 bits; i16+ not seen), base = i18 or i19
```

Opcode counts over the corpus (family-form matches): c2 6,932 (186 files),
c9 2,174, f9 1,305 (134), f0 1,239 (130), fc 440, 08 419, **01 415 (105)**,
fa 343 (97), bb 338, b0 296, c3 143... Offsets: +0xc rank 583x, +0x10 319x,
**+0x28 288x (c2) + fa-forms**, +0x50 200x, stride-4 descriptor fields
throughout (+0x04..+0x78 in 4-byte steps = the descriptor layout).

Instantiations verified byte-exact through `shave_asm.py` (cal_fix fmt110-114,
confidence rebuilt as corpus-confirmed):

```
LSU0.LDO.32 i8, i18, 0x28 ; fmt=110 -> 0182645100   (gold .text+0x8d, op=01)
LSU0.LDO.32 i6, i19, 0    ; fmt=112 -> fa66660100   (gather.elf, THE +0x00 read)
LSU0.LDO.32 i6, i18, 0x28 ; fmt=111 -> fa62645100   (gather.elf)
LSU0.LDO.32 i1, i18, 0x10 ; fmt=113 -> f912642100   (dims via f9-class)
LSU0.LDO.32 i1, i18, 0xc  ; fmt=114 -> f016641900   (rank via f0-class)
```

### Q4: does any prebuilt kernel read +0x00 (in-ptr)?  YES

`c2..64 01 00` = ZERO in all 187 files, but the fa-class does it:
gather.elf `fa 66 66 01 00` = LDO (i6, i19, +0x00) — in-ptr read off a
copied struct base (i19), opcode 0xfa.  (Generic imm0-tail scan: the only
non-noise rows are this family; +0x00 never appears with op c2/f9.)

### GOLD: correction + narrowed divergence

Re-scan of gold .text with the full family template (not just op=c2) finds
THREE family reads:

```
0x000 c2 a2 64 19 00   LDO (i10, i18, +0xc)   rank
0x005 c2 72 64 21 00   LDO (i7,  i18, +0x10)  dims
0x08d 01 82 64 51 00   LDO (i8,  i18, +0x28)  OUT-POINTER  <-- FOUND
```

CORRECTIONS: (1) sec 13/14's "gold has no +0x28 read" is RETRACTED — the
`645100` substring I dismissed at 0x8f IS this instruction (op byte 0x01,
previously rejected because it lacks the 0xc2 prefix). (2) The 0x8e
`STI.16.F32.F16 i11, i20, i0` decode was an overlapping-window illusion: the
true instruction starts at 0x8d (5B LDO); gold's real loop store is the
`LSU1.STI.16.F32.F16 (i7, i8)` at 0xaf whose base i8 is exactly the register
loaded here. Provenance chain COMPLETE for the output pointer:
`params(+0x28) -> i8 -> STI@0xaf -> out-window`.
(3) `+0x00` (in-ptr) remains ABSENT from gold: no family-form with imm 0
exists in .text (op classes fa/f9/f0/c9 = zero occurrences in gold at all).
The in-ptr producer is therefore either the LDXV (i0, i0) at 0x27 (i0
provenance = the unknown 1B `40` at 0x0a or the 0x1e-0x26 run), or the
fixture and gold genuinely diverge on the input-pointer read — ACT kernels
read it explicitly (fa-class), gold does not.  LOUD FLAG stands, narrowed to
the in-ptr only.

The 0x1e-0x26 run remains unknown-family (a3-prefixed; no corpus instance of
`a3 fc ff`), but it is NOT the out-read — that is solved.  Its templates
remain as in sec 14.

### Appendix: store-form candidate table (corpus_hits > 0, sorted desc)

```
mnemonic                  fmt B hits  instantiation -> bytes        files/conf
LSU1.STO.64               1  5  14   STO.64 i1,i2,2 ; fmt=1 -> c916040500  ef_j0,j1,jn,sqrt [table]
LSU1.STO.32               0  5  11   STO.32 i1,i2,2 ; fmt=0 -> c916440500  ef_j1,jn,sqrt,half,wf_j0/j1
LSU0.STX.64.H             1  4  11   i1,i2,i3 ; fmt=1 -> 821cc400          [repacked]
LSU0.STX.128.RGB1555      1  4   7   i1,i2,i3 ; fmt=1 -> 821cc468          [repacked]
LSU0.STI.16.U16.U8        2  4   3   i1,i2,i3 ; fmt=2 -> 0419c430          [table]
LSU0.STI.16.I8.I4         2  4   3   i1,i2,i3 ; fmt=2 -> 0419c440          [table]
LSU1.STA.64 fmt7/fmt5     5  2          -> c914200100                      [repacked]
LSU1.ST.64 fmt5 / LSU1.ST.32 fmt18     -> 89190401 (identical = tie)       wf_j0/jn
LSU0.STI.16.F16.U8FS fmt6              -> 821cc438
LSU1.STI.64.U8.U4S fmt1                -> 1319c4c8   ef_sqrt
LSU1.STI.16.F32.F16 fmt6               -> 49140408   GOLD 0xaf [repacked]
LSU1.STI.64.H fmt3                     -> 49140408   == SAME BYTES (tie w/ gold form!)
LSU0.ST.16.F32.F16 fmt1                -> 42240800   halfmath
... (full table: 54 store forms with hits>0; low-hit rows all reproducible via fmt hints)
```

Ties flagged: `LSU1.STI.16.F32.F16 fmt6` == `LSU1.STI.64.H fmt3`
(49140408); `LSU1.ST.64 fmt5` == `LSU1.ST.32 fmt18` (89190401);
`LD.32.F16.F32 fmt0` == `LD.64.H fmt0` (sec 13).

### Appendix: gold store operand raw bits (PleasedDog field-level probing)

```
0xaf  LSU1.STI.16.F32.F16 fmt6  bytes 49 74 10 08
      mask=0xfc00fff fixed=0x8000449 (value-space, raw_value)
      op0 reg @value[16,12] w4?? -> displayed i7  (bits: (v>>12)&0xf ... actual w=5 @12)
      op1 reg @value[21,17] w5 -> i8   (base = out-ptr from 0x8d)
      2 operands: NO increment register — increment semantics unknown
                  (implicit element-size post-inc vs none: unverified)
0x8e-window STI: RETRACTED (overlap with the 0x8d LDO); fmt1 3-reg raw bits
      for reference: op0 @13 w5, op1 @18 w5, op2 @23 w5 from bytes 82 64 51 00 30.
Corpus 3-op STI lines: STI.64.U8.U4S (i7, i23, i26), STI.128 (i11, i13, i13)
      — 3rd operand = a REGISTER (wild values), increment/stride semantics
      NOT decodable from bytes; hardware probing required.
```

## 16. Gold guard2 region 0x3a-0x66 decode

Full region table (greedy decode state; `.byte` = no form matches at mask>=2
any length 1-6 including the sec 15 family forms):

```
0x3a  81801205     CMU.CMII.I8 fmt4 (i8, i9)        mask22 [repacked; fmt5 same-mask tie]
0x3e  95 f1 01 eb 50  .byte 5 — unknown family (nothing at mask>=2)
0x43  ea4210       LSU0.LDIL fmt100 (i29, 0x1042)   mask3 weak (greedy pick)
      -- alternative boundary: 0x44 421000e0 LSU0.LD.32.F16.F32 fmt0 (i1, i0)
         mask18 [table], with ea@43 and 9c@48 as 1-byte tails --
0x46  00 e0 9c     .byte 3 — unknown (tail under the 0x44 alternative)
0x49  81c2524e     CMU.CMVVABS.I8 fmt1 (i22, i20)   mask18 [table]
0x4d  fb ff 50     .byte 3 — unknown family
0x50  415000       CMU.CMZ.I32 fmt7 (i5)            [repacked]
0x53  b5640005     BRU.BRA fmt102 (i5 -> 0xb7)       rel=0x64, 0x53+0x64=0xb7 = function exit
0x57  1b 40        .byte 2 — unknown (pair '1b 40'/'1b 50' also seen in ACT corpus)
0x59  819c0c08     CMU.CP.32 (i6, i9)               decode fmt15 (mask22 tie); encode pins fmt1
0x5d  3c062503     SAU.DIV.U32 fmt0 (i9, i8, i6)    [corpus 1]
0x61  c0           .byte 1 — unknown
0x62  81501200     CMU.CMII.U32 fmt4 (i5, i9)       [corpus 11]
0x66  b5510005     BRU.BRA fmt102 (i5 -> 0xb7)       rel=0x51, 0x66+0x51=0xb7
```

### Q2: dims null-check — NOT VISIBLE; lives in unknown runs or absent

Exhaustive scan of 0x0a..0x66 (all forms, mask >= 2, lengths 1-6, including
the sec 15 family entries) for any instruction whose inverted operands equal
7 (dims) or 10 (rank): only two artifacts — the known 0x4a LDO wild-imm
coincidence (base=7, imm=32765, overlaps CMVVABS) and a mask3 LDIL fake at
0x4b.  THERE IS NO GENUINE i7-REFERENCING INSTRUCTION ANYWHERE between the
dims read at 0x05 and the loop: dims is never compared (`!dims`), never
addressed (the `dim = dims[d]` load is absent too), never copied, in any
decodable form.  The guard skeleton that IS present: `0x50 CMZ i5 ->
0x53 BRA i5, 0xb7` and `0x62 CMII (i5, i9) -> 0x66 BRA i5, 0xb7` — both
fail-branches target 0xb7 = the BRU.JMP function exit (early-return on
guard failure, void function needs no value).  Verdict: either the `!dims`
check and the whole dim-loop addressing sit inside the unknown runs
(0x1e-0x26, 0x3e-0x42, 0x4d-0x4f — note 0x3e-0x42 and 0x4d-0x4f sit exactly
between the compares and the branches), or fixture/gold divergence omits it.
Given the unknown runs also hold every rank/count copy, unknown-family is
the stronger hypothesis; byte evidence above.

### Q3: rank>15 candidate (PleasedDog guard1 byte-compare target)

Best candidate chain (semantics-inferred, confidence MED):
```
0x18  420f00       LDIL (i8, 15)              the rank limit constant
0x3a  81801205     CMII.I8 (i8, i9)           '15 vs i9' — fits rank>15 IF i9 holds rank
      [i9 provenance gap: rank loads to i10 at 0x00; no known-form i10->i9
       copy exists; 0x2b-0x2e `18 53 ac ad` (4B, unknown) is the prime
       copy-candidate]                          [INFERENCE]
0x50  415000       CMZ.I32 (i5)                boolean -> i5
0x53  b5640005     BRA fmt102 (i5 -> 0xb7)     fail -> function exit
```
rank==0 check: not separately visible (would be CMZ/CMII on i10 or i9 —
absent from known forms in 0x0a-0x66); `rank == 0 ||` may fold into the
same compare (unsigned `rank-1 > 14` style) or live in unknown bytes.
PleasedDog's guard1: byte-compare their emitted sequence against
`42 0f 00 | 81 80 12 05 | 41 50 00 | b5 64 00 05` (with the i10->i9 copy
slot unresolved).

### Q4 bonus: 0x1e-0x26 + 0x27 — subop-variant constant loads

The run parses as 3-byte `[reg5 = byte0>>3 | subop3 = byte0&7][imm16 LE]`
members of the LDIL family with subop classes ABSENT from the DLL tables:

```
0x0b  0e88ac  subop6 (i1, 0xac88)   [known fmt101]
0x0e  0a0832  subop2 (i1, 0x3208)   [known fmt100]
0x13  6eea70  subop6 (i13, 0x70ea)  [known fmt101]
0x16  0c 01   subop4 (i1, 0x010c?)  [unknown — 2B window ambiguous]
0x18  420f00  subop2 (i8, 15)       [known]
0x1b  320100  subop2 (i6, 1)        [known]
0x1e  a3fcff  subop3 (i20, 0xfffc=-4)   [UNKNOWN subop3]
0x21  0f28c8  subop7 (i1, 0xc828)       [UNKNOWN subop7]
0x24  501000  subop0 (i10, 0x0010=16)   [UNKNOWN subop0]
0x26  00      tail byte
0x27  420100d2  LSU0.LDXV fmt0 (i0, i0)  mask13 [table]
```
subop 3-bit selector plausibly = unit/predication/width (cf. sec 15 unit
nibbles 2/6); the reg-bit hypothesis (byte0>>3) is proven only for subops
2/6 — if subop0's reg field sits elsewhere, `0x50 10 00` need not write i10
(which would otherwise overwrite rank).  Note `a3 fc ff` / `0f 28 c8` have
ZERO instances in the 187-kernel ACT corpus, but `50 10 00` appears 13x
(often as immediates).  In-ptr provenance hypothesis (LOW-MED confidence):
1B `40`@0x0a = fixed-reg move (params -> i0; a 1B form cannot encode two
general registers), `LDXV (i0, i0)`@0x27 = i0 <- mem[i0] = load32(params+0)
= the in-ptr, then unknown 0x2b-0x2e copies i0 -> i9 (the loop walker).

## 17. Guard2 tail 0x67-0x8c, imm=0 class validity, deref emit

### 1. Region 0x67-0x8c (dim-loop skeleton CONFIRMED structurally)

```
0x66-0x69  b5510005  BRA fmt102 (i5 -> 0xb7)      [guard exit; occupies 66-69]
0x6a       2b 40     .byte 2 — unknown
0x6c       c643fdff  IAU.INCS (i10, 0x3fff)        same bytes as 0x9d in the main loop
0x70       41a000    CMU.CMZ.I32 fmt6 (i5)
0x73       b5d7ff09  BRA fmt102 (i9 -> 0x4a)       rel -41: BACKEDGE 0x73 -> 0x4a = dim-loop
0x77       eb 30 bc e0 .byte 4 — unknown ('eb 30' pair also in ACT corpus)
0x7b       1c12a4    SAU.RCP fmt0 (i9, i32)        DECODE-SUSPECT: operand i32 > i30 impossible
0x7e       24 03     .byte 2 — unknown
0x80       07        BRU.INC fmt0                  HW-loop increment op
0x81       419000    CMU.CMZ.I32 fmt7 (i9)
0x84       b5330001  BRA fmt102 (i1 -> 0xb7)       third guard-exit -> function end
0x88       1b 50 e9 9d 24 .byte 5 — unknown
```

Structural verdict: the region CONTAINS A REAL LOOP: header ~0x4a, backedge
`0x73 BRA -> 0x4a`, counter increment `0x6c INCS (i10, 0x3fff)` (identical
bytes also at 0x9d inside the main loop = i10 = the recycled loop counter,
d and i in fixture terms), two guard-exits inside the loop (0x53, 0x66 ->
0xb7) plus one more at 0x84 -> 0xb7, and the div/convert arithmetic feeding
`0x5d DIV.U32 (i9, i8, i6)` / `0x62 CMII (i5, i9)`.  Therefore gold DOES
execute a dim-loop -> `dim = dims[d]` MUST be dereferenced somewhere — but
no KNOWN-form instruction references i7 anywhere in 0x0a..0x8c (exhaustive
scan, sec 16), so the dims load is one of the unknown-family bytes sitting
exactly at loop entry (strongest candidates: 0x46 `00 e0 9c`, 0x4d
`fb ff 50`, or the 0x57 `1b 40`/0x61 `c0` fragments).  Conclusion: dims
deref = YES by loop structure; its encoding instruction family is absent
from the DLL tables (BRU fmt102 precedent).  DIV provenance (partially):
i6 = LDIL@0x1b (count=1); i8 = LDIL@0x18 (15) with possible redefinition
in unknowns (16 needed for 16/count; the subop0 `50 10 00`@0x24 = 16 is a
candidate); i9 = both loop-bound source (@0x3a) and DIV destination (@0x5d)
— exact assignment needs the unknown runs.

### 2. imm=0 class validity — Main's premise partially corrected

Precise scan (gold + mlibm + 187 ACT files), pattern
`[class][reg][size-tag][01 00]`:

```
gold add1-fp16:            ZERO hits for ALL classes   <- gold never reads +0x00
mlibm:  c2 (byte2 64/66/27) 60+ hits (ef_j1 4, halfmath 8, math_lite 16,
        shave_fp64 25+34..., ef_jn 4+1), plus 08/b0/f0/c9/c3 variants
ACT corpus (per class imm0 instances / files):
        c2: 63 / 33 files    c9: 10 / 7    f0: 14 / 10   b0: 12 / 4
        08: 3 / 2    bb: 5 / 3    f9: 2 / 2    fa: 2 / 2    fc: 0    c3: 0
```

VERDICT: `c2 + imm0` IS corpus-proven (mlibm-heavy, 33 ACT files) — NOT
invalid; PleasedDog's c2-class null-offset deref was legitimate in general.
What holds for gold alone: gold contains NO imm0 read of ANY class — the
input-pointer read is absent from this binary regardless of opcode (divergence
or the unknown-form route via LDXV, sec 16).  fa is NOT uniquely "the imm=0
reader" (fa imm0 = only 2 instances; c2 = 63) — fa's fame comes from the
specific gather.elf in-ptr example.  For a null-offset deref, corpus ranks:
c2 (fmt0) > f0/b0/c9 > fa.

Class semantics: no width/address-space partition observed — every class
carries both 32-bit tags (0x64/0x66) and 64-bit tags (0x27), and all classes
cover the whole +0x00..+0x78 offset range.  Strongest correlation = unit
nibble: {c2, f9, fa, 01, bb, c3} -> unit 2; {c9, f0, fc, 08, b0} -> unit 6;
f9/fa/01 appear with both.  Frequencies (c2 4,615 vs fa 297) look like
compiler opcode allocation, not semantics.  Working assumption: classes are
interchangeable encodings of the same load with unit/port selection — treat
as UNPROVEN-EQUIVALENT; stick to the class instantiated in the corpus line
you are reproducing.

### 3. Verbatim deref emits (i5 base, imm=0)

```
LSU0.LDO.32 i5, i5, 0 ; fmt=0    -> c2524a0100   (c2, unit2 — most-proven class for imm0)
LSU0.LDO.32 i5, i5, 0 ; fmt=112  -> fa564a0100   (fa, unit6 — matches gather in-ptr class)
LSU0.LDO.32 i5, i5, 0 ; fmt=111  -> fa524a0100   (fa, unit2)
LSU0.LDO.32 i6, i19, 0 ; fmt=112 -> fa66660100   (gather.elf in-ptr, byte-exact)
```
Field map (sec 15): dst = byte1>>4, base = (byte2>>1)&0x1f, imm =
(byte3>>1)|(byte4<<7), byte3 odd-bit = fixed 1, unit = byte1&0xf.

## 18. Ladder-driven answers (rank guard, ST ambiguity, fmt110, ADD-dup)

### Q1: gold rank-guard 0x0a-0x49 — byte evidence says NO exit branch exists there

BRU-family byte0 values in encodings.json: {07, 13, 47, 87, 89, b5, c7}.
Scan of gold 0x0a..0x49: ZERO branch-family bytes (the lone 0xb4 at 0x31 is
the high byte of `LDIL i2, 0xb421` at 0x2f; 0x4a at 0x34 belongs to
`LDIL i21, 0x14a`).  Combined with the exhaustive operand scan (sec 16: no
i10/rank instruction outside LDO@0x00), gold contains NO rank compare or
rank-guard exit between the dims read at 0x05 and the dim-loop header ~0x4a.
The two in-loop guards that DO exist match the fixture's dim guards exactly:

```
0x50 CMZ.I32 (i5) -> 0x53 BRA (i5 -> 0xb7)   = `dim == 0 -> return`
0x62 CMII (i5,i9) -> 0x66 BRA (i5 -> 0xb7)   = `dim > 16/count -> return`
(0x84 BRA (i1 -> 0xb7) = post-loop exit; backedge 0x73 -> 0x4a, INCS i10 = d++)
```

Verdict: for `rank == 0 || rank > 15 || !dims` pre-loop guards, gold's
compiled binary has (a) no decodable exit branch, and (b) no decodable
rank/dims operands in range — either they were compiled out (rank=0 then
falls into a zero-trip dim-loop, count stays 1, main loop writes one element
— graceful without an explicit guard) or they hide in unknown bytes.
DECIDING TEST: run the GOLD fixture itself under the carrier that serves
+0x0c = 0 (their runtime content) — gold's writes-or-not settles it without
any decode.  Also: the fixture has NO +0x08 field (params = +0x00 in,
+0x0c rank, +0x10 dims, +0x28 out) — the ladder's "+0x08 == 1 in file" read
is not from add1-fp16.c; treat +0x08 content as driver-layout, not fixture.

### Q2: STI/ST register-combo ambiguity — mask tables

```
49640a08 (their STI i6,i5):
  mask22 LSU1.ST.16.I8.I4 fmt3 / fmt1   <- decode picks these (richer mask)
  mask18 LSU1.STI.64.H fmt3 (chits1) / LSU1.STI.16.F32.F16 fmt6 (chits1, GOLD)
  => decode label depends on mask ranking only; encode for the mnemonic
     LSU1.STI.16.F32.F16 resolves among ITS OWN forms -> fmt6 (correct bytes).
     The mislabel is decode-cosmetic; keep ; fmt=/raw= annotations as truth.
49741008 (gold): only mask18 pair (STI.64.H fmt3 / STI.16.F32.F16 fmt6) — tie,
  fmt6 corpus+gold-anchored.
42c41400 (faulting isolated ST): ops = (i6, i5) under ST.16.F32.F16 fmt1
  (mask22); re-encode fmt1 == fmt3 == 42c41400 (byte-identical).  Form is the
  same one that occurs in halfmath (i21, i14) — encoding proven; a 3-instruction
  fault therefore points at the ADDRESS (alignment/segment) or operand order
  (value-first remains an inference from family convention), not form choice.
  Discriminator: run the halfmath register pair verbatim (value i21, base i14)
  and a 2-byte-aligned base in the same harness.
```
Encode-side "only proven instances" restriction is not implementable as a
form rule (operand values are not invalidity); the practical contract = decode
lines keep `; fmt=N raw=...` so re-assembly never depends on the mislabel.

### Q3: fmt110 vs fmt0 — op byte is REAL semantics (sec 17 revised)

Identical source `LDO i5, i18, 0x28`: fmt0 (c252645100) passes,
fmt110 (0152645100) faults — operand fields are identical, only byte0
differs.  Combined with gold@8d executing fine in gold's own context (and
415 corpus instances of op=01), the working assumption from sec 17
("classes interchangeable") is WRONG at runtime: opcode class carries real
behavior (mode/address-space/predication), class equivalence is UNPROVEN.
Guidance: emit c2 (fmt0) for all new reads (100% corpus-proven, ladder-
passing); fmt110-114 remain decode/annotation forms for reproducing
existing binaries (gold@8d = 01-class stays correct for disasm+roundtrip).

### Q4: LDIL/LDIH i3 + dst-duplicated IAU.ADD — encoding exonerated

* LDIL i3 `1afdff` = byte0 0x1a = (3<<3)|2 = dst i3 subop2 — exact family
  layout; LDIH i3 `a91bffff` = 0xa9 + (3<<3)|3 — corpus-verified pattern.
* corpus IAU.ADD fmt2 lines: 19 total, **16 dst-duplicated** — dst==src
  fmt2 instances are normal vendor output; fmt2 fields = dst@18, a@13, b@23,
  mask 0xf0001fff fixed 0xc6.
* z23 passes while executing the SAME i3 pair; z24d fails with the SAME
  `c6008400` bytes that pass at 0x0c in z23/z24 — so no byte string is
  individually invalid; the fault is (LDI-pair earlier) x (dst-dup fmt2
  later) sequence-dependent.  Encoding review finds nothing: both components
  independently corpus-proven.  Remaining suspects: VLIW bundle-slot packing
  around 0x43-0x4e (z23 packs pair+JMP, z24 pair+ADD+JMP — check bundle
  boundaries in the AsmPrinter output) or a hardware interlock.
  Suggested discriminator: dst-dup fmt2 ADD placed BEFORE the i3 pair in the
  same bundle layout, and pair+dst-distinct with identical bundle padding
  (they report dst-distinct passes — that asymmetry plus 16 corpus dst-dup
  instances argues the interaction, not the encoding).

## 19. Vendor field-map correction for ST/STI.16.F32.F16 (TRACE_REPORT Q1)

Provenance: /home/asdf/projects/npunlock-work/rptrace/TRACE_REPORT.md Q1
[STATIC-RE] encoder record list `{D,0x40,9}{IR,1,5}{IR,0,5}{D,0x442,13}`
+ [VENDOR-PROBE] register sweep.  In byte/LE space (what shave_asm encodes):

```
value (op0) = LE bits[16:12]      (word bits20-23 + word bit8)
base  (op1) = LE bits[21:17]      (word bits13-9)
STI bit     = LE bit27            (word | 0x08)
LSU0 base word 0x42040000 / STI 0x42040008 ; LSU1 0x49240000 / 0x49240008
```

Our previous map (op0@[18,14], op1@[13,9] canonical) was wrong; symptom:
`ST.16.F32.F16 i6, i5` emitted `42c41400` = vendor-decodes as (i12, i10) —
wrong registers → the isolated hardware fault.

Fix (cal_fix.py, value-space step, entries normalized to
`first-at-top-repacked` so no remap is applied at load):

```
LSU0.ST.16.F32.F16  i6, i5    -> 42640a00   (vendor-exact)
LSU0.ST.16.F32.F16  i7, i8    -> 42741000   (vendor probe)
LSU0.ST.16.F32.F16  i8, i7    -> 42840e00   (vendor probe)
LSU1.STI.16.F32.F16 i6, i5    -> 49640a08
LSU1.STI.16.F32.F16 i7, i8    -> 49741008   (gold .text+0xaf, byte-exact)
halfmath site                   42a43a00 now decodes (i10, i29) [was (i21,i14)]
old emitted 42c41400            decodes (i12, i10) — matches PleasedDog's fault
```

Gold roundtrip after the change: 192/192 IDENTICAL (line 0xaf re-decodes as
`LSU1.STI.16.F32.F16 fmt=6`); decode 135/192 (70.3%); regression suite 15/15
byte-exact (LDIL/LDIH/CP.32/SAU/LDO fmt0+fmt110/ST/STI/AND/DIV/LD-F16).

Sibling audit (vendor 2-field map vs our forms on real corpus raws):
`LSU1.ST.32 fmt18` (10,18)==(10,18); 3-op families `STX.32.I32.I8S` (5,20),
`STX.64.I32.I16S` (1,20), `STI.64.U8.U4S` (7,23), `STO.32` (30,19),
`STO.64` (28,19) — op0/op1 AGREE with vendor in every case.  Only the
ST/STI.16.F32.F16 plain forms carried the wrong positions; nothing else
changed.

Side-findings from the full pipeline rerun (gen && patch && cal_fix):

* `CMU.CP.32` gen fmt1 encodes `819c8c00` (NOT gold); the gold convert bytes
  `819c0c08`/`81dc2508` = repacked fmt15 (21 decode hits) — pin moved:
  fmt15 encode_priority -1, fmt1 demoted to 2.
* `LSU0.LDO.32` fmt0 pinned encode_priority 0 (48 decode hits) so hintless
  reads stay on the c2 class, not the fmt110-114 family forms.
* Hand-recovered forms (LDIL fmt100/101, LDIH fmt100, BRU.BRA fmt102) do not
  exist in gen output — cal_fix.py `ensure()` now recreates them every run;
  the pipeline gen -> patch -> cal_fix is self-healing (running gen alone
  regresses everything, as documented in sec 10).

# TRACE_REPORT — vendor SHAVE semantics for the fp16 store kernel

Object: our backend's build of `add1-fp16.c`
(`npunlock/tests/fixtures/npu3720/add1-fp16.c`, gold `add1-fp16.elf`, md5
`a363411057bbb0d0fbf8ab09ae757015`). Executes, writes **zero bytes** to the NPU
output window; gold passes the oracle. Verbatim evidence in
`rptrace/SUPPORTING.md` (§n).

Provenance: `[STATIC-RE]` decompile/string corpus read back now; `[PROBE]`
vendor-encoder probe (retired method, bytes re-checked against the DB); `[DB]`
`shave-isa/encodings.json` md5 `1568e902…` re-encoded via `shave_asm.py`;
`[INFERENCE]` reasoned. Sources: `movicomp/decompiled_all.c` md5 `3c35b678…`,
`moviAsm64/decompiled_all.c` md5 `4b800930…`, `moviCompile64.ascii.txt` md5
`1f4c45d3…`.

---

## Q1 — STORE SEMANTICS — HIGH

Gold `.text+0xaf` = `49741008` = `LSU1.STI.16.F32.F16 i7, i8` `[DB]`. `i7` is
the value loaded at 0x98; base `i8` is the out pointer read at 0x8d.

* **Order = `<value>, <base>` (value first).** `[STATIC-RE]` the def record list
  for `LSU0.STI.16.F32.F16` is `{D,0x40,9}{IR,1,5}{IR,0,5}{D,0x442,13}`
  (`moviAsm64/decompiled_all.c:370964-370979`) — operand 0 (value) is the first
  `IR` record. `[DB]` fmt6 bitfields = `value@[16:12]`, `base@[21:17]`; disasm of
  `49741008` → `i7, i8`. Printer text `"$src, $addr"`
  (`SHAVEInstrInfo.td:243`) agrees.
* **Value class = integer `iN` (IR file), NOT vector.** `[STATIC-RE]` every
  operand record of the `:238212` block points at `DAT_180692eb4`, which
  resolves at VA `0x180692eb4` to the 2-char string `"IR"` (neighbours `…ea8
  "VR"`, `…eac "VW"`, `…eb0 "IW"`, `…eb8 "I"`, `…ebc "V"`). `[PROBE]`
  `LSU1.STI.16.F32.F16 v7.0, i8` is rejected (*expected format: I<regNo>*).
  **Our `iN` choice is correct.**
* **No increment operand; STI bumps by one element = 2 bytes.** `[DB]` fmt6 has
  exactly 2 operands; `49741008` vs `49741000` (`ST`, non-increment) differ only
  in bit 3 of byte 0. Vendor *does* define 3-op STI forms (`[DB]` fmt0 = 3 ops
  `[[19:15]],[[14:10]],[[9:5]]`, 4 B; fmt1 = 3 ops, 5 B) whose third operand is
  an **increment register**: `[PROBE]` `LSU1.STI.16.F32.F16 i7,i8,i9` →
  `89745002`. Gold uses none of those.
* **`(i6,i5)` labelling `ST.16.I8.I4` while `(i7,i8)` labels STI fmt6 is a decode
  tie-break, not a semantic rule.** `[DB]` `49640a08` matches two forms at equal
  mask popcount 22 — `LSU1.ST.16.I8.I4` fmt3 (index 9366) and
  `LSU1.STI.16.F32.F16` fmt6 (index 9551); the sort `shave_asm.py:220` =
  `(-mask.bit_count(), -length_bytes)` keeps DB order, so **the earlier index
  wins**. `49741008` has only the one 22-bit match, so it labels right.
  `49640a08` *is* the correct encoding of `LSU1.STI.16.F32.F16 i6,i5`.
* **Encode-side choice = corpus-hit preference.** `[DB]` `shave_asm.py:385-388`
  sorts by `(encode_priority, has_corpus_hits?0:1, -mask.bit_count(),
  -corpus_hits)`. Measured: `LSU1.STI.16.F32.F16 i7,i8` → `49741008` (fmt6 =
  gold anchor, 1 hit) **but** `LSU0.STI.16.F32.F16 i7,i8` → `4078100c` (fmt2,
  0 hits). `[INFERENCE]` an `LSU0.` STI without `; fmt=6` emits non-gold bytes.
  `LSU0.ST.16.F32.F16 i7,i8` → `42741000` is fine (fmt1 has a hit).

## Q2 — REGISTER FILES — HIGH for this kernel

* `[STATIC-RE]` classes `Generic::ScalarRC`/`Generic::VectorRC`
  (`moviCompile64.ascii.txt:78365-78366`), physical views `IRF8_q0..q3`,
  `IRF16_h/l`, `VRF16_e0..e7`, `VRF32_q0..q3`, `WVRF*` (`:79259-79291`), bridge
  node `LSU_CP16_vrf_irf0` (`:79309`). Two files exist; the scalar path is one.
* `[PROBE]` grammar is two-syntax: `iN` vs `vN.S`
  (`moviAsm64.ascii.txt:2603,2660`; VA `0x180692ebc` = `"V"`, `:ec0` =
  `"<regNo>"`, then `"<regNo>.<subregNo>"`). Plain `LSU0.ST.16 i7, i8` is
  rejected (*expected format V<regNo>.<subregNo>*) — that form is vector.
  `ST.16.F32.F16`/`STI.*`/`SAU.ADD.F32`/`LD.32.F16.F32` all take `iN`.
* `[DB]` `SAU.ADD.F32 i7,i7,i1` → `3ce09c30` (= gold 0xaa);
  `LSU0.LD.32.F16.F32 i7,i9` → `42701200` (= gold 0x98). Both IR file.
* **Answer: no separate FP file is involved; our shared-`iN` model
  (`SHAVERegisterInfo.td:53-67`, GPR/GPRF32/GPRF16 all R0..R30) is correct. The
  zero-write bug is NOT a register-file mismatch.**

## Q5 — LDO OPCODE-CLASS BYTE — MEDIUM, one point unresolved

```
gold 0x00  c2a2641900  rank,    +0xc   = fmt0    [DB] re-encode exact
gold 0x05  c272642100  dims,    +0x10  = fmt0    [DB] re-encode exact
gold 0x8d  0182645100  out-ptr, +0x28  = fmt110  [DB] decode: dst i8, base i18, imm 0x28
```

* `[DB]` identical operands encode under four classes with **byte-identical
  operand fields**: fmt0 → `c282645100`, fmt110 → `0182645100`, fmt113 →
  `f912645100`, fmt111 → `fa62645100`. Byte 0 carries state the operand
  bitfields cannot express.
* `[PROBE]` the unit nibble does not explain it: `LSU1.LDO.32` fmt0 byte0 =
  `c9`. Corpus class counts: `c2` 6932, `c9` 2174, `f9` 1305, `f0` 1239, `fc`
  440, `08` 419, `01` 415, `fa` 343, `bb`, `b0`.
* `[STATIC-RE]` `FUN_180434100` (Myriad2Platform builder, 458005 B,
  `moviAsm64/decompiled_all.c:170229`) **failed to decompile** — any
  Myriad2-only class constant is missing from the table (`ENCODING_NOTES.md`
  §7.1/§15).
* **What the class selects: unresolved.** Best-supported reading
  (`ENCODING_NOTES.md` §18 Q3, revised): fmt0 (`c2`) is runtime-proven for new
  reads while fmt110 (`01`) as a new read faulted, yet gold's own `01` read at
  0x8d runs fine in context ⇒ class is real semantics (port/unit/mode), its
  equivalence unproven, its mapping unreadable from the tables. `[INFERENCE]`
  gold@0x8d class `01` vs our `c2` is a genuine divergence, but the evidence
  does not rank it above the store bug as the cause of the zero write.

## Q3 — GUARD CODEGEN — MEDIUM

```
0x50 415000    CMU.CMZ.I32 i5      -> 0x53 b5640005 BRU.BRA i5, 0xb7
0x62 81501200  CMU.CMII.U32 i5, i9 -> 0x66 b5510005 BRU.BRA i5, 0xb7
0x73 b5d7ff09  BRU.BRA i9, 0x4a    (loop backedge)
```

* These two are the **dim-loop** guards (`dim == 0`; `dim > 16/count`).
  `[STATIC-RE]` `ENCODING_NOTES.md` §18 Q1: a byte scan of gold 0x0a–0x49 finds
  **zero** branch-family bytes ⇒ gold has **no decodable pre-loop rank/dims exit
  branch**; it was either compiled out (rank 0 → zero-trip loop, count stays 1)
  or hides in the undecoded 0x0a–0x49 runs.
* Idiom: `CMU.CMZ.I32`/`CMU.CMII.U32` set a flag register, then a **predicated**
  `BRU.BRA <flag>, <target>`; vendor negates by branch sense, never with
  `IAU.XOR i,1`. Our emit uses `LDIL/LDIH 0xFFFF; IAU.ADD; CMU.CMII.U32; LDIL 1;
  IAU.XOR; BRU.BRA` — equivalent, structurally different.

## Q4 — LOOP IDIOM — MEDIUM (context)

Gold: `LD.32.F16.F32 i7,i9` (0x98) → `SAU.ADD.F32 i7,i7,i1` (0xaa) →
`STI.16.F32.F16 i7,i8` (0xaf, out++ implicit) → `IAU.ADD i9,i9,2` (0xb3, in++
explicit) → `CMU.CMZ.I32` + `BRU.BRA` backedge. Ours: same load/add, then
**`LSU0.ST.16.F32.F16 i3,i0` (non-increment)** → **two** explicit `IAU.ADD`s
(`i2,i2,2` and `i0,i0,2`) → `XOR`-inverted predicate. `[INFERENCE]` the doubled
explicit bump plus non-increment store is the divergence that matters; LSU0-vs-
LSU1 is secondary.

---

## Corrections to the previous revision

* `42c41400` is **not** our `(i6,i5)` store encoding. `[DB]`
  `LSU0.ST.16.F32.F16 i6,i5` → `42640a00` (fmt1); `[DB]` decode of `42c41400`
  gives `i12, i10`. The `(i6,i5)→ST.16.I8.I4` observation is the **LSU1**
  `49640a08` case — a decode tie-break, not an encoder fault.
* The old Q3 block mislabelled gold's 0x50/0x62 branches as a pre-loop rank
  guard; they are dim-loop guards, and no pre-loop exit is decodable.
* `LSU0.STI.16.F32.F16` becomes `4078100c` without `; fmt=6` — a new trap for
  whoever switches the backend to STI.

## Backend actions (ordered)

| # | Change | Evidence |
|---|---|---|
| 1 | Lower `out[i] = in[i]+1.0f` to `LSU1.STI.16.F32.F16 <iN>, <iN>`, no explicit out++ (drop `IAU.ADD i0,i0,2`); keep in++ explicit | Q1, Q4 |
| 2 | Keep `iN` for f32/f16 across `SAU.ADD.F32`/`LD.32.F16.F32`/`STI.*` | Q2 |
| 3 | Pin the STI form (`; fmt=6`) or use the LSU1 spelling, else the corpus-hit rule picks fmt2 (`4078100c`) | Q1 |
| 4 | Guard: `CMU.CMZ/CMII` + direct predicated `BRU.BRA`; drop the `XOR` flip | Q3 |
| 5 | New `LDO.32` reads: class `c2` (fmt0, 47 hits); keep class `01` only to reproduce gold bytes | Q5 |
| 6 | Emit legal 3-slot pipes (`A \|\| B \|\| C`, ≤128 bits) so store slot bits are correct | SUPPORTING §7 |

## Unresolved (stated, not guessed)

1. What LDO opcode-class byte 0 selects (port/mode) — the Myriad2 builder is
   undecompiled (`FUN_180434100`).
2. Whether gold@0x8d's `01`-class out-pointer read is *required* for oracle
   success or just the vendor's habitual class at that site.
3. Gold's undecoded runs 0x0a–0x49 / 0x1e–0x39 (pointer setup) and 0x53–0x8e
   (guarded region) — no recovered table form matches them.

# VALIDATION — shave_asm.py / encodings.json against real binaries

All outputs are verbatim runs in `/home/asdf/projects/npunlock-work/shave-isa/`.

## Gate results (current)

`python3 gates.py` = the regression + legality authority (G1-G8, 128 checks):

| gate | target | result |
|---|---|---|
| G1 gold disasm->asm byte-identical roundtrip | 192/192 | **192/192 IDENTICAL — MET** |
| G2 mlibm .o disasm->asm roundtrips | byte-identical | **13/13 MET** |
| G3 encode anchors (+ decode-back) | all exact | **39/39 MET** |
| G4 branch delay slots (6, idempotent, `.byte 0x07`, BRU.BRA descriptor) | exact | MET |
| G5 load latency >= 6 slots in the emitted bytes | exact | MET |
| G6 LSU unit policy (LSU0 loads / LSU1 stores, conflict reporting) | exact | MET |
| G7 pipe shape (<=3 slots, <=1 op/unit, <=128 bits) | exact | MET |
| G8 verbatim mode + CLI equivalence | exact | MET |
| gold .text instructions-decoded bytes | >=90% | **132/192 = 68.8%** — NOT met (composition improved: 0xaa/0x8e/0xaf-0xb7 now real instructions, some former fake-LDIL matches dropped) |
| mlibm .text roundtrip lengths exact | exact | MET (all tested files) |
| mlibm instruction decode % | >=95% | NOT met: ef_j0 82.2%, halfmath 75.0%, ef_sqrt 66.8%, math_lite 66.7%, shave_fenv 34.6% (repacked-form boundary shifts after the value-space fix; see sec-12 note) |

## (a) gold .text disassembly (run paste, head)

```
$ python3 shave_asm.py disasm --elf .../npu3720/add1-fp16.elf
; decoded 132/192 bytes (68.8%)
0x0000: c2a2641900   LSU0.LDO.32 i10, i18, 0xc    ; fmt=0 raw=c2a2641900
0x0005: c272642100   LSU0.LDO.32 i7, i18, 0x10    ; fmt=0 raw=c272642100
0x000a: .byte 0x40    ; undecoded 1 byte(s)
0x000b: 0e88ac       LSU0.LDIL i1, 0xac88    ; fmt=101 raw=0e88ac
0x000e: 0a0832       LSU0.LDIL i1, 0x3208    ; fmt=100 raw=0a0832
0x0011: .byte 0x00, 0x00    ; undecoded 2 byte(s)
0x0013: 6eea70       LSU0.LDIL i13, 0x70ea    ; fmt=101 raw=6eea70
0x0016: .byte 0x0c, 0x01    ; undecoded 2 byte(s)
... (0x0018..0x0039 mixed LDIL/LDIH pairs + short undecoded fragments)
0x003a: 81801205     CMU.CMII.I16 i20, i4    ; fmt=2 raw=81801205
0x0044: 421000e0     LSU0.LD.32.F16.F32 i1, i0    ; fmt=0 raw=421000e0
0x0053: b5640005     BRU.BRA i181, 0xb7    ; fmt=102 raw=b5640005
0x0059: 819c0c08     CMU.CP.32 i6, i9    ; fmt=1 raw=819c0c08
0x005d: 3c062503     SAU.DIV.U32 i9, i8, i6    ; fmt=0 raw=3c062503
0x006c: c643fdff     IAU.INCS i10, 0x3fff    ; fmt=0 raw=c643fdff
0x007b: 1c12a4       SAU.RCP i9, i32    ; fmt=0 raw=1c12a4
...
```

Semantic plausibility vs `controlled_act`: the decoded head is exactly right —
two `LSU0.LDO.32 iD, i18, <0xc/0x10>` = `load32(params+0xc)` / `+0x10` (i18 =
arg reg); later `BRU.BRA iN, label` loop/conditional branches, `CMU.CMII*`
compares (rank/dim checks), `SAU.DIV.U32` (16/count), `IAU.INCS`, stores.
Remaining undecoded runs are families documented in ENCODING_NOTES §7.

## (b) gold roundtrip — GATE MET

```
$ python3 shave_asm.py disasm --elf .../add1-fp16.elf 2>/dev/null > /tmp/gold.s
$ python3 shave_asm.py asm /tmp/gold.s --hex > /tmp/gold.out
GATE gold roundtrip identical: True ndiff: 0
```

Byte-exactness mechanism: every disasm line carries `; fmt=N raw=<chunkhex>`;
asm re-selects the exact form via `fmt` and merges don't-care (`X`) bits from
`raw` (`v |= raw & ~mask`), so re-assembly reproduces all 192 bytes including
vendor don't-care slots (gold bytes 0x45/0x63/0x7b/0x99/0xb0 previously lost
are now restored).

## (c) mlibm .text roundtrips (run paste)

```
; decoded 1433/2144 bytes (66.8%) <- ef_sqrt
; decoded 1573/1913 bytes (82.2%) <- ef_j0
; decoded 51/68 bytes (75.0%) <- halfmath
; decoded 8/12 bytes (66.7%) <- math_lite
; decoded 9/26 bytes (34.6%) <- shave_fenv
```

Roundtrip lengths exact on every file (undecoded bytes re-emitted through
`.byte`). Instruction-decode % on mlibm: **best 82.4% (halfmath) — target >=95% NOT met**
(caveat: some short `LSU0.LDIL fmt=100/101` matches use the 3-bit calibrated
sub-opcode and may over-match; ranked below any higher-information form).

## Independent cross-checks validating the model

1. `IAU.AND i4,i1,i2` <-> `82044308` byte-exact both directions.
2. Interval-DP (validate_efsqrt.py, msb_le + DWARF/type-209 boundaries):
   exact fixed-bit matches on real code for CMU.CMII.U32 `81 90 10 00`,
   IAU.AND `86 50 26 03`, CMU.CMZ.I32 `41 90 00`, LSU0.LDO.64 `c2 62 27 e1 00`,
   LSU0.LDIL `82 4a 00 00`, IAU.INCS, SAU.SUB.I8, IAU.OR, IAU.ADD/SHL.
3. LDI pair calibration: across ALL mlibm type-202 sites (n=739) the
   high-half instruction is `[0xa9][subop3=3|reg<<3][HI16 LE]` (byte0=0xa9 in
   ~700/739, byte1&7==3 in all), low-half `[subop3|reg<<3][LO16 LE]` 3-byte
   with subop=2 (LSU0) / 6 (LSU1, the 695x variant). Recalibrated forms
   fmt=100/101/102 encode these; ef_sqrt pair tiles exactly (0x79e/0x7a1/0x7a5).
4. Reloc cross-check (LinkerRelocRE): type-209 `field = r_offset -
   instruction_start`; decoded boundaries agree with every type-209 start used
   as DP constraint.

## Blockers (why <90%/<95%)

Unchanged in kind, now with calibrated partial answers (ENCODING_NOTES §7):

* **LSU1.LDIH opcode byte**: def-constant 0x389/0x382 (decompile-verified)
  emits byte0=0x89/0x82, but every real binary shows byte0=0xa9 — recovered as
  calibrated form (fmt=100) from 700+ corpus instances; root cause (bit-5/unit
  overlay vs DLL revision delta) unresolved.
* **24-bit branch `[0xb5][rel16][operand@byte3]`**: recovered as inferred
  form BRU.BRA fmt=102 (`BRU.BRA iN, label` — matches every type-209 site in
  gold: 0x53/0x73/0x84/0xa1...); no such def exists in the DLL (exhaustive
  search) — labelled `inferred`.
* **Prologue run at 0x0a (12 bytes)** and assorted per-function prologue/epilogue
  and loop sequences still match no form.
* Don't-care `X` bits: resolved via `raw=` roundtrip (not silently guessed —
  the raw bytes are printed and re-emitted verbatim).

## (d) Encode-side verification (form selection, hintless path)

Exact-byte repros (in-process `assemble()`, fresh runs):

```
ENC LSU0.LDIL i16, 0x0E        -> 820e00        (3B fmt=100)
ENC LSU1.LDIH i16, 0x00        -> a9830000      (4B fmt=100, corpus layout)
ENC IAU.ADD i19, i19, 0x28     -> c6604e94      (EXACT vs ef_j0 raw)
ENC IAU.SUB i19, i19, 0x28     -> c6614e94      (EXACT vs ef_j0 raw)
ENC IAU.AND i1, i8, i1         -> 86108500      (EXACT vs ef_j0 raw)
ENC SAU.DIV.U32 i9, i8, i6     -> 3c062503      (EXACT vs gold raw)
ENC LSU0.LD.8 i2, [i0]         -> 425d000e      (4B)
CMU.CP.32 i18, i29 ; fmt=1     -> 81dc2508      (EXACT; fmt-less pick differs)
CMU.CMZ.I32 i5 ; fmt=0         -> 41a000        (EXACT)
LSU0.LDIL i1, 0xac88 ; fmt=101 -> 0e88ac        (EXACT)
```

Form-selection machinery (all changes verified against the gates above):

* operand-kind filter (`reg`/`mem`/`imm` vs form field kinds) — imm operands
  no longer land in reg-only forms;
* corpus stamps REBUILT decode-anchored (`cal_fix.py` recount over our own
  disasm listings, 130 mnemonics / 787 forms) — patch_encodings.py's original
  stamps disagreed with decode-side ranking and mis-routed IAU.ADD/SUB/AND;
* sort: encode_priority, corpus-presence, mask richness, corpus frequency;
* candidate fallback in `asm` (first form that encodes wins; clean error only
  if none fit);
* pass-1 rel placeholder (absolute target no longer forced through a 5-bit
  rel field during length collection).

Per-mnemonic single-line re-encode (stripped disasm line -> asm -> byte
compare, all 13 mlibm .o + gold; non-rel mnemonics):

```
IAU.ADD 27/27 100%    IAU.SUB 7/7 100%    IAU.AND 4/4 100%
IAU.SHL 19/19 100%    IAU.INCS 3/3 100%   SAU.DIV.U32 1/1 100%
LSU0.LDO.64 76/76 100%  LSU1.LDIH 43/50 86%  CMU.CMZ.I32 5/8 62%
CMU.CP.32 6/11 55%      LSU0.LDIL 193/376 51%
```

Why the sub-100% rows are structural, not encoding bugs:

* `LSU0.LDIL` has two corpus-real 3-byte subop variants (fmt100 subop=2,
  fmt101 subop=6) whose operand text is IDENTICAL — hintless asm picks one
  (fmt100, 193 sites) so fmt101 sites (172) mismatch; with `; fmt=101` both
  are byte-exact (see repros).
* `CMU.CP.32` fmt1/fmt2 (and `CMZ`) are same-shape same-length alternatives;
  emit `; fmt=1` (gold/ef_j0 both decode these sites as fmt1) for exactness.
* Hintless whole-listing re-encode (bytes incl. `.byte` passthrough), fresh:

```
add1-fp16.elf 81/135 = 60.0%   ef_j0 553/1554 = 35.6%   ef_sqrt 213/1573 = 13.5%
halfmath 50/56 = 89.3%         shave_fenv 15/17 = 88.2%  TOTAL 913/3343 = 27.3%
```

  (Rel-operand lines amplify any single form mismatch into pc-drift for the
  rest of the file; hinted roundtrip — the supported disasm->asm path — is
  192/192 on gold and byte-exact by construction on every file.)

`asm` now tries candidate forms in resolve order until one encodes the given
operand widths (clean `no form encodes operands` error instead of a traceback).

### Addendum: memory operands with displacement

`[iN]` selects the 4B mem form; `[iN+off]` previously DROPPED `off` silently
(same bytes as `[iN]` — regression found via ISelConverge's emission probe) and
now expands to an implicit immediate operand, routing to the 5B
displacement-carrying form:

```
LSU0.LD.8 i2, [i0]      -> 425d000e        (4B, fmt14)
LSU0.LD.8 i2, [i0+4]    -> 80850021ac      (5B, fmt19; `; fmt=19` gives the same)
LSU0.ST.32 i4, [i6]     -> 42999803        (4B, fmt12)
LSU0.ST.32 i4, [i6+8]   -> a00019223c      (5B, fmt13)
```

Gates after the fix: gold roundtrip 192/192 identical; ef_j0 81.2%;
halfmath 82.4% (unchanged).

### Addendum: BRU.JMP pin (gold epilogue anchor)

Gold .text+0xb7 holds `47 f1 01` = `BRU.JMP i30`. The other table form
(fmt=1) encodes `01fe28`; patch-era corpus stamps favored it (1 hit vs 0),
so `cal_fix.py` pins fmt0 `encode_priority=-1`:

```
BRU.JMP i30  -> 47f101    (hintless, matches gold epilogue bytes)
```

Caveat: gold's 0xb6 is currently over-matched by a 4B `CMU.CP.128` entry
(mask-poor), so the decode listing shows the jmp as the tail of that line;
the encode path is unaffected and the hinted roundtrip (which restores via
`raw=`) still reproduces all 192 bytes. Corpus stamps now rebuilt over
gold + all 13 mlibm .o (171 mnemonics / 1,049 forms).

### Addendum: HW-verified 4-byte LDI forms (Mockingbird silicon, PACKET_SPEC)

The 3-byte corpus-derived `LDIL` forms (fmt100/101) DO NOT execute on
hardware (raw subop sweep: no write / write 0x00800000 / trap) — they are
mis-segmented payloads.  cal_fix now pins the 4-byte table forms:

```
LSU0.LDIL i2, 0xABCD   -> 8212cdab    [82 | (reg<<3|2) | lo16]
LSU0.LDIH i4, 0x5678   -> 82237856    [82 | (reg<<3|3) | hi16]
LSU1.LDIL i4, 0x5678   -> 89227856    [89-class lo16]
LSU1.LDIH i4, 0x5678   -> a9237856    [a9 | (reg<<3|3) | hi16]
LSU0.LDIL i20, 0x1234  -> 82a23412    (reg>=16 OK; pair lo->hi verified
LSU1.LDIL i20, 0xABCD  -> 89a2cdab     adjacent or >=2 slots apart, 0x1234ABCD exact)
```
3B forms demoted (encode_priority 4) so hintless resolve can never pick them;
gold's own 3B-LDIL decode lines are unaffected (decode = mask-match, not
priority) => gold roundtrip still 192/192, decode 135/192.

Also pinned this round (regression 19/19 green): CP.32 fmt15, SAU fmt3,
ST/STI vendor map, LDO fmt0/110, IAU ADD/AND, BRA/JMP, LD.32.F16.F32.

Store-unit rule (Mockingbird HW): LSU0 load + LSU0 store in one stream
corrupts (vendor diagnostic "LSU0/LSU1 unit is used more than once in the
same pipe"); store on the OTHER unit works (gold uses LSU1.STI).  Backends
must pair LSU0 loads with LSU1 stores (prefer-lsu1 default).

### Addendum: packets probe cross-validation (BiologicalMockingbird VERIFIED_EXAMPLES)

Regenerated probes from npunlock-work/packets/probes/ with the pinned tool:
`r5`, `u1`, `n6` are byte-for-byte EXACT against VERIFIED_EXAMPLES.md
(r5 = `c212645100` + 6x`c620a504` + `8212cdab` + `82133412` + 6x`c620a504`
+ `89290201` + `47f101`; u1 store `82090008`; n6 store `89090001`);
`v6` selects the `82a3cdab` (LSU0.LDIH) variant of the allowed pair;
`r2/p2/p4/u5/s6` all match table forms; `chain` pending its expected block.
=> the full silicon-verified rule set (4B-LDI pins, latency fills, store-unit
classes, BRU.JMP) is assembler-reproducible end-to-end.

### NOTE: gate authority transfer (AsmSchedImpl)

The hand-written emit lists above are SUPERSEDED by shave-isa/gates.py
(AsmSchedImpl's G1-G8 suite) against the current encodings.json.  Retired
stale numbers: `82044308` (old AND cross-check; current
`IAU.AND i4,i1,i2 -> 86301001`, decode-back exact), `80850021ac` (old LD.8
displacement; current `LSU0.LD.8 i2,[i0+4] -> c22a002106` = fmt4 5B, decode
`LD.8 i2, i0, i4`), `42999803`/fmt12 ST.32 (current hintless `82490c08` =
fmt5, decode-equal), `820e00` 3B LDIL (demoted; 4B `82820e00`).  Still-valid
anchors verified on the current DB (hintless): CP.32 i18,i29 -> 81dc2508
(fmt15 pin), CP.32 i6,i9 -> 819c0c08, LDIL i2,0xABCD -> 8212cdab,
ST/STI vendor map 42640a00/49741008, SAU 3ce09c30, LDO c2a2641900 /
0182645100 (fmt110) / c2524a0100, AND i1,i8,i1 -> 86108500, SUB c6614e94,
DIV 3c062503, LD.32.F16 42701200, BRA b5040002, JMP 47f101.

---

# Issued-stream legality gates (AsmSchedImpl; D1-D4 in shave_asm.py)

Model, switches and rule table: PACKETS.md.  Everything below is a verbatim run
in this directory (2026-09-28).

## G1-G8 suite — 128/128

```
$ python3 gates.py
gates.py — shave-isa regression + legality (gold=add1-fp16.elf, mlibm=13 objs)
G1 gold roundtrip (192/192)
G2 mlibm roundtrips (13 objects)
G3 encode anchors (39 forms)
G4 branch delay slots
G5 load latency (>=6 slots in the emitted bytes)
G6 LSU unit policy
G7 pipe shape (<=3 slots, <=1 op per unit, <=128 bits)
G8 verbatim mode + CLI equivalence

128/128 checks passed
ALL GATES GREEN
```

G3 now pins 39 forms with the evidence for each (HW anchors, gold/ef_j0 raw
bytes, `; fmt=`-pinned alternatives) and additionally requires the emitted
bytes to DECODE BACK to the same mnemonic (form-selection symmetry).

## Gold roundtrip gate command unchanged (192/192)

```
$ python3 shave_asm.py disasm --elf .../npu3720/add1-fp16.elf 2>/dev/null > /tmp/g.s
$ python3 shave_asm.py asm /tmp/g.s --hex > /tmp/g.out
legalize: verbatim (disasm listing with raw= hints): re-encoded byte-for-byte, no padding/rewrites
$ # byte compare against the ELF .text:
identical: True 192 192
```

Rationale: a `disasm` listing carries `raw=` byte hints, i.e. it *is* a
vendor-scheduled legal stream — padding or unit-rewriting it would corrupt the
byte-identity roundtrip.  `legalize_text()` therefore short-circuits on any
input containing `raw=` (and `--exact`/`--no-legalize` do the same for any
file).  Compiler output never carries `raw=` (checked: 0 occurrences in
`/tmp/mm/*/ours.s`), so the legality pass is fully active on the real path.

## D1/D2/D3: naive source -> legal stream, byte level

```
$ cat C_autofill.s
	LSU0.LDO.32 i0, i18, 0x28
	LSU0.ST.32 i0, i0
	BRU.JMP i30
$ python3 shave_asm.py asm C_autofill.s --hex
legalize: 6 delay-slot NOP(s); 6 load-latency NOP(s); 1 LSU unit rewrite(s) [LSU0.ST.32->LSU1.ST.32]
c2026451000707070707078909000147f101070707070707

$ python3 shave_asm.py asm /tmp/k1.s --hex --exact      # hand-written recipe, no pass
c2026451000707070707078909000147f101
$ python3 shave_asm.py asm /tmp/k1.s --hex               # ... with the pass
legalize: 6 delay-slot NOP(s)
c2026451000707070707078909000147f101070707070707
```

The auto-generated stream for the naive source is **byte-identical** to the
hand-written working recipe k1 (`LSU0.LDO.32` + 6 fillers + `LSU1.ST.32`) plus
the 6 delay-slot NOPs after the `BRU.JMP` that k1 lacked.  k3b (3 loads, f16
conversion, `LSU1.STI.16.F32.F16`) is likewise byte-identical apart from the
same trailing fill.

## Hardware (MTL NPU, runp3/runp4 + dump_probe; repeated 2x where marked)

| probe | source | ACTUAL (window[0:4] / window[16:20]) |
|---|---|---|
| A `k1` control (hand-written) | `LDO+6x0x07+LSU1.ST.32+JMP` | `00 40 01 2e` / `10 40 01 2e` |
| C `C_autofill` (auto) | `LDO / LSU0.ST.32 / JMP` | `00 40 01 2e` / `10 40 01 2e` (2x: schedC, C_def2) |
| C `--exact` (no pass) | same naive source | **no ACTUAL — probe faults** (2x) |
| B `k3b` control | f16 chain, `LSU1.STI.16.F32.F16 ;; fmt=6` | `80 c4 00 00` / `11 40 00 00` |
| B `--exact` | same | identical to B |
| D `LSU1.ST.16.F32.F16 i3,i0` (the D3 rewrite target, bytes `49340000`) | f16 chain, plain ST on LSU1 | `80 c4 00 00` / `11 40 00 00` — identical to k3b (2x) |
| E `LSU1.ST.16.F32.F16 i3,i0 ;; fmt=0` (old hintless pick, bytes `20390000`) | f16 chain | **no ACTUAL — probe faults** (2x) |

Consequences:
* D1+D2+D3 are sufficient to turn an unscheduled naive kernel into a stream
  that delivers the loaded value on the real device (C == k1 byte-for-byte and
  in ACTUAL, while `--exact` faults): failing-before / passing-after evidence
  for the assembler pass itself.
* The D3 rewrite target for the f16 store family is hardware-validated (D),
  and the previously-selected hintless form is not (E) — therefore
  `cal_fix.py` now pins `LSU1.ST.16.F32.F16` fmt=1 (demotes fmt0) and
  `LSU1.ST.32` fmt=18 (`89 09 00 01`, the u5/k1-verified store).  The rewrite
  path pins the chosen fmt on the rewritten line, so the compiler path is safe
  even without the DB pin; the pin closes the hole for a backend that writes an
  LSU1 store itself.

## What remains NOT automatic (by design)

* Effective combination on the current backend stream: `SHAVEFillDelaySlots`
  emits `1b 40` (fwd) / `2b 40` (bwd) => **k = 1** real instruction executes in
  the 6-slot window of a taken conditional branch; the assembler backstop does
  not fire on those streams (verified: 0 descriptor warnings on
  `/tmp/mm/FINAL/ours.s`) and would use `1b 50`/`2b 50` = k = 0 (immediate) if
  a descriptor were ever missing.
* `1b 40` after a conditional `BRU.BRA` — emitted by the backend
  (SHAVEFillDelaySlots, agent MedicalCrayfish).  The assembler **warns**
  ("without it the branch will NOT transfer control") and can emit it with
  `--bra-descriptor`; it never fills delay slots after `BRU.BRA` (with the
  descriptor the transfer is immediate, measured S00..S08).
* LDIL/LDIH-pair -> LSU spacing (>=6 slots) — backend-owned; the assembler
  warns and `--ldi-pair-pad` pads.
* Compressed NOPs / SLOT_ALL promotion — not implemented (plain `0x07` NOPs
  only; the compressed byte encoding is unrecovered).
* Flag liveness (`CMU.CMx` -> `BRU.BRA <flag>` must not be separated by a write
  to that register) — backend responsibility (FINDINGS R4); the assembler has
  no representation for flag names.

## Round 2 (VendorRE2 compressed-NOP + descriptor measurements, Main directive)

Defaults changed (all measured; escape hatches keep the old behaviour):

| default | what | off switch |
|---|---|---|
| compressed filler | gaps >= 2 slots emit ONE byte `(n-1)<<4` (6 slots -> `0x50`, 12 -> `0xc0`); 1-slot gaps stay `0x07` (the n=1 compressed form `0x00` was only ever observed in failing <6 gaps, so it is never emitted) | `--no-compressed-nops` |
| branch descriptor | a conditional `BRU.BRA` with no descriptor in the source gets `1b 50` (forward) / `2b 50` (backward) = **immediate** transfer, k=0 real delay instructions; never doubled when the backend already emitted `.byte 0x1b, 0x40` | `--no-bra-descriptor` |
| `; desc=<hex>` hint | supplies a vendor descriptor variant per branch (passthrough, e.g. `; desc=eb30bce0`) | — |
| filler accounting | `.byte 0x10..0xf0` counts `(b>>4)+1` slots (VendorRE2: 0x50=6, 0x20+3x0x07=6, 0x40=5) so idempotency holds for vendor-compressed streams | — |
| `--div-latency 13` | `SAU.DIV*` result visible only >=13 slots later (k<=10 stale, 11/12 fault, >=13 correct) | `--div-latency 0` |
| IAU.ADD-imm warning | the silicon uses only `imm & 3` as the effective addend (C17: 1->1, 2->2, 4->0) | — |

NOTE: the C14 "orphan-HI LDIH" restriction was RETRACTED (Mockingbird E5/S3/S5:
bit15-set LDIH immediates execute normally; the original trap was a skeleton
artifact), so the assembler imposes no LDIH immediate restriction and G5
asserts the absence of such a warning.  C17 (`IAU.ADD` imm&3) stays until
VendorRE2's discriminator sweep reports.

Gate suite after the change: **145/145 green** (`python3 gates.py`), gold
roundtrip still 192/192 byte-identical (the listing is verbatim, so the new
defaults cannot touch it), 40 encode anchors incl. `IAU.ADD i9,i9,2 ->
c6202581` (gold .text+0xb3).

Hardware re-verification with the compressed defaults (runp4 + dump_probe,
MTL NPU):

| probe | stream | ACTUAL |
|---|---|---|
| `C_autofill.s` naive source, compressed defaults | `c202645100 50 89090001 47f101 50` | `00 40 01 2e` / `10 40 01 2e` (same as the uncompressed stream) |
| `B_k3b.s` f16 chain, compressed defaults | unchanged except the trailing `50` | `80 c4 00 00` / `11 40 00 00` (same as baseline k3b) |

So the two known-good recipes pass with the compressed filler form, and the
assembler-produced stream for a naive source is 2 bytes longer than the code
itself (one `0x50` for the load gap, one for the return branch).

### Pipe/slot formation experiments (Main's hypothesis)

* Grouping is byte-invisible and behaviour-neutral: `F_pipe_flat.s` (one op
  per line) and `F_pipe_grouped.s` (same instruction sequence in 3-slot
  `a || b || c` pipes) assemble to the SAME bytes
  (`c20264510007c620a50441f00107c600210441e0018909000147f101` + fills) and
  give the same ACTUAL (`00 40 01 2e` / `10 40 01 2e`).
* Position/mix shifts do not flip the k1-class recipe: store at byte 12 with
  7 filler slots (`G2_shift1`) and store at byte 12 with a different filler
  mix (`G3_mix`: 5x `BRU.NOP` + 4-byte `IAU.ADD`) both pass with the correct
  value.
* Conclusion: text-level pipe formation cannot change the byte stream, so it
  cannot be the residual kernel blocker; what remains is *unmodeled opcodes*
  (gold's unresolved runs 0x0b-0x49, 0x79-0x7c, 0x7e-0x7f, 0x8a-0x8c per
  VendorRE2) and per-form silicon quirks (imm&3, LSU class bytes).

## CRITICAL FIX: obj writer dropped bit 31 of the AU-immediate form (round 3)

Found by VendorRE2 (PACKET_SPEC §13, VERIFIED_EXAMPLES K.4/L): `shave_asm.py obj`
emitted different bytes than `asm --hex` for `IAU.ADD d, s, imm` - the
`0x80000000` fixed bit was gone because the OBJ path called
`resolve_form(db, mn, nops, fmt_hint)` **without the operand-kind filter** and
without the `[iN+off]` -> implicit-immediate expansion, so an immediate operand
selected the reg-only form and landed in a register field:

```
asm --hex : IAU.ADD i9,i9,2 -> c6202581   (== gold .text+0xb3)
obj       :                    c6202501   (decodes as IAU.ADD i9,i9,i2)
asm        : IAU.ADD i1,i1,1 -> c6208480   obj: c6208400
asm        : IAU.ADD i1,i1,4 -> c6200482   obj: c6200402
```

Silicon (VendorRE2 control, correct bytes, base 0x12340000): imm 0 -> +0,
imm 2 -> +2, imm 8 -> +8 - i.e. the field map and the immediate semantics are
fine and **no "imm & 3" restriction exists**; the earlier C17 note was this
bug's shadow and the corresponding warning was removed.

Fix: `assemble_object()` now mirrors `assemble()` - operand-kind filter plus
the `[iN+off]` expansion.  New gate **G9 (obj/asm byte agreement)**:
* explicit obj bytes for the AU-immediate anchors (`c6202581`, `c6208480`,
  `c6200482`, `c6604e94`, `c6614e94`),
* obj == asm for 13 representative forms (loads/stores/LDI/CMU/SAU/BRU/pipe),
* obj == asm for **every mnemonic line of the current compiler output**
  (`/tmp/mm/FINAL/ours.s`: 262 lines, 0 mismatches).

Gate suite: **168/168 green**; gold roundtrip 192/192 (unchanged command).

Consequence: every previously built object that materialised an AU-immediate
(a pointer/counter bump, loop arithmetic) executed a *different instruction*
on the NPU - re-assemble and re-run the oracle after pulling this fix.

## Offset-0 32-bit loads (round 4, Main's pinned defect) - MEASURED, not reproduced

Minimal effect-read kernel (out-pointer from `params+0x28`, loaded value stored
through a proven `LSU1.ST.32`), `--exact`, runp4 + dump_probe:

| form | bytes | ACTUAL[0:4] inv1 / inv2 | verdict |
|---|---|---|---|
| `LSU0.LDO.32 i5, i18, 0x0` | `c252640100` | `40 43 01 2e` / `50 43 01 2e` | loads the in-pointer (0x2E014340/50) - OK |
| `LSU0.LDO.32 i3, i18, 0x0` | `c232640100` | `40 43 01 2e` / `50 43 01 2e` | OK (the fp-loop preheader shape) |
| `LSU0.LDO.32 i2, i18, 0x0` | `c222640100` | `02 00 00 00` | value 2, NOT the in-pointer |
| `LSU0.LD.32 i5, i18` | `825d2401` | `40 43 01 2e` / `50 43 01 2e` | OK (2-operand, no offset field) |
| `LSU0.LDO.32 i2, i2, 0x0` (i2 = dims ptr) | `c222440100` | `08 00 00 00` | loads dims[0] = 8 - OK (kernel counter shape) |
| `LSU0.LD.32 i2, i2` (rewritten) | `822d0401` | `08 00 00 00` | identical to the line above |
| `LSU0.LDXV i3, i18` | `42312400` | `03 00 00 00` | destination NOT written (stale i3) |
| `LSU0.LDXV i5, i18` | `42512400` | `02 00 00 00` | destination NOT written (stale i5) |

So: (a) the offset-0 LDO form is **not** universally fatal - it works for
dst=i3/i5 and for the kernel's `dst=base=i2` counter load (dims[0] = 8 read
correctly); (b) the vendor's `LDXV`-style indexed form as our tables encode it
does **not** write its destination, so it is NOT a candidate; (c) the 2-operand
`LSUx.LD.W d, base` is silicon-verified and equivalent.  The assembler now
rewrites an explicit offset 0 to that form (`D5`, `--no-zero-offset` to keep
the LDO form), which on the current compiler output converts exactly the two
kernel loads:
`LSU0.LDO.32 i2, i2, 0x0 -> LSU0.LD.32 i2, i2` and
`LSU0.LDO.32 i3, i18, 0x0 -> LSU0.LD.32 i3, i18`.

Byte-level discrepancy worth chasing on the backend side: the reported faulting
bytes (`c2 52 65 01 00`, `82 5d 25 01`) differ from the measured-working ones
(`c2 52 64 01 00`, `82 5d 24 01`) in **bit 0 of byte 2** - i.e. the caller's
streams carry a different variant of the same form; only the bit-0-clear
variants are silicon-verified here.

### High load destinations: PATTERN legality, not a fixed register list (VendorRE2 §14, rewritten)

Direct NPU sweep (VendorRE2, hand-written bytes, 2x per fault row): in the
5-byte `LSU0.LDO.32` load form the destination set **{i19, i21, i24, i30}
FAULTS** while i16, i17, i18, i20, i22, i23, i25..i29, i31 execute - and i25
was verified to load the in-pointer correctly, so the earlier "r16..r31 alias
onto r0..r15" story is REFUTED (bit 16 of the destination field is honoured);
this is a register-dependent trap.  The 2-operand `LSU0.LD.32` form is
verified-good for i5 but unsafe for high registers (`LD.32 i21` traps;
`LD.32 i25` executes but writes nothing).

VendorRE2's follow-up sweep (LDIL/LDIH pairs, 0xBEEF/0xDEAD vs 0x0001/0x0000):
the fault set is NOT a fixed register list - r21 is legal with imm
0x0001/0x0000 and traps with 0xBEEF/0xDEAD, while r19/r30 trap in both; i.e.
legality is a *pattern* property of the whole encoded word (register field +
immediate bits), the same phenomenon as the LDIH immediate sweep.  The
mechanical rule the assembler can enforce is the conservative one: destinations
in r0..r15 and only effect-verified immediates.

Assembler consequences (both landed):
* **D5** rewrites `LSUx.LDO.W d, base, 0x0` to `LSUx.LD.W d, base` **only for
  destinations <= i15** (the verified range); high destinations keep the
  5-byte LDO form.
* **D6** warns when a memory-load destination is > i15, naming the imm=0 fault
  set and stating that trap legality is a (register, immediate) pattern
  property (VendorRE2 §14); LDI forms and store value fields are not flagged
  (stores unmeasured).
This is a hard constraint for the register allocator: keep load destinations
in r0..r15.

### DIV consumption in the live backend stream (round 5 finding)

The current `/tmp/mm/FINAL/ours.s` emits each `SAU.DIV.U32` with only three
`BRU.NOP` lines before its `CMU.CMZ.I32 <result>` consumer (~4 slots), i.e.
inside the stale/fault zone MedicalCrayfish measured (k<=10 stale, 11-12 fault,
>=13 OK).  The assembler's `--div-latency 13` default tops all five sites up to
13 slots (`legalize: 13 latency slot(s) filled`), so the *assembled* stream is
legal; the finding is reported to the backend because this is precisely the
dims-loop guard arithmetic (`DIV -> CMU.CMZ flag -> BRU.BRA`) that decides
`count`.

### `; fmt=N` hints are never dropped silently (G10)

AsmEncodingRE's round-2 note: `; fmt=N` was silently dropped when the hinted
form's operand count did not match the `[iN+off]` expansion (e.g. `; fmt=8`
with a displacement resolves to the 5-byte c2 class without the hint), which
mis-attributed probe bytes.  Both `asm` and `obj` now print
`warning: line N: MN: fmt=K hint not honoured for this operand shape (emitted
fmt=M)` to stderr while still emitting the bytes; the gold/mlibm roundtrip path
produces ZERO such warnings (verified), so the diagnostic never fires spuriously
on supported listings.  Gate G10 pins both the warning and its absence.

### Byte loads (`LSUx.LD.8`): the 0x42-class 2-operand form is silicon-dead

AsmEncodingRE C21 + StrangeSole's nonzero-witness redo: `LSU0.LD.8 i2,i1` =
`425d040e` inverts to the correct operands but delivers NOTHING (both byte-3
variants, 0x0e and 0x00; trailing stores still execute, so it is not a
half-byte leak), and the vendor corpus contains no LD.8 instance at all.  The
assembler therefore WARNS on the plain 2-operand shape ("measured non-working
... use the displacement form or widen and mask") and does not touch its bytes
(they are correct-but-non-working, indistinguishable at byte 3).  The
displacement form `LSUx.LD.8 d, [base+off]` (`c22a422006` for `i2,[i1+1]`) is
the candidate replacement and is NOT flagged; its silicon verdict is pending
(StrangeSole).  If it works, the natural follow-up is a D5-style rewrite from
the dead 2-op shape to the displacement form with offset 1 (offset 0 is the
zero-offset hazard shape).

### SAU.ADD.F32 result latency (ResultingBear, dump_probe)

The consumer of a `SAU.ADD.F32` result must issue >= 2 slots later (gap 0/1
reads the stale pre-add value, >= 2 is correct); the backend pads it
(fixFAddLatency) and the assembler now guarantees it too (`--fadd-latency`,
default 2).  Verified: `SAU.ADD.F32 i7,i7,i1` + `LSU1.ST.32 i7,i0` ->
`3ce09c30` + 2 NOPs + `89790001`; an already-2-slot stream is untouched.
Also closed by the same measurement: the SAU.ADD.F32 third operand is a
hardware **constant selector**, not a GPR (i1 -> +1.0 even when i1 holds 5.0f;
i4 -> +1.41494, i5 -> +0.70604, i6 -> 3.14159 ...), so the G3 anchor
`SAU.ADD.F32 i7,i7,i1 -> 3ce09c30` is correct and no "materialise b" rule is
needed.

### LSU op in a load's write-back window (StrangeSole, loopdiff/DIFF.md)

Measured: an LSU-licensed op issued **4-5 slots after `LSU0.LDO.32`** (and 0-6
after `LSU0.LD.32.F16.F32`) silently discards that load's write-back - the
destination keeps its previous value; non-LSU fillers never do (this is the
mechanism behind the kernel's dim load returning the address).  Implemented as **D3b**, refined to the follow-up sweep:
* `LSU0.LDO.32` (and plain loads): **same-unit only** - an LSU1 op in the same
  slots is measured safe; an op landing at gap 4-5 is pushed to gap 6 with
  fillers (`BRU`-unit NOPs, so the filler itself cannot trigger the effect);
  gap 0 (back-to-back loads) is left alone (measured safe).
  Verified: `LDO.32 + 4xNOP + LSU0.LDIL` -> 6 fillers; `+ LSU1.LDIL` -> untouched.
* `LSU0.LD.32.F16.F32`: wider and **cross-unit** (an `LSU1.LDIH` at slot 1 and an
  `LSU0.LDIL` at slot 6 both lost it in the unseeded probes) -> >= 8 slots are
  kept clear.  CAVEAT: StrangeSole's seeded re-sweep (seed 0xAABB3344 vs the
  real 0xC0900000, gaps 3..9, same-unit and cross-unit, 14/14) KEEPS the value
  everywhere, i.e. this family's losses are layout/packet dependent rather than
  a clean distance boundary.  The wider window is therefore a deliberately
  conservative choice (cost: a few compressed filler bytes), not a measured
  boundary; narrowing it to the plain window is a one-line change if byte count
  ever matters more.
  Verified: `LD.32.F16.F32 + 5xNOP + LSU0.LDIL` -> 7 fillers.
StrangeSole independently reports that with D3b + the backend's sense fix the
fp-loop preheader now reads count = 8 and one fp iteration stores a real value,
and re-ran their whole probe set (104 `.s` files) against this assembler with
every DIFF.md number reproduced.

OPERATIONAL CAVEAT (StrangeSole): fillers change byte offsets, and several
measured cases show identical slot distances behaving differently with
different surrounding bytes - so if a stream misbehaves right after this pass
added slots, the first thing to suspect is the layout, not the encoder.  A/B
with `--no-legalize` (or `--exact`) vs the defaults, or `--lsu-policy off` for
just the unit/rewrite rules, via runp3.sh + dump_probe.

Gates after the change: **181/181 green** (G6 gained three zero-offset checks,
G9 keeps obj==asm), gold roundtrip 192/192.

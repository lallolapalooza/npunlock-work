# VERIFIED_EXAMPLES — reproduced packet / LSU-delivery encodings

All examples are reproduced end-to-end with
`OUT=/tmp/pk/run bash llvm-14/tmpdump/runp3.sh /tmp/pk/<name>.s` (assemble with
`shave-isa/shave_asm.py obj` → `ld.lld` → `npunlock-patchblob` → `porting_probe` →
`dump_probe`).  The observable is the 32-byte ACTUAL oracle window.  Probe sources are in
`/tmp/pk/`; every verdict below was reproduced at least twice except where noted.

Baseline window with a trivially-wrong kernel:
`00 01 00 00 ff ff ff 00 00 00 80 3f 00 00 00 bf 20 01 00 00 ff ff ff 00 00 00 80 3f 00 e0 dd be`.
Success patterns below are `xx xx xx xx ff ff ff 00 00 00 80 3f 00 00 00 bf …` (only the
first 4 bytes are written by the probe; bytes 16-19 are the carrier's second slot).

Common prologue in every probe:

```
LSU0.LDO.32 i1, i18, 0x28      ; bytes c2 12 64 51 00   (i1 = out pointer, i18 = params)
6 x IAU.ADD i9, i9, i9         ; bytes c6 20 a5 04 (x6)  (harmless fill)
```

Note `LSU0.LDO.32 d, base, imm` = 5 bytes `c2 (d<<4|2) ((base<<1)|0) ((imm<<1)|1) 00`; e.g.
`c2 12 64 51 00` = dst i1, unit 2 (LSU0), base = (0x64>>1)&0x1f = 18, imm = (0x5100>>1)... =
0x28.  (Encoding produced by `shave_asm.py`; matches gold bytes at 0x00/0x05/0x8d.)

---

## Example A — LSU write-back latency is exactly 6 instruction slots  [HW]

```
LSU0.LDO.32 i0, i18, 0x28       ; i0 <- *(params+0x28)  (out pointer)
<N x IAU.ADD i9, i9, i9>        ; 4-byte fill (or N x BRU.NOP = 07, or CMU.CMZ.I32 = 3B)
LSU1.ST.32  i0, i0              ; store i0 at address i0  -> window shows the pointer
BRU.JMP i30
```

`n6.s` bytes: `c202645100 c620a504×6 89090001 47f101`
(the store `LSU1.ST.32 i0,i0` = `89 09 00 01`; `BRU.JMP i30` = `47 f1 01`.)

| probe | fills | ACTUAL (first 4 bytes) | verdict |
|---|---|---|---|
| n0  | 0 × IAU.ADD | no ACTUAL (execution failed) | stale pointer used as address → trap |
| n1  | 1 | no ACTUAL | idem |
| n2  | 2 | no ACTUAL | idem |
| n3  | 3 | `00 01 00 00` (baseline) | store happened, pointer stale |
| n4  | 4 | `00 01 00 00` | stale |
| n5  | 5 × IAU.ADD | no ACTUAL (failed, reproducible) | stale value used as address |
| **n6** | **6** | **`00 40 01 2e` = 0x2E014000** | **load delivered** |
| n7/n8 | 7 / 8 | `00 40 01 2e` | delivered |
| s5  | 5 × BRU.NOP (`07`) | `00 01 00 00` | stale |
| **s6** | **6 × BRU.NOP** | **`00 40 01 2e`** | **delivered** |
| s12 | 12 × BRU.NOP | `00 40 01 2e` | delivered |
| c5  | 5 × CMU.CMZ.I32 (3 B) | – | stale |
| **c6** | **6 × CMU.CMZ.I32** | **`00 40 01 2e`** | **delivered** |

Interpretation: the threshold counts **instruction slots, not bytes** (1-byte, 3-byte and
4-byte fills all flip at 6).  Implication for the backend: every LSU producer must be
≥6 slots ahead of its first consumer (schedule, or pad with `BRU.NOP`).

Also verified in the same shape: `LSU0.LDO.32 i0,i18,0x28` + 6 fills + `LSU1.ST.32` with a
*pre-loaded* value register behaves identically, i.e. the rule applies to the address
operand as well as the value operand.

## Example B — one LSU unit per issue group  [HW]

| probe | body | ACTUAL[0:4] | verdict |
|---|---|---|---|
| u5 | `LSU0.LDO.32 i0,i18,0x28` + 6 fills + `LSU1.ST.32 i0,i0` | `00 40 01 2e` | correct |
| u1 | `LSU0.LDO.32 i0,i18,0x28` + 6 fills + `LSU0.ST.32 i0,i0` | `00 5c b4 8f` | **garbage** |
| u2 | same as u1 with 12 fills | `00 0c 00 8f` | **garbage** (distance does not help) |
| n6 | LSU0 load + 6 fills + LSU1 store | `00 40 01 2e` | correct |

`u1.s` bytes: `c202645100 c620a504×6 82 09 00 08 47f101`
(`LSU0.ST.32 i0,i0` = `82 09 00 08`; the LSU1 variant `89 09 00 01`.)  The only difference
between u1 and u5 is the LSU unit (and hence the store opcode class) — the addressing
operands are identical, so the failure is not an operand-encoding artifact.
This is the silicon behaviour behind the vendor diagnostics
`LSU0/LSU1 unit is used more than once in the same pipe` (moviAsm64 strings 2616/2617).

## Example C — the LDI (constant materialisation) family  [HW + TBL]

Hard-coded byte sequences (all 4-byte; `reg<<3|subop` in byte 1):

| # | bytes | expected meaning | ACTUAL[0:4] |
|---|---|---|---|
| r2 | `82 12 cd ab` | `LDIL i2, 0xABCD` (low half) | `cd ab 00 00` = **0x0000ABCD** |
| r3 | `82 a2 cd ab` | `LDIL i20, 0xABCD` | `cd ab 00 00` ✓ |
| f1 | `82 a2 cd ab` (same, store value i20) | ✓ | `cd ab 00 00` |
| **r5** | **`82 12 cd ab` `82 13 34 12`** | **LDIL+LDIH i2** | **`cd ab 34 12` = 0x1234ABCD** |
| v6 | `82 a3 cd ab` | `LDIH i20, 0xABCD` (high half) | `00 00 cd ab` = **0xABCD0000** |
| p2 | `82 a2 cd ab` `a9 a3 34 12` | LDIL + LSU1.LDIH, reg i20 | `cd ab 34 12` = 0x1234ABCD |
| p4 | `82 a2 cd ab` + 6 fills + `a9 a3 34 12` | pair at distance | `cd ab 34 12` ✓ |
| p1 | `a9 a3 34 12` then `82 a2 cd ab` | reversed order | `00 00 34 12` (only high survived) |
| r4 | `82 12 cd ab` `a9 13 34 12` | reg i2 mixed forms | execution failed (reproducible 2/2) |

`r5.s` full bytes:
`c212645100 c620a504×6 8212cdab 82133412 c620a504×6 89290201 47f101`

**Our assembler's current LDIL is not one of these.**  `shave_asm.py asm` of
`LSU0.LDIL i20, 0xABCD` emits the 3-byte `a2 cd ab`; on hardware that triple is not an
LDIL:

| probe | instruction | ACTUAL[0:4] |
|---|---|---|
| v1 | `LSU0.LDIL i20, 0xABCD` (assembler output `a2 cd ab`) | execution failed |
| v4 | `LSU0.LDIL i2, 0xABCD` | `00 00 00 00` (writes nothing / zero) |
| ld0..ld7 sweep | raw `[0xA0..0xA7] cd ab` in the same slot | s∈{1,4,6}: `00 00 80 00`; others: execution failed |

Table provenance [TBL, `shave-isa/defs_*.pkl`]:
`LSU0.LDIL = [('0x',1,16),('IW',0,5),('D',642,11)]` (642 = 0x282),
`LSU0.LDIH = [('0x',1,16),('IW',0,5),('D',898,11)]` (0x382),
`LSU1.LDIH = [('0x',1,16),('IW',0,5),('D',905,11)]` (0x389 / 0x3a9 with the corpus `|0x20`),
`LSU1.LDIL = [('0x',1,16),('IW',0,5),('D',649,11)]` (0x289) — 32 bits each, hence 4 bytes.
Packing first-record-at-top gives byte0 = opcode>>3, byte1 = `(reg<<3)|(opcode&7)` — exactly
the `82/89/a9` prefixes observed.

## Example D — complete load → ALU → store datapath in *our* encoding  [HW]

```
LSU0.LDO.32 i0, i18, 0x28      ; out pointer
LSU0.LDO.32 i1, i18, 0x0c      ; rank (params+0x0c == 4 in the fixture)
6 x IAU.ADD i9, i9, i9
IAU.ADD i2, i1, i1             ; 2*rank
6 x IAU.ADD i9, i9, i9
LSU1.ST.32 i2, i0              ; store 2*rank at out
BRU.JMP i30
```
bytes: `c202645100 c212641900 c620a504×6 c6208800 c620a504×6 89290001 47f101`
(`LSU1.ST.32 i2, i0` = `89 29 00 01`; `IAU.ADD i2, i1, i1` = `c6 20 88 00`.)
ACTUAL[0:4] = `08 00 00 00` = 8 = 2 × rank(4) ✓.
This is the existence proof that load-result *delivery* works with nothing but our own
encodings, once (a) ≥6 slots of distance are respected and (b) constants are materialised
with the correct LDI forms.

## Example E — gold's pointer-setup pipe at 0x88-0x97

Gold `.text` (file 0xc0.., 192 B) bytes 0x88..0x97:
`1b 50 e9 9d 24 | 01 82 64 51 00 | 30 86 c4 a8 02 | 07` (16 bytes = 128 bits).

* `01 82 64 51 00` = `LSU0.LDO.32 i8, i18, 0x28` (the out-pointer read, verified byte-equal
  by our assembler's `fmt=110` form, and the only one of the three groups our tables can
  decode).  Patching its class byte `01`→`c2`/`00`/`10`/`40` keeps execution but produces
  **wrong values**; `f9`/`fa` fail — i.e. the class byte is semantically load-bearing
  (ByteExactLog iteration 20b, re-derived here: the value path is unchanged, so the class
  selects the slot/queue in which the load retires).
* `1b 50 e9 9d 24` and `30 86 c4 a8 02` match no form in the extracted tables (0 matches
  over 10,941 forms at any offset/length, `shave-isa` scan) and contain the load-bearing
  bytes `30`; the whole 0x88..0x97 run is required (NOP-ing it fails execution, iteration
  20c).
* `07` = `BRU.NOP` (the only 1-byte instruction in any extracted table).
* Sum check: 40 + 40 + 40 + 8 = 128 bits exactly, consistent with "pipe ≤128 bits, ≤3
  operations + filler".

## Example F — negative results that constrain the packet model

1. **The 3-byte "LDI" sequences in gold are not (usable) LDILs.**  Testing gold's verbatim
   bytes as an instruction in our kernel: `40 0e 88 ac` → execution failed (t4); the
   `40 (reg<<3|subop) imm16` and `0a (reg<<3|2) imm16` families write nothing (t1,t2,t3,t5
   → `00 00 00 00`).  Gold's 3-byte-looking runs must therefore be re-segmented before they
   can be named (`PACKET_SPEC.md` §4.1).
2. **There is no standalone 1-byte packet/descriptor instruction.**  Exhaustive scan of all
   10,941 extracted forms: only `0x07` (`BRU.NOP`/`BRU.INC`) has `length_bytes == 1`; no
   form of any length has an opcode byte equal to `0x40`, `0x03`, `0xc0`, `0xeb`, `0x10`,
   `0x30`, `0x1b`, `0x2c`, `0x24`, `0x2b`, `0x9c`, `0xea`, `0x50` (checked in both packing
   orders).  Consequence: the gold "marker" bytes are payload bytes of longer instructions,
   not NOP/descriptor opcodes.
3. **Gold is byte-aligned, not bit-packed.**  Bit-offset scan of all 192 gold bytes (all 8
   sub-byte offsets × 2/3/4/5-byte forms): matches at sub-offsets 1..7 are all
   low-information degenerates (`LSU0.LDIL` mask-3 forms, `SAU.RQT`…); only offset 0
   decodes the real stream (70/192 instructions).  So variable-length instructions are
   padded to byte multiples, and no 4-bit remainder exists in this binary.
4. **`LDIL i5, 0x0FFF` never delivering** (ByteExactLog iteration 20) is fully explained by
   Example C: the 3-byte LDIL form our assembler emitted is not executed as an LDIL, so no
   distance can help.  Same root cause as "the guards compare against garbage" → early exit
   → zero writes.

---

## Appendix — probe corpus in `probes/`

Sources of every measurement above (81 `.s` files), to be run one at a time, >=1 s apart:

```
OUT=/tmp/pk/run bash /home/asdf/projects/llvm-14/tmpdump/runp3.sh \
    /home/asdf/projects/npunlock-work/packets/probes/<name>.s
```

`runp3.sh` assembles (`shave-isa/shave_asm.py obj`), links (`ld.lld`), patches the carrier
blob, builds the bundle and prints `INPUT/EXPECT/ACTUAL` from `dump_probe`; a missing
`ACTUAL` means `execution: failed` (device result 0x70000001) — run
`dump_probe --bundle <out>/bundle` for the JSON.

| probe group | claim |
|---|---|
| n0..n8, s1..s12, c5/c6 | load→consumer distance threshold = 6 slots (Example A) |
| u1/u2/u5, sa6 | same-unit vs cross-unit LSU rule (Example B) |
| r2/r3/f1, r5, v6, p1..p6, r4 | LDI family forms (Example C) |
| chain, chain0 | full load→ALU→store datapath (Example D) |
| v1/v4, ld0..ld7 | our assembler's 3-byte LDIL emit is invalid (Example C) |
| t1..t6, g1..g6, f2..f6 | negative results for the candidate "gold marker byte" families (Example F) |
| lu0..lu8, al0/al3/al6 | LDIL delivery / ALU consumer probes (superseded by r*/chain but retained) |

## Example G — predicted encoding for the sequence our backend needs

Requested shape: *out-pointer load + 32-bit constant (lo/hi) + indexed store*.  Assembler
output for the naive (unsafe) form, and the corrected form:

```
; as shave_asm.py emits it today  (LDIL = invalid 3-byte form, no spacing)
c2 82 64 51 00   LSU0.LDO.32 i8, i18, 0x28
3a 00 00         LSU0.LDIL  i7, 0x0000        <-- BROKEN (missing 0x82 prefix byte)
a9 3b 80 3f      LSU1.LDIH  i7, 0x3F80        (correct 4-byte form)
49 74 10 08      LSU1.STI.16.F32.F16 i7, i8   (== gold .text+0xaf, byte-exact)
```
Corrected, hardware-safe encoding:

```
LSU0.LDO.32 i8, i18, 0x28      c2 82 64 51 00        ; 5 B
LSU0.LDIL  i7, lo             82 3a lo_lo lo_hi      ; 4 B  (0x82 | (reg<<3|2))
LSU1.LDIH  i7, hi             a9 3b hi_lo hi_hi      ; 4 B  (0xa9 | (reg<<3|3))
6 x BRU.NOP                   07 07 07 07 07 07      ; 6 slots of separation
LSU1.STI.16.F32.F16 i7, i8    49 74 10 08            ; 4 B
```
Requirements (all measured): the two *writers* `LDIL`/`LDIH` may be adjacent (r5/p2), but
the `STI` (an LSU consumer) must be ≥6 instruction slots after **each** of
`LSU0.LDIL`/`LSU1.LDIH` and after the `LDO` that produced `i8`; and it must be on the other
LSU unit than the load (here LSU1 store vs LSU0 load ✓).  With those constraints the same
addressing/constant/store shape lands the expected value in the oracle window (Examples A,
C, D).

## Example H — hardware A/B verdicts on the candidate re-segmentations of gold

Placement used for every row (identical to the working probes, so the "slot context" is
proven-good by the control):

```
LSU0.LDO.32 i20, i18, 0x28   ; c2 94 64 51 00   (address register for observation)
6 x IAU.ADD i9, i9, i9
<candidate bytes>            ; 0, 1 or 2 candidate instructions
6 x IAU.ADD i9, i9, i9
LSU1.ST.32  <valreg>, i20    ; store the register the candidate is supposed to write
BRU.JMP i30
```

| probe | candidate | expected if the reading is right | ACTUAL[0:4] | verdict |
|---|---|---|---|---|
| CTL2 (control) | `82 0a 08 32` + `82 0b ac 88` | i1 = 0xac883208 | `08 32 ac 88` | **control PASS** (placement + LO/HI semantics valid) |
| CTL | `82 0a 08 32` + `82 0f ac 88` | i1 = 0xac883208 | `08 32 00 00` | subop 7 is not the HI class (LO only) |
| C2 | `03 aa 4a 01` (op 0x03, reg21, subop2=LO, imm 0x014a) | i21 = 0x0000014a | `00 00 00 00` | **REFUTED** — writes nothing |
| C1a | `40 0e 88 ac` (op 0x40, reg1, subop6) | i1 = 0x0000ac88 (or HI half) | no ACTUAL (execution failed) | **REFUTED** |
| C1p | `82 0a 08 32` + `40 0e 88 ac` | i1 = 0xac883208 | no ACTUAL | **REFUTED** (even as a paired HI) |
| C3p | `82 ea 08 32` + `01 eb 50 ea` (reg29) | i29 = 0xea503208 | no ACTUAL | **REFUTED** (even as a paired HI) |

Consequences:
* The 4-byte *LDI-class* reading of gold's `40 0e 88 ac`, `03 aa 4a 01`, `01 eb 50 ea`
  (AsmEncodingRE C1/C2/C3) is dead on silicon.  (Caveat: a candidate that needs a specific
  *slot/pipe position*, i.e. a different opcode variant, would also fail in this placement;
  CTL2 shows the placement itself is valid for the 0x82-class forms.)
* Gold's load-bearing bytes therefore remain unexplained by "opcode byte + (reg<<3|subop)
  + imm16" shapes with opcode bytes in {0x40, 0x03, 0x01}.  The remaining hypotheses are
  (i) the bytes are payload of instructions whose *opcode byte precedes* the byte we take as
  an instruction start (our boundaries are wrong by one byte or by a bit) or (ii) the
  CV3600xx opcode constants for those families differ from every form in our extracted
  tables; distinguishing them needs either a bit-level segmentation or the CV3600xx table
  re-extract (PACKET_SPEC §4.2).

## Example I — hardware smoke of the CV3600xx opcode candidates (AsmEncodingRE C12 table)

Placement as in Example H; observable = oracle window + execution pass/fail.
"harmless" below means *no trap and the address register `i20` still held the out pointer*
(so the candidate neither faulted nor clobbered `i20`); it does **not** prove the candidate
is a no-op — a clobber of a filler register would look identical.

| probe | candidate bytes | AsmEncodingRE's reading | result | verdict |
|---|---|---|---|---|
| P2 | `2b 40 c6 43` (gold run @0x6a verbatim) | generic 4B, op 0x2b | harmless (`00 40 01 2e`) | bytes form a non-faulting instruction in this slot; not the SWZMV template below |
| P1 | `2b f4 00 00` | `LSU0.SWZMV.128.64` 4B | execution failed | REFUTED as written |
| P3 | `40 14 04` | `CMU.FLUSHB i1` 3B | execution failed | REFUTED as written |
| P4 | `40 18 14 00` | `LSU0.ST.16.F32.F16 i1, i20` 4B | no store (window baseline) | REFUTED as written (0x40 is not that store form) |
| P5 | `00 10 21 03` (gold run @0x36 verbatim) | generic 4B, op 0x00 | execution failed | bytes are not a benign control op in this slot |
| P6 | `00 20 10 3a` | `LSU0.CFLUSH_ALL` template | harmless | plausible control op (or benign decode) |
| P7 | `00 28 10 3a` | `LSU0.CLOCK_ON` template | execution failed | REFUTED as written |

Net: none of the C12 candidates tested explains gold's load-bearing bytes; the only
non-faulting ones (`2b 40 c6 43`, `00 20 10 3a`) are indistinguishable from a benign
mis-decode with this observable.  Getting a positive identification needs either a
bit-level (sub-byte start) segmentation or a `-list`-style vendor decode we cannot run
without wine.

## Example J — bit-level candidate batch (AsmEncodingRE C13): verdicts + a hard constraint

**Placement** (identical to Examples H/I): `LSU0.LDO.32 i20,i18,0x28` + 6 fills + `<candidate bytes>` +
6 fills + `LSU1.ST.32 i20,i20` (+JMP).  "harmless" = no trap and `i20` still held the out pointer.

### (a) Grid verdicts (bit-sliced windows placed byte-aligned in the slot)

| candidate | source | our decoder says | silicon |
|---|---|---|---|
| BB1 `49802059` (bit 1122) | 0x8c+2b | `LSU1.LD.128.U16.U32 i8, i16` | harmless |
| BB2 `c139244849` (bit 975) | 0x79+7b | `CMU.CEX.U16 i20,i9,i1,i16` | harmless |
| BB3 `82734890` (bit 974) | 0x79+6b | `LSU0.LDIH i14, 0x9048` | harmless **but see (b)** |
| G1 k = 0,1,2,3,4,5,6,7 (bits 1344+k, len 32) | 0xa8 run | `eb073ce0`/`f5031e70`/`fa010f38`/`fd80079c`/`7ec003ce`/`3fe001e7`/`1ff08073`/`0f78c039` | FAIL, FAIL, harmless, FAIL, harmless, harmless, FAIL, harmless |
| G2 k = 0..7 (bits 1392+k, len 32) | 0xae run | `10497410`/`88243a08`/`44121d04`/`22890e02`/`91440781`/`48a28340`/`24d14120`/`92e82010` | FAIL, harmless, harmless, harmless, FAIL, harmless, harmless, FAIL |

The "harmless" rows are not evidence of a real instruction: three of them are simply the
*byte-aligned* encodings of instructions we already know (`fa010f`/`7ec003`/`22890e` are
3-byte LDI-family patterns, `82734890` is the 4-byte LDIH shape) whose destination registers
(`i31`,`i15`,`i4`,`i14`) are not observed by the probe; others are undecoded byte strings.
**Pass/fail alone therefore has almost no discriminating power here** — a positive
identification needs the effect-matching protocol of (b).

### (b) Effect-matching protocol (demonstrated); earlier "immediate control bit" claim retracted

Probe `Y2`: candidate `82 73 12 34`, then (i) store `i20` at ptr, (ii) build `i21 = ptr+4`
with the *pinned* 4-byte LDI forms, (iii) store `i14` at `i21`.
Result: window[0:4] = `00 40 01 2e` (pointer, so later code ran ⇒ not a branch) and
window[4:8] = `01 00 12 34`, i.e. `i14` = **0x12340001** — the candidate wrote `0x1234`
into the **high half** of `i14` (whose low bits were stale `…0001`), exactly the
`LSU0.LDIH` semantics.  So the slice at bit 974 is a real LDIH-shaped instruction *and* the
protocol positively identifies both the destination field and the immediate placement.

Immediate sweep on the same shape `82 73 imm16` (probe skeleton as above):

| imm16 | verdict |
|---|---|
| 0x0000, 0x0001, 0x0048, 0x0100, 0x1000, 0x1234, 0x2000, 0x4000, 0x7fff, **0x8000** | executes (high half = imm16) |
| 0x8001, 0x8002, 0x8048, 0x8080, 0x9000, 0x9048, 0xa000, 0xc000, 0xffff | **traps** |

**RECONCILED (rev 4).**  An earlier revision concluded from the sweep above that "imm16 bit 15
set together with any lower bit ⇒ the instruction does not execute", then retracted it when the
same candidates passed in a simpler skeleton.  Both statements are superseded: the trap class is
**pattern-dependent (form × register field × immediate bits)**, independently reproduced in two
different skeletons (see `PACKET_SPEC.md` §5b and §14) — the immediate values below really do
trap *in this skeleton*, and the E/S/T rows below really do execute.  Neither probe therefore
establishes an ISA rule on its own; legality must be effect-checked per (form, register,
immediate) in the stream shape the backend will actually emit.  Data points:

| probe (skeleton) | candidate | result |
|---|---|---|
| E5 (single readback) | `82 73 cd ab` (imm 0xABCD) | i14 = 0xABCD0001 |
| S3 (single readback) | `82 73 48 90` (imm 0x9048) | i14 = 0x90480001 |
| S5 (single readback) | `82 73 00 80` (imm 0x8001) | i14 = 0x80010001 |
| T1 (sweep minus the extra i21 setup) | `82 73 cd ab` | i14 = 0xABCD0001 |
| Y1/Z9048 (full sweep skeleton) | `82 73 48 90` | execution failed |

So the LDIH immediate is 16 bits of data, and the sweep traps were a probe-structure effect
(extra pinned LDI pair + second store) interacting with some high-half values — unresolved
in my harness, **not** an ISA constraint.  Nothing in the LDI family needs bit15 handling.

**Paired materialisation, full 16-bit immediates (as requested, probe E1c/E1d):**

| probe | bytes | result |
|---|---|---|
| E1c | `82 0a 08 32` (LDIL i1,0x3208) + `a9 0b ff ff` (LSU1.LDIH i1,0xFFFF) | i1 = **0xFFFF3208** |
| E1d | `82 0a 08 32` + `a9 0b 00 90` (LSU1.LDIH i1,0x9000) | i1 = **0x90003208** |

⇒ pairing with the **`a9`-class (LSU1.LDIH)** materialises *any* 16-bit immediate correctly,
including the values that failed as orphans in the sweep skeleton.  Compiler rule: emit
`LDIL lo` + `LSU1.LDIH hi` pairs; no split-immediate path is needed.

E3 (load hypothesis for the 0x89-0x8c slice, candidate `49 80 20 59` with i16 seeded to the
valid out-pointer and i8 seeded to 0x1111) → execution failed: the candidate does not behave
as a benign load into i8 with base i16.


---

## Example K — compressed-NOP fillers, branch marker bytes, flag polarity (round 2, [HW])

All probes below live in `probes/q_*.s`, `probes/cn_*.s`, `probes/ca_*.s`, `probes/br/*.s` and
were run through `packets/tools/runp3raw.sh` (= tmpsdump/runp3.sh + `--no-legalize`),
2-3x each, each run re-assembled from source:

```
OUT=/tmp/pkr bash /home/asdf/projects/npunlock-work/packets/tools/runp3raw.sh \
    /home/asdf/projects/npunlock-work/packets/probes/q_50.s
```

### K.1 One compressed-NOP byte == n issue slots  (`q_<hex>.s`)

Skeleton: `LSU0.LDO.32 i0,i18,0x28` ; `<one byte>` ; `LSU1.ST.32 i0,i0` ; `BRU.JMP i30`.
PASS = `ACTUAL[0:4] = 00 40 01 2e` (the loaded out pointer), FAULT = no ACTUAL at all.

| byte | claimed slots (n = (b>>4)+1) | verdict |
|---|---|---|
| `00` | 1 | FAULT |
| `10` | 2 | FAULT |
| `20` | 3 | FAULT |
| `30` | 4 | FAULT |
| `40` | 5 | FAULT (3/3) |
| **`50`** | **6** | **PASS (3/3)** |
| `60` | 7 | PASS |
| `70`..`f0` | 8..16 | PASS |
| `4f`, `5f`, `0f`, `17`, `27` | (low nibble != 0) | FAULT |
| `57` | (low nibble != 0) | store of a **stale** value (executes as something else) |

Full byte copies: `tools/runp3raw.sh probes/q_50.s` assembles to
`c202645100` `50` `89090001` `47f101`.

Additivity: `40 07` (5+1), `20 07 07 07` (3+3), `00 07 07 07 07 07` (1+5), `20 20` (3+3),
`30 30`, `40 40` all PASS (3/3 each); `20 07 07` (5 slots) FAULT 3/3 — one earlier PASS was
an outlier (device flakiness under concurrent agent load). Control: 6x `BRU.NOP` PASS, 5x
`BRU.NOP` FAULT.

### K.2 Conditional branches require a marker byte; the fill declares the delay window

Skeleton (`br/t_*`, `br/s*`): i1 = 0xDEADBEEF, i2 = 0xCAFEBABE (both via pinned LDI pairs),
`CMU.CMII.U32 i0,i0` (flag true), `BRU.BRA i0,TGT`, `<candidate bytes>`, fall-through store
of i1 → `BRU.JMP i30`, `TGT:` store of i2.

| bytes after BRU.BRA | ACTUAL[0:4] | verdict |
|---|---|---|
| (none) | – | **FAULT (2/2)** — the 4-byte `b5 …` branch alone does not execute |
| `40` / `50` / `1b` | – | FAULT |
| `1b 40` / `2b 40` / `1b 50` / `eb 30` | `be ba fe ca` | TAKEN (2/2) |
| `0b` / `2b` / `4b` / `8b` / `cb` / `eb` (alone, store follows) | `be ba fe ca` | TAKEN |
| `3b` / `5b` / `db` / `fb` (alone) | – | FAULT |

Delay-window counter (`br/dc_*.s`: i1 = 1, then eight `IAU.ADD i1,i1,i1`, store at the
branch target; the value read back is `2^k`, k = real delay slots executed):

| marker + fill | ACTUAL[0:4] | k |
|---|---|---|
| `1b 20` | `08 00 00 00` | 3 (2/2) |
| `1b 40` | `02 00 00 00` | 1 (2/2) |
| `1b 50` | `01 00 00 00` | 0 (2/2) |
| `1b 60`, `1b f0` | `01 00 00 00` | 0 (2/2) |

⇒ window = 6 slots, fill = compressed NOP of (6-k) slots. `1b 50` = immediate branch.

Not-taken path (`br/x_false.s`, `br/nf.s`): `CMU.CMZ.I32 i2` (i2 = 2 ⇒ flag false) +
`BRU.BRA i2,TGT` + `1b 50` ⇒ the branch is NOT taken (the fall-through code runs; with the
target replaced by 16 `0xff` bytes the same skeleton faults only when the flag is true,
`br/x_true.s`). ⇒ flag polarity: `BRU.BRA` branches when the CMU flag is TRUE; the marker
does not skip fall-through code.

### K.3 Class-byte (LSU byte0) substitution in a minimal kernel

`probes/br/cls_<X>.s`: verified `LSU0.LDO.32 i0,i18,0x28`; `50`; `<X> 82 64 51 00` (which
would be `LSU0.LDO.32 i8,i18,0x28` for X = the right class); `50`; `LSU1.ST.32 i8,i0`.

| X | ACTUAL[0:4] | verdict |
|---|---|---|
| `c2` | `00 40 01 2e` | **i8 = out pointer (correct)** |
| `01` (gold's class!) | – | FAULT |
| `00` / `f0` / `b0` | – | FAULT |
| `bb` | `0f 00 00 00` | executes, wrong value (i8 != pointer) |

⇒ the class byte is load-bearing and *context-dependent* (gold's own `01` legal there, illegal
in a minimal kernel). Our assembler must keep emitting the table's `c2` class.

### K.4 Two building-block checks used by the probes above

* `IAU.ADD d, s, imm` — ROOT-CAUSED: **the `obj` path of `shave_asm.py` corrupts this form**
  (drops bit 31 of the 32-bit word, i.e. byte3 bit7, which is part of the vendor opcode mask
  `0x80000000|0xc6`), while the `asm --hex` path emits it correctly:

  ```
  source:           IAU.ADD i9, i9, 2   /   IAU.ADD i1, i1, 1   /   IAU.ADD i1, i1, 4
  asm --hex:        c6202581                c6208480                c6200482
  obj (read .text): c6202501                c6208400                c6200402
  ```
  A 13-instruction mixed file (CMU/SAU/LDO/ST/STI/BRA/JMP + IAU.ADD reg-form) shows *only* the
  AU-immediate form differing between the two paths, so the field map and the immediate
  semantics are correct — the object writer mangles the opcode.
  Consequences measured on silicon, and why the earlier table misled: through the *obj* path
  (base i1 = 0x12340000, base set with the pinned LDI pair) the mangled instructions produced
  addends 0 -> `0x40354000`, 1 -> `2x base`, 2 -> +0, 3 -> +3, 4 -> +0, 5 -> +2, 8 -> +15,
  16 -> +2 (2/2 each) — those numbers are the bug's signature, not ISA behaviour.
  Control with the CORRECT bytes hand-written (`br/g9_*.s`, dst = src = i9, base 0x12340000,
  2/2): `c6 20 25 80` -> `+0`, `c6 20 25 81` -> `+2`, `c6 20 25 84` -> `+8`, i.e. the immediate
  field applies verbatim (no `imm & 3` mask; the earlier "addend = imm & 3" model from the
  encoder review is REFUTED). Interim rule: do not build addresses/loop counters from
  `IAU.ADD d,s,imm` until the object writer is fixed (the register form is unaffected and
  measured correct: `IAU.ADD i1,i1,i2` with i2 = 4 gave +4, `br/ai_reg.s`).
* LDI pair + 12 slots of fill + store is the verified constant-materialisation path
  (`82 0a lo_lo lo_hi` `82 0b hi_lo hi_hi` = `LDIL i1,lo` + `LDIH i1,hi`); `probes/br/v_ldi2.s`
  lands `ef be ad de` for i1 = 0xDEADBEEF.

## Example L — E3/E4 effect-reads + a probe-design trap that must not be repeated

(note: renamed from a duplicate 'Example K' — the round-2 section above claimed K first;
VendorRE2, 2026-09-28)

### (a) The trap: never use the observed register as the fill register  ⚠
My first effect-read probes filled with `IAU.ADD i9, i9, i9` **while reading back i9**.  The
fills double i9 on every slot, so any register that is both a filler and an observation
target is meaningless.  Symptom and its control:

```
E4b  seeds i20=0x4444,i9=0x2222,i1=0x1111,i16=0x3333 ; candidate c1 39 24 48 49 ; read back
     window 44 44 00 00 | 00 00 00 11 | 11 11 00 00 | 33 33 00 00   (i9 = 0x11000000 ?!)
E4c  same probe with the candidate replaced by 5 x BRU.NOP
     window 44 44 00 00 | 00 00 00 11 | 11 11 00 00 | 33 33 00 00   (identical => artifact)
E4e  control with filler changed to `IAU.ADD i10,i10,i10`
     window 44 44 00 00 | 22 22 00 00 | 11 11 00 00 | 33 33 00 00   (all four seeds intact ✓)
```
Rule for every future probe: the fill register must be a register that is neither seeded nor
read back.

### (b) E4 verdict (candidate `c1 39 24 48 49`, CMU.CEX.U16 fmt1 tuple (i20,i9,i1,i16))
```
E4e control (5 x BRU.NOP) : i20=0x4444 i9=0x2222 i1=0x1111 i16=0x3333   (machinery valid)
E4d candidate             : i20=0x4444 i9=0x2222 i1=0x1111 i16=0x3333
```
⇒ the candidate executes without trap and changes **none** of the four registers of its fmt1
tuple, so the fmt1 reading is not confirmed (fmt0 tuple pending); it joins the
"executes-without-observable-effect in this class" set.

### (c) E3 verdicts (the 0x89-0x8c slice as a load)
```
E3  4B slice 49 80 20 59 , i8 seeded 0x1111, i16 seeded to the valid out pointer  -> execution failed
E3b 5B slice 49 80 20 59 14 (same seeds, plus i9/i0 seeded and read back)          -> execution failed
```
⇒ neither reading behaves as a benign load into i8/i9/i0; both trap.

### (d) Fresh table-shaped candidates (E6/E7) and the E4 ring scan

All with a non-colliding filler (`IAU.ADD i10,i10,i10`) and the validated readback shape.

```
E7  `70 12 00 10` @0x99 as LSU1.LDD64.H (ops i1,i0), i0 seeded to the valid out pointer
    -> i1 = 0x1111 (seed), i0 = 0x2E014000 (seed): executes, writes NEITHER -> tuple REFUTED
E6  `33 00 01 1b` @0x85 as VAU.ADD.I16/I32 (ops i0,i8,i22), all seeded 0x1111/0x2222/0x3333
    -> execution FAILED
E4r1 `c1 39 24 48 49` with i22,i23,i24,i29 seeded 0x5501..0x5504  -> all four read back unchanged
E4r2 same with i26,i27,i28,i20 seeded 0x5501..0x5504              -> all four read back unchanged
    => the CMU.CEX.U16 slice writes none of 12 candidate destinations
       (i1,i9,i16,i20 + i22,i23,i24,i26,i27,i28,i29): undetermined/benign, not identifiable this way
```

Cumulative result of the bit-level campaign (Examples H, I, J, K): every bit-sliced candidate
either traps or has no observable effect on any register its table shape predicts.  None of
gold's load-bearing runs (0x0a/0x32/0x36/0x57/0x61/0x6a/0x77/0x88/0x92/0x9c/0xa8/0xae) has
been named.  The byte-aligned and bit-aligned hypothesis spaces are now largely exhausted;
the remaining productive route is the CV3600xx table re-extract (`PACKET_SPEC.md` §4.2), after
which any *table-shaped* form can be judged in ~10 s with the effect-read protocol above.

## Example L — CV3600xx opcode-class sweep (partial, positive-only conclusions)

Protocol: `LSU0.LDO.32 i21,i18,0x28` (readback address) + `LSU0.LDO.32 i2,i18,0x28`
(i2 = *valid* address, so memory-class prototypes do not fault on the base) + seeds
`i1=0x1111`, `i3=2` (pinned 4-byte LDI pairs) + `<prototype bytes>` + readback of i1 then i2.
25 of the 79 prototypes were runnable with byte0 preserved by our assembler (the other 54 need
raw byte strings from AsmEncodingRE's extractor, because our assembler rewrites the opcode
class - e.g. `LSU0.STI.16.I8.I4` is legalized to the LSU1 form, `LSU0.LDIL fmt=100` emits the
3-byte 0x0a form instead of byte0 0x02).

**Trustworthy rows (window[0:4] = i1 after the candidate; semantic cross-check in the last column):**

| byte0 | prototype (bytes) | i1 after | interpretation |
|---|---|---|---|
| 0x82 | `82 0a 01 00` (LSU0.LDIL i1,1 fmt0) | `01 00 00 00` = 1 | **LDI low-half write ✓** (matches our pinned form) |
| 0xc6 | `c6 40 84 80` (IAU.ADD i1,i2,1 fmt1) | `01 40 01 2e` = 0x2E014001 | **i1 = i2+1 ✓ exact** |
| 0x86 | `86 50 84 01` (IAU.AND i1,i2,i3) | `00 00 00 00` | **i1 = ptr & 2 = 0 ✓ exact** |
| 0x46 | `46 49 04` (IAU.BSF i1,i2 fmt1) | `0e 00 00 00` = 14 | **i1 = lowest set bit of ptr(0x2E014000) = 14 ✓ exact** |
| 0x3c | `3c 44 84 21` (SAU.MUL.I8 i1,i2,i3) | `00 00 00 00` | **low byte 0x00 × 2 = 0 ✓** |
| 0x42 | `42 10 04 00` (LSU0.LD.64.H i1,i2) | `00 00 80 37` | memory read into i1 → **load class exists** |
| 0xc2 | `c2 12 04 03 00` (LSU0.LDO.64 i1,i2,1) | `01 00 00 ff` | memory read into i1 → **load class exists** |
| 0x1c | `1c 42 04` (SAU.SUMC i1,i2) | `ff ff ff ff` | register write (-1) → class exists |
| 0x21/0x22/0x41/0x70/0x80/0xc1/0xc9/0x12/0x2d/0x33/0xc9 | — | i1 = 0x1111 (seed), i2 = ptr | no observable register effect (store-class, predicate-class, or no-op decode) — **not** evidence of absence |
| 0x05, 0xc4, 0xc5, 0xc8 | — | execution failed | see caveat |

**Caveat that limits this table (important):** the traps above are instances of the
pattern-dependent legality class (`PACKET_SPEC.md` §5b/§14): in this run `a9 0b 01 00` (the
LSU1.LDIH shape that passes in E1c/E1d) FAILED, and the same candidates passed or failed
depending on the surrounding probe (Examples J/K) and on the register/immediate pattern (§14).
Therefore **negative rows must not be read as "class not implemented"**; only the positive,
semantically-consistent rows above are usable.  A publishable sweep needs (i) precomputed
address registers and no filler/increment register that the candidate could touch, (ii) a
register-clobber control, and (iii) per-row reproduction in the exact stream shape the backend
emits (see the appendix + Example K(a)).
Raw sweep output: `probes/cv_sweep_raw.txt`, probe sources `probes/C*.s`.

## Example M — CV3600xx class sweep round 2 (hardened harness, 43 prototypes, raw bytes from AsmEncodingRE)

Hardened skeleton (validated by a control probe: 4 readback slots = i1,i2,i3,i2 with seeds
0x1111 / 0x2E014000 / 2 and 6x `BRU.NOP` fills, address registers precomputed *before* the candidate,
no filler register in use, no increment register):

```
LSU0.LDO.32 i4, i18, 0x28        ; ptr
LSU0.LDO.32 i2, i18, 0x28        ; valid base for memory-class prototypes
6x 07
82 42 04 00 / 82 43 00 00        ; i8 = 4 (pinned LDI pair)
6x 07
IAU.ADD i5,i4,i8 ; IAU.ADD i6,i5,i8 ; IAU.ADD i7,i6,i8   (6x07 between each)
82 0a 11 11 / 82 0b 00 00        ; i1 = 0x1111
82 1a 02 00 / 82 1b 00 00        ; i3 = 2
6x 07
<candidate bytes>
6x 07
LSU1.ST.32 <r1>, i4 ; LSU1.ST.32 <r2>, i5 ; LSU1.ST.32 <r3>, i6 ; LSU1.ST.32 <r4>, i7  (6x07 between each)
BRU.JMP i30
```

| byte0 | bytes | prototype | silicon verdict (slot values, seed vs observed) |
|---|---|---|---|
| 0x04 | `0419c440` | LSU0.STI.16.I8.I4 i1, i2, i3 ; fmt=2 | execution FAILED (see caveat) |
| 0x05 | `05c2970000` | LSU0.CINVAL i1, 1 ; fmt=2 | i1=11 11 00 00 |
| 0x08 | `080a05` | CMU.CPZI i1, 1 ; fmt=1 | i1=11 11 00 00 |
| 0x0a | `0a0100` | LSU0.LDIL i1, 1 ; fmt=100 | execution FAILED (see caveat) |
| 0x0e | `0e0100` | LSU0.LDIL i1, 1 ; fmt=101 | i1=11 11 00 00 |
| 0x12 | `1219c440` | LSU1.STI.16.I8.I4 i1, i2, i3 ; fmt=2 | i1=11 11 00 00; i2=00 40 01 2e; i3=02 00 00 00 |
| 0x13 | `1319c4c8` | LSU1.STI.64.U8.U4S i1, i2, i3 ; fmt=1 | i1=11 11 00 00; i2=00 40 01 2e; i3=02 00 00 00 |
| 0x1c | `1c4204` | SAU.SUMC i1, i2 ; fmt=0 | i1=ff ff b1 ff; i2=00 40 01 2e |
| 0x20 | `20190400` | LSU1.ST.16.F32.F16 i1, i2 ; fmt=0 | execution FAILED (see caveat) |
| 0x21 | `212842` | CMU.CPZI i1, 1 ; fmt=0 | i1=11 11 00 00 |
| 0x22 | `222842` | CMU.CPZV i1, 1 ; fmt=1 | i1=11 11 00 00 |
| 0x23 | `23490400` | VAU.ACCPW.U32 i1, i2, 0 | i1=11 11 00 00; i2=00 40 01 2e |
| 0x2c | `2c520402` | SAU.ABS.F32 i1, i2 ; fmt=0 | i1=00 40 01 2e; i2=00 40 01 2e |
| 0x33 | `33408421` | VAU.ADD.I8 i1, i2, i3 ; fmt=0 | i1=11 11 00 00; i2=00 40 01 2e; i3=02 00 00 00 |
| 0x3c | `3c448421` | SAU.MUL.I8 i1, i2, i3 ; fmt=0 | i1=00 00 00 00; i2=00 40 01 2e; i3=02 00 00 00 |
| 0x41 | `411000` | CMU.CMZ.I32 i1 ; fmt=1 | i1=11 11 00 00 |
| 0x42 | `42100400` | LSU0.LD.64.H i1, i2 ; fmt=0 | i1=00 00 80 37; i2=00 40 01 2e |
| 0x46 | `464904` | IAU.BSF i1, i2 ; fmt=1 | i1=0e 00 00 00; i2=00 40 01 2e |
| 0x47 | `470a01` | BRU.SWP i1 ; fmt=0 | execution FAILED (see caveat) |
| 0x49 | `49108408` | LSU1.LD.128.I16.I32 i1, i2 ; fmt=0 | i1=00 01 00 00; i2=02 40 01 2e |
| 0x70 | `70120400` | LSU1.LDD64.H i1, i2 ; fmt=1 | i1=11 11 00 00; i2=00 40 01 2e |
| 0x80 | `80110900` | IAU.LFSR i1 ; fmt=0 | i1=11 11 00 00 |
| 0x81 | `812c0208` | CMU.CP.32 i1, i2 ; fmt=15 | i1=00 40 01 2e; i2=00 40 01 2e |
| 0x82 | `820a0100` | LSU0.LDIL i1, 1 ; fmt=0 | i1=01 00 00 00 |
| 0x84 | `84188407` | LSU0.ST.16.F16.U8 i1, i2 ; fmt=0 | i1=00 00 00 00; i2=00 40 01 2e |
| 0x85 | `85188403` | LSU0.ST.16.F16.U8FS i1, i2 ; fmt=0 | i1=11 11 00 00; i2=00 40 01 2e |
| 0x86 | `86508401` | IAU.AND i1, i2, i3 ; fmt=0 | i1=00 00 00 00; i2=00 40 01 2e; i3=02 00 00 00 |
| 0x87 | `87060000` | BRU.RPI i1 ; fmt=4 | i1=11 11 00 00 |
| 0x89 | `891d0401` | LSU1.LD.32 i1, i2 ; fmt=18 | i1=00 01 00 00; i2=00 40 01 2e |
| 0x92 | `92188402` | CMU.CP.16 i1, i2 ; fmt=0 | i1=11 11 00 00; i2=00 40 01 2e |
| 0x93 | `93188403` | LSU1.ST.16.F16.U8FS i1, i2 ; fmt=0 | i1=11 11 00 00; i2=00 40 01 2e |
| 0xa9 | `a90b0100` | LSU1.LDIH i1, 1 ; fmt=100 | i1=11 11 01 00 |
| 0xc1 | `c12a040000` | CMU.CMPI i1, 1 ; fmt=0 | i1=11 11 00 00 |
| 0xc2 | `c212040300` | LSU0.LDO.64 i1, i2, 1 ; fmt=1 | i1=01 00 00 ff; i2=ff ff 00 00 |
| 0xc4 | `c4408441` | CMU.PUTB.X16 i1, i2, i3 ; fmt=0 | i1=11 11 00 00; i2=00 40 01 2e; i3=02 00 00 00 |
| 0xc5 | `c5408401` | CMU.GETB.X32 i1, i2, i3 ; fmt=0 | execution FAILED (see caveat) |
| 0xc6 | `c6408480` | IAU.ADD i1, i2, 1 ; fmt=1 | i1=01 40 01 2e; i2=00 40 01 2e |
| 0xc7 | `c7010001` | BRU.BRA i1 ; fmt=102 | execution FAILED (see caveat) |
| 0xc8 | `c8408401` | CMU.COMPRESS.X32 i1, i2, i3 ; fmt=0 | execution FAILED (see caveat) |
| 0xc9 | `c916040300` | LSU1.STO.64 i1, i2, 1 ; fmt=1 | i1=11 11 00 00; i2=00 40 01 2e |
| 0xf0 | `f016440300` | LSU0.LDO.32 i1, i2, 1 ; fmt=114 | i1=11 11 00 00; i2=00 40 01 2e |
| 0xf9 | `f912440300` | LSU0.LDO.32 i1, i2, 1 ; fmt=113 | i1=11 11 00 00; i2=00 40 01 2e |
| 0xfa | `fa12440300` | LSU0.LDO.32 i1, i2, 1 ; fmt=111 | i1=ff ff ff 7f; i2=00 40 01 2e |

Interpretation (only *exact semantic* matches are counted as positive):

**Positive — class exists on silicon with the predicted effect:**
0x82 LDIL (i1 <- 1), 0xa9 LSU1.LDIH (i1 0x00001111 <- 0x1111 | high half 1), 0x81 CMU.CP.32
(i1 <- i2), 0xc6 IAU.ADD (i1 <- i2+1 = 0x2E014001), 0x86 IAU.AND (i1 <- i2 & i3 = 0),
0x46 IAU.BSF (i1 <- 14 = lowest set bit of the pointer), 0x3c SAU.MUL.I8 (i1 <- 0),
0x2c SAU.ABS.F32 (i1 <- |i2| = pointer, positive float), 0x1c SAU.SUMC (i1 <- 0xFFB1FFFF),
0x42 LSU0.LD.64.H / 0x89 LSU1.LD.32 (memory content into i1), 0x49 LSU1.LD.128.I16.I32
(load + base advanced to ptr+2), 0xc2 LSU0.LDO.64 (memory into i1), 0xfa LSU0.LDO.32 fmt111
(i1 <- 0x7FFFFFFF).

**Traps (NOT evidence of absence — pattern-dependent legality, PACKET_SPEC §5b/§14):**
0x04, 0x0a, 0x20, 0x47, 0xc5, 0xc7, 0xc8.  Two of these corroborate earlier results: 0x0a is
the 3-byte LDIL-shaped form that traps (and its sibling 0x0e executes but writes nothing) —
the same class that made gold's `42 0f 00` look like a load-immediate; and 0x20
(`LSU1.ST.16.F32.F16` in this shape) traps, matching the team's earlier "ST.16.F32.F16 faults"
observation.

**No observable effect (neutral, unidentifiable by register readback):** 0x05 CINVAL,
0x08/0x21/0x22 CPZI/CPZV, 0x12/0x13/0x85/0x84 store-class forms, 0x23, 0x33, 0x41, 0x70, 0x80,
0x92, 0xc4, 0xf0, 0xf9.  Predicate/conditional and store classes write no register or the
write is predicate-gated, so a register readback cannot see them.

Probes and raw results: `probes/cv/` (43 `.s` + index + results).
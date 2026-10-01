# PACKET_SPEC — SHAVE (npu3720 / `--cv 3720xx`) issue-packet, slot and LSU-result-delivery model

Author: PacketRE (agent BiologicalMockingbird).  Date: 2026-09-28.
Scope: what the vendor assembler (moviAsm64) puts in the byte stream, what the hardware
requires of that stream, and the minimal rules our assembler must follow so an LSU load's
result reaches its consumer.

Evidence tags used below:

| tag | meaning |
|---|---|
| **[HW]** | measured on the real device through the npunlock harness (`tmpdump/runp3.sh` → `dump_probe`, 32-byte oracle window; each probe re-run, deterministic) |
| **[TBL]** | read out of the vendor instruction-definition tables (extracted from moviAsm64.dll; `shave-isa/defs_*.pkl`, `encodings.json`) |
| **[DEC]** | read out of the decompiled vendor assembler (`r2gvp04w-re/re_decompiled/moviAsm64/decompiled_all.c`, line numbers cited) |
| **[STR]** | vendor string table (`re_strings/moviAsm64.ascii.txt`, line numbers cited) |
| **[INF]** | inference — not directly verified |

Probe sources and transcripts: `/tmp/pk/*.s`, `/tmp/pk/run*/*/bundle`, and
`VERIFIED_EXAMPLES.md` in this directory.  All probes are minimal kernels
(`controlled_act`), executed through the real smoke path; the observable is the 32-byte
oracle window dumped by `dump_probe`.

---

## 0. Actionable rules (what to implement) — all [HW]

**R1 — LSU write-back latency = 6 instruction slots.**
A register written by *any* LSU-unit instruction is not readable by a later instruction
until **6 instruction slots** have issued after it.  Consumers closer than 6 read the stale
value (or trap if the stale value is used as an address).

```
0..5 fill slots -> stale / fault        6+ fill slots -> correct value
measured with 1-byte (BRU.NOP), 3-byte (CMU.CMZ.I32) and 4-byte (IAU.ADD) fills:
  5 fills : FAIL (n5,c5 fault; s5 no-write)      6 fills : PASS (s6,c6,n6,n7,n8)
```
Because pipes are groups of ≤3 instructions, "6 slots" = **two complete pipes** of
distance.  This is the load-delay the vendor exposes as `-forceLoadDelay:<loadDelay>`
[DEC/STR, moviAsm64.ascii.txt:2721].

**R2 — one LSU unit per issue group; a consumer must be on the *other* LSU unit or ≥6 slots later.**
`LSU0.LDO.32` + 6 fills + `LSU1.ST.32` = correct (u5, n6, c6).  Same unit
(`LSU0.LDO` + 6 *or* 12 fills + `LSU0.ST.32`) = garbage written (u1 `00 5c b4 8f`, u2
`00 0c 00 8f`) — distance does **not** fix it.  This is the hardware face of the vendor
diagnostics `LSU0 unit is used more than once in the same pipe` /
`LSU1 unit is used more than once in the same pipe` [STR:2616-2617].
Our assembler emits no pipe boundaries, so the only safe rule today is: *keep an LSU
producer and its LSU consumer on different LSU units, or ≥6 slots apart and never on the
same unit.*

**R3 — constant materialisation must use the 4-byte LDI forms.  Our assembler currently emits an invalid LDIL.**
Hardware-verified encodings (bytes little-endian):

```
LSU0.LDIL  reg, lo16 :  82 (reg<<3|2) lo16            (4 bytes)  [HW: r2 reg2, r3/f1 reg20]
LSU0.LDIH  reg, hi16 :  82 (reg<<3|3) hi16            (4 bytes)  [HW: r5 -> 0x1234ABCD]
LSU1.LDIH  reg, hi16 :  a9 (reg<<3|3) hi16            (4 bytes)  [HW: v6 -> 0xABCD0000, p2 pair]
LDIL then LDIH (adjacent, or with gaps) = full 32-bit constant  [HW: p2, p4, p6, r5]
```
These are exactly the vendor table forms `[imm16][IW reg5][D op11]` with
`D=0x282` (LSU0.LDIL), `D=0x382` (LSU0.LDIH), `D=0x3a9` (LSU1.LDIH) [TBL].
`shave_asm.py` today emits **3-byte** `(reg<<3|2) lo16` for `LSU0.LDIL` (verified by
disassembling its own output: `LSU0.LDIL i20, 0xABCD` -> `a2 cd ab`).  On hardware that
byte triple is **not** an LDIL: it writes nothing, writes 0, or clobbers a register and
traps (v1, v4, ld0..ld7 sweep).  This single defect explains "LSU-written values never
reach consumers": every 32-bit constant our compiler materialises with LDIL/LDIH is
garbage, so every compare against a constant (the entry guards!) decides arbitrarily and
the kernel returns before the store.

**R4 — pipes are ≤128-bit groups of ≤3 instructions; there is no separate packet header byte.**
[STR/DEC] `… might generate a stall (> 128 bits)`, `-L : Optimize labels' address
(align to 128 bits)`, and the pipe parser rejects a unit used twice per pipe.
No instruction in the extracted tables other than `0x07` (`BRU.NOP`/`BRU.INC`) has a
1-byte encoding [TBL scan of 10,941 forms], so the load-bearing single bytes seen in gold
(`40`,`03`,`c0`,`eb`,`10`,`30`) are **not** standalone NOP/descriptor instructions — they
are bytes of instructions our tables/reference decoder do not yet model (most plausibly
the 4-byte LDI family and the `CMU.CP.*` pack/unpack ops, see §4).

**R5 — Recipe for a load-bearing sequence our assembler can emit today** (all [HW]):

```
    LSU0.LDO.32  d, i18, 0x28      ; 5B  c2 (d<<4|2) ((18<<1)|0) ((0x28<<1)|1) 00
    BRU.NOP  x6                    ; 07 x6  (or any 6 instructions that touch nothing)
    <first consumer of d>          ; not earlier than slot 6
    ...
    LSU1.STI.16.F32.F16 v, d       ; store through d on the *other* LSU unit
```
`LSU0.LDO + 6xANY + LSU1.ST` and the full chain `LDO(ptr) ; LDO(rank) ; 6 fills ;
IAU.ADD ; 6 fills ; LSU1.ST` both produce exactly the expected value [HW: n6/c6/u5/`chain`].

---

## 1. The vendor's own model (source level)

Source syntax is a pipe of up to three operations: `slot0 || slot1 || slot2`
(`shave_asm.py` docstring + vendor diagnostics).  Rules the vendor *enforces at assembly
time* (all [STR], `moviAsm64.ascii.txt`):

| line | string | meaning |
|---|---|---|
| 2557,2559,2570,2616,2617,2630,2632,2641 | `<UNIT> unit is used more than once in the same pipe` | per-pipe resource check, one op per unit |
| 2631 | `Incorrect input of slots literal parameter: %s.` | pipes carry an explicit slot count in the source |
| 2605,2886 | `Invalid .salign specificator` / `.salign` | slot-alignment directive |
| 2476,2477,2491 | `…might generate a stall (> 128 bits): %d.` | pipe size limit = 128 bits |
| 2492 | `There are side effects after delay slots, remaining latency cycles: %d` | delay slots + latency model |
| 2495 | ` has no effect in pipe. There is no ` | operand/unit has no effect in a given pipe |
| 2701,2702,2703 | `Invalid NOP in pipe`, `Double pipe found`, `Invalid continued pipe after %s` | pipe syntax checks |
| 2755-2757 | `--noCompressedNOPs`, `--noFinalSlotCompression`, `--noSlotAllPromo` (`…: Disable the SLOT_ALL promotion for labels alignment (ma2xxx only)`) | NOP compression + slot promotion are real assembler stages |
| 2721 | `-forceLoadDelay:<loadDelay>` | explicit load-delay control (R1) |
| 2717 | `-forceDelaySlots:<delaySlots>` | explicit branch delay-slot control |
| 2739 | `-a : Disable the code analyzer` | the conflict/latency analyzer |

`3720xx` selects the **CV3600xx** platform table, *not* Myriad2 [DEC]:
`FUN_18002cb90` (chip-version → platform, decompiled_all.c:25560-25760) matches the
version string in a nested chain; the `"3720xx"` branch falls through the chain to
`FUN_1800325a0(...,1)` (line 25731), which installs `CV3600xx::vftable`
(decompiled_all.c:24116-24130).  CV3600xx tables are built by `FUN_18042d4a0` +
`FUN_18042d4c0` (a 4,689-byte dispatcher, decompiled_all.c:1845) into ~156 per-family
builder functions.  **FUN_180434100 is the Myriad2Platform builder** — it is *not* the
platform for `--cv 3720xx`; its 1,495 defs are still useful as a cross-check but its
opcode constants differ (see R3/§5).

### 1.1 Annotated decompiled functions (renamed for this spec)

Confidence [DEC]; my renaming is [INF].

**(a) `analyzer_emit_delay_fill()` = `FUN_18005b9e0` (decompiled_all.c:57171)**
Emits the fill for an instruction's delay slots and applies NOP compression:

```c
void analyzer_emit_delay_fill(ctx *c, insn *ins, uint nr_bits) {
  /* RENAMED: FUN_18005b9e0 */
  u8 noCompressedNOPs = *(char *)(*(u64 *)(c + 0x1f0) + 0x4c);   /* options+0x4c */
  u8 noFinalSlotCompression = *(char *)(*(u64 *)(c + 0x1f0) + 0x4d); /* +0x4d */
  if (nr_bits < 1 || (noCompressedNOPs && c->x120 != -1)) {
      if (nr_bits == 0) nr_bits = 1;
      do { emit_one(c, ins); c->x218[8]=1; emit_one(c, ins); tick_counters(c); }
      while (--nr_bits);
  } else {
      uint units16 = nr_bits >> 4;                 /* 16-BIT SLOTS */
      if (!noFinalSlotCompression || c->x120 == c->xa8) {
        while (units16--) {                        /* 16 compressed NOPs at a time */
            emit_one(c, ins); c->x218[8]=1; c->x218[0x5c]=0xf0; emit_one(c, ins);
            for (lVar8=0x10; lVar8; lVar8--) tick_counters(c);
            ...
        }
      } else {
        nr_bits = FUN_18005f070(c,0x10,ins,ins2);  /* emit_nop_run */
      }
      nr_bits &= 0xf;                              /* remainder < 16 */
      ...  c->x218[0x5c] = nr_bits*0x10 - 0x10;    /* ANN: "NOP run length - unit" */
  }
}
```
Key readings: (i) the fill quantity is a **bit/cycle count** (`nr_bits`), consumed in
**units of 16** (`>>4`, `&0xf`), with one instruction emitted per unit — this is the
"compressed NOP" generator; (ii) the emitted NOP object carries a size field
`+0x5c = n*0x10 - 0x10`; (iii) the two flags live at **options+0x4c/0x4d**
(set by the CLI handler `FUN_18002xxx` at decompiled_all.c:16322-16356 for
`-noCompressedNOPs` / `-noFinalSlotCompression`).

**(b) `emit_nop_run_upto_block_end()` = `FUN_18005f070` (decompiled_all.c:57188)**
Clips a NOP run to the current ≥128-bit block: it compares the running slot index
`c->x120` with the block size `*(c->x128 + 0x50)` and sets `+0x5c = (avail-1)*0x10 - 0x10`
before continuing.  [INF] the `0x10` unit and the `>128 bits` stall warning identify the
issue block as **128 bits**, i.e. the same constant as `-L`'s label alignment.

**(c) `parse_slots_literal()` = `FUN_18007afc0` (decompiled_all.c:79745)**
Error builder for `"Incorrect input of slots literal parameter: %s."` — the source form
carries an explicit **slots** literal per pipe; its parser is `FUN_180036d70`
(decompiled_all.c:27765), a generic 0..N literal reader reused by the source parser.

**(d) `conflict_warning()` = `FUN_18003c5d0` (decompiled_all.c:32844)**
Formats `"Instruction %s may be conflicting: %s"` from the instruction's two operand/unit
name strings (`param_1+0x30`, `param_1+0x50`); emit site for
`WARN_INSTRUCTION_MAY_BE_CONFLICTING` (RTTI at moviAsm64.ascii.txt:7792).  Companion
`FUN_18003c4f0` builds `"Instruction %s is conflicting: %s."` / `ERR_INSTRUCTION_IS_CONFLICTING`.
These are the analyzer's per-pipe resource/hazard checks the hardware silently mirrors as
R2 [HW].

Without wine we cannot run the reference assembler, so (a)-(d) are read-only evidence; the
*numbers* they use are confirmed independently by [HW] and [STR].

---

## 2. The hardware-verified model

### 2.1 What a "packet"/pipe is
* A pipe is the vendor's issue group: 1..3 operations, total encoded size ≤128 bits
  [STR], one operation per unit [STR], assembled from the `a || b || c` source form.
* The byte stream is a **plain concatenation** of the member encodings, in slot order,
  each member padded to its own byte multiple (verified by the 192/192 byte-identical gold
  round-trip in `shave-isa/VALIDATION.md`); there is no packet header/length field that we
  could find [INF: negative result over 192 gold bytes + 187-file ACT corpus].
* Therefore the *hardware* must infer the issue groups from the instruction stream itself.
  Two mechanisms are consistent with all observations:
  1. per-instruction opcode variants that mark slot/position (the vendor table contains
     several encodings per operation: e.g. `LSU0.LDO.32` has 6 distinct operand-shape
     defs plus the corpus classes `c2/f9/fa/01/f0/fc/bb/b0/c3/c9/08`); and
  2. the assembler's NOP-compression/delay-slot fill bytes (§1.1a).
  We could **not** isolate a single "slot index" bit field; every class byte we tested that
  differed from the one gold uses either produced wrong values or a trap [HW, iteration 20c
  patch matrix: `01`->`c2`/`00`/`10`/`40` = wrong values, `f9`/`fa` = fail].
  **Confidence: HIGH that the class byte is semantically load-bearing; LOW on its exact
  bit meaning.**

### 2.2 LSU result delivery (the actual blocker)
Measurement (minimal kernel: `LSU0.LDO.32 i0,i18,0x28` then N fills then `LSU1.ST.32 i0,i0`;
the store's address *and* value come from the loaded pointer, so the oracle window shows the
pointer only if the load delivered):

| N fills | 1-byte BRU.NOP | 3-byte CMU.CMZ.I32 | 4-byte IAU.ADD |
|---|---|---|---|
| 0..2 | fault | – | fault |
| 3..5 | no write | fault (5) | no write (3,4), fault (5) |
| **6..12** | **correct (`00 40 01 2e`)** | **correct** | **correct** |

* The unit is the **instruction slot**, not bytes (1-byte and 4-byte fills behave
  identically at 5 vs 6).
* Same-unit reuse breaks regardless of distance: `LSU0.LDO`+6/12 fills+`LSU0.ST` wrote
  garbage (`00 5c b4 8f`, `00 0c 00 8f`); the LSU1 store form is correct.
* Chain plausibility check: `LDO(ptr)`+`LDO(rank)`+6 fills+`IAU.ADD i2,i1,i1`+6
  fills+`LSU1.ST.32 i2,i0` wrote `08 00 00 00` = 2×rank(=4) — a full load→ALU→store
  datapath in *our* encoding without any vendor descriptor bytes.  **This is the existence
  proof that load-result delivery does not require any unknown "descriptor" byte — only
  spacing and unit discipline.**

### 2.3 LDI (constant materialisation) family
[Table forms, TBL] 32-bit, packed first-record-at-top then LE:

```
LSU0.LDIL : [imm16][IW dst5][D=0x282 11b]   ->  82 (dst<<3|2) imm16_le
LSU0.LDIH : [imm16][IW dst5][D=0x382 11b]   ->  82 (dst<<3|3) imm16_le
LSU1.LDIH : [imm16][IW dst5][D=0x3a9 11b]   ->  a9 (dst<<3|3) imm16_le
LSU1.LDIL : [imm16][IW dst5][D=0x289 11b]   ->  89 (dst<<3|2) imm16_le   [unverified on HW]
```
Hardware results (see VERIFIED_EXAMPLES §2 for transcripts): `82 (r<<3|2) lo` writes the
**low** half, `82 (r<<3|3) hi` writes the **high** half, `a9 (r<<3|3) hi` writes the
**high** half; `82…lo` followed by `82…hi`/`a9…hi` yields the full 32-bit constant.
The 3-byte `(r<<3|2) lo` form (what `shave_asm.py` emits for `LSU0.LDIL`) is not accepted
as an LDIL by the silicon.

---

## 3. Confidence table

| claim | tag | confidence |
|---|---|---|
| pipes ≤128 bits, ≤3 ops, one op per unit | STR+DEC+HW | HIGH |
| NOP compression / delay-slot fill exist and are emitted by the assembler | STR+DEC | HIGH |
| LSU write→read distance must be ≥6 instruction slots | HW | HIGH (deterministic, 3 fill sizes) |
| same-unit LSU reuse after an LSU producer corrupts data | HW | MED-HIGH (LSU0 pair 2×; LSU1 load form itself also trapped) |
| our `LSU0.LDIL` 3-byte emit is invalid; 4-byte `82…` form is correct | HW+TBL | HIGH |
| class byte (byte0) variants are semantically load-bearing | HW (gold patch matrix) | HIGH |
| a *single* bitfield in byte0 encodes the slot index | – | LOW — not isolated; treat as unknown |
| gold's 1-byte runs (`40`,`03`,`c0`,`eb`,`10`,`30`) are compressed NOP/descriptor opcodes | – | LOW — no such 1-byte instruction exists in the extracted tables; most likely they are bytes of 4-byte instructions our tables/decoder mis-segment (unresolved) |
| `--cv 3720xx` -> CV3600xx platform (not Myriad2) | DEC | MED-HIGH (code path, but the tables we extracted are dominated by FUN_180434100) |

## 4. Open items that block a *complete* packet spec
1. Gold's undecoded runs (0x0a, 0x1e-0x26, 0x2b-0x2e, 0x32, 0x36-0x39, 0x3e-0x42,
   0x46-0x48, 0x4d-0x4f, 0x57, 0x61, 0x6a, 0x77-0x7a, 0x7e, 0x88-0x96, 0x9c, 0xa8, 0xae)
   contain bytes that are *load-bearing* when NOP-ed.  The 4-byte LDI discovery (R3) means
   at least some of them are the opcode byte of a 4-byte instruction whose 3 trailing bytes
   our decoder attributed to the *next* pseudo-instruction — this re-segmentation has not
   been completed.  Concrete test already run: gold's verbatim `40 0e 88 ac` does **not**
   behave like `LSU0.LDIL i1,0xac88` on hardware (t4: trap), and `40 (r<<3|2) imm16`
   writes nothing (t1/t2/t3/t5) — so byte0=0x40 is *not* the CV3600xx LDIL opcode either.
2. The CV3600xx builder family (`FUN_18042d4a0`/`FUN_18042d4c0` and its ~156 callees) has
   not been re-extracted; `defs_platform.pkl` is dominated by Myriad2
   (`FUN_180434100`) defs.  A focused re-extract of the CV3600xx callees is the cheapest way
   to close (1).
3. `LSU1.LDIL` (`89 …`) and the `0x20` opcode overlay observed in the corpus
   (`a9` vs table `0x389`) remain unverified on hardware.

## 6. Status of the fix (revision 2, after cross-validation)

* AsmEncodingRE pinned the 4-byte LDI forms in `shave-isa/encodings.json` (cal_fix: `LSU0.LDIL`
  `82|(reg<<3|2)|lo16`, `LSU0.LDIH` `82|(reg<<3|3)|hi16`, `LSU1.LDIH` `a9|(reg<<3|3)|hi16`,
  `LSU1.LDIL` 0x89-class) and demoted every 3-byte LDI form to `encode_priority 4` so hintless
  resolution can never pick them; regression 19/19 byte-exact, gold round-trip 192/192.
* Cross-validation: my ten probe sources re-assembled with the pinned tool reproduce the
  byte strings in `VERIFIED_EXAMPLES.md` exactly (r5, u1, n6 identical; v6 now deterministically
  takes `82` for LSU0.LDIH and `a9` for LSU1.LDIH), i.e. the rule set is assembler-reproducible
  end-to-end, not just hand-written bytes.
* Hardware A/B verdicts on the candidate re-segmentations of gold (Example H): the 4-byte
  LDI-class readings `40 0e 88 ac` / `03 aa 4a 01` / `01 eb 50 ea` are **refuted** (write
  nothing or trap), while a control pair in the same placement works (`08 32 ac 88`).
  Consequences in §4.1: gold's load-bearing single bytes are still unexplained; next step is
  either a bit-level segmentation or a CV3600xx-specific table re-extract.

## 7. Practical guidance for our assembler (implementable today)
1. **Fix LDIL**: emit `82 (reg<<3|2) lo16` (4 bytes) — i.e. select the table form with
   opcode 0x282, not the corpus-derived 3-byte form.  Keep `LSU1.LDIH` as `a9 (reg<<3|3) hi16`.
2. **Space every LSU producer from its consumer by ≥6 instruction slots** (schedule or pad
   with `BRU.NOP` = `07`).  If the consumer is itself an LSU op, put it on the *other* LSU
   unit as well.
3. Prefer `LSU0` for loads and `LSU1` for stores (matches gold: `LD.32.F16.F32` = LSU0,
   `STI.16.F32.F16` = LSU1) and never let an LSU op read a register written by an LSU op
   fewer than 6 slots earlier.
4. Do not emit the "1-byte descriptor" guesses: our tables contain no such instruction, and
   `07` (`BRU.NOP`) is the only legal 1-byte filler.

## 5a. Note on `tools/defdec.py` (sibling contribution)

`tools/defdec.py` decodes a byte stream directly against the extracted vendor record
lists.  Smoke test on gold `.text` (`python3 tools/defdec.py <gold.bin>`):

```
0x0000 c2a2641900  LSU0.LDO.32    ok
0x0005 c272642100  LSU0.LDO.32    ok
0x000a .byte 40 ... 0x0017 .byte 01   (the whole 0x0a-0x17 run undecoded)
0x0018 420f0032    LSU0.SWZMC.16.8   <-- greedy mis-segmentation:
                                          swallows `42 0f 00` (which our other
                                          decoder calls an LDIL) plus the next byte
```
Conclusion: byte-aligned greedy decoding against the Myriad2-dominated record space cannot
resolve gold's runs *and* actively mis-cuts sites the reference decoder round-trips
byte-exactly.  Gold's undecoded bytes therefore need either a bit-level segmentation or the
CV3600xx-specific tables (§4.2) before any candidate can be named — consistent with the
negative A/B results in `VERIFIED_EXAMPLES.md` Examples H/I.

## 5b. Immediate/register field behaviour — CORRECTED (rev 4, reconciled with §14)

An earlier revision claimed "imm16 bit 15 set together with any lower bit traps" for the
`82 (reg<<3|3) imm16` LDIH shape; the next revision retracted that as a probe artifact.  Both
statements are now superseded by the reconciled, independently reproduced result:

**Encoding legality is a PATTERN property of the whole encoded word — form x register field x
immediate bits.  It is not a property of any single bit, and it is not a fixed register set.**

Evidence for the pattern dependence (two independent probe families, both deterministic):
* *Immediate-dependent, register fixed* (my sweep, `82 73 imm16`): `0xABCD`, `0x9048`,
  `0x8001`, `0x8000`, `0x1234`, `0x7FFF` and others execute (`i14` gets the high half), while
  the same bytes with `0x9000`, `0xFFFF`, `0xA000`, `0xC000`, `0x8048`, `0x8080`, `0x8002`
  trap — *in one probe skeleton*; several of those values execute in a smaller skeleton
  (E5/S3/S5/T1).
* *Register-dependent, immediate fixed* (§14, sibling probes in a different clean skeleton):
  `82 (R<<3|2) ef be` + `82 (R<<3|3) ad de` traps for R = 19, 21, 23, 24, 30 and executes for
  R = 16,17,18,20,22,25..29,31 — and the *same* R flips to "ok" when the immediate changes to
  0x0001/0x0000 (R = 21,23,24) while R = 19,30 keep trapping.
* The 5-byte `LSU0.LDO.32` shows the same shape of dependence on the destination register.

Consequences (these are what matters for the toolchain):
1. The vendor table's field map is **necessary but not sufficient**: shape match never proves
   executability.
2. Every `(form, register, immediate)` combination the backend/assembler will actually emit
   must be effect-checked on silicon; safe conservative defaults measured so far are
   destinations r0..r15 in LSU forms, LDIL/LDIH pairs with small immediates, the 5-byte
   `LSU0.LDO.32`, and 2-operand `LD.32` only for dst < i16 (§14).
3. Byte-identical instructions are *not* context-free: the same candidate can trap in one
   surrounding stream and execute in another (my E-skeleton vs S-skeleton), so a single
   pass/fail probe never establishes legality — always reproduce in the stream shape the
   backend will emit.


---

# ROUND 2 (agent VendorRE2, 2026-09-28) — compressed NOPs, branch delay descriptors,
# revised gold byte map, class-byte status, packet-emission recipe

Probe sources: `probes/q_*.s`, `probes/cn_*.s`, `probes/ca_*.s`, `probes/br/*.s`
(each verdict repeated 2-3x; assembler invoked with `--no-legalize` through
`tools/runp3raw.sh` so the new auto-legalizer cannot insert fillers behind our back).
`tools/runp3raw.sh` = copy of `llvm-14/tmpdump/runp3.sh` with
`ASMFLAGS="${ASMFLAGS:---no-legalize}"` passed to `shave_asm.py obj`.

> METHOD NOTE: the current `runp3.sh` runs the new AUTO-LEGALIZER
> (`legalize: 6 delay-slot NOP(s)`), which silently inserts the 6-slot fill. A first sweep
> was invalidated by it (everything "passed"). All results below use `--no-legalize`.

## 8. The vendor compressed-NOP (filler) encoding — [HW]+[DEC], HIGH

**Rule.** A single byte with **low nibble 0** encodes a NOP run covering
`n = (byte >> 4) + 1` instruction slots:

| slots n | byte | load-visibility verdict (R2 gate) |
|---|---|---|
| 1 | `00` | fail (stale/fault) |
| 2 | `10` | fail |
| 3 | `20` | fail |
| 4 | `30` | fail |
| 5 | `40` | fail |
| **6** | **`50`** | **PASS — oracle = `00 40 01 2e`** |
| 7 | `60` | PASS |
| 8 | `70` | PASS |
| 9..15 | `80`..`e0` | PASS |
| 16 | `f0` | PASS |
| `07` = `BRU.NOP` (single slot) | | 5x fail / 6x PASS (classic R2 sweep) |

* `probes/q_<byte>.s` = `LSU0.LDO.32 i0,i18,0x28` -> **one byte** -> `LSU1.ST.32 i0,i0` ->
  `BRU.JMP i30`. `q_50` is behaviourally identical to the 6x `BRU.NOP` control: the load
  result reaches the store.
* **Counts add**: `20 20` (3+3), `40 07` (5+1, 3/3), `20 07 07 07` (3+3, 3/3),
  `00 07 07 07 07 07` (1+5, 3/3) all PASS; `20 07 07` (3+2 = 5, expected fail) faulted
  3/3 (one earlier outlier PASS existed - the NPU can be flaky when siblings run probes;
  always repeat).
* **Low nibble != 0 is not a NOP**: `4f`,`5f`,`0f`,`17`,`27` fail; **`57` writes a stale
  value** (executes as some *other* instruction, does not fault).
* Vendor code that produces this field [DEC]: `FUN_18005b9e0` (decompiled_all.c:57169,
  analyzer_emit_delay_fill) writes the per-packet record field `+0x5c = nr_bits*0x10 - 0x10`
  (tail, line 57326) and `0xf0` for a full 16-slot unit (line 57249); `FUN_18005f070`
  (emit_nop_run, line 59801) and `FUN_18005be70` (line 57412) clip a run to the >=128-bit
  block with the *same* formula; the vendor's own printer `FUN_1800642d0` decodes the field as
  `(field >> 4 & 0xf) + 1` slots (line 63160 - exactly `n = (b>>4)+1`). The switches
  `-noCompressedNOPs` / `-noFinalSlotCompression` are options+0x4c/+0x4d read at the
  `FUN_18005b9e0` entry (lines 57186-57195); with them set the *uncompressed* path emits
  `nr_bits` real NOP instructions whose opcode comes from the latency table
  (`+0x40 = 7` = `BRU.NOP`, FUN_18005be70:57416).
* **Unresolved:** the low-nibble-`7` family. FUN_18005be70 sets `opcode = 7` *and*
  `count = (...-1)*0x10` on the same record (which would suggest a multi-slot `BRU.NOP`
  `0x17`,`0x27`,...); silicon rejects those as fillers, so the byte form for that family is
  not established.
* **BRU.NOP vs compressed NOP - when the vendor emits which**: gold (assembler defaults, no
  `-noCompressedNOPs`) emits the **compressed** form for every run of >=1 slot
  (`0x40`,`0x50`,`0x10`,`0x30`,`c0`) and reserves `07` for single-slot fills (gold bytes
  0x80/0x97/0xa9). With `-noCompressedNOPs`/`-noFinalSlotCompression` the vendor emits
  `07`-runs instead [DEC] - i.e. **byte-parity requires the compressed form**.

## 9. Branch delay: the marker byte + fill — [HW], HIGH (behaviour)

**R-B1 - every conditional branch needs a marker byte.** `BRU.BRA <flag>,tgt`
(4 bytes `b5 disp16_lo disp16_hi flag`) with *nothing* between it and the next instruction
**does not execute** (device fault). A taken branch needs a **branch marker byte of the
`?b` class** immediately after it (measured working: `0b`, `2b`, `4b`, `8b`, `cb`, `eb`;
`1b` works only when another byte follows):

| probe (after `BRU.BRA iN,TGT`) | next bytes | result |
|---|---|---|
| `br/t_none.s` | (store) | **FAULT** (2/2) |
| `br/t_1b40.s` | `1b 40` | target store ran (TAKEN, 2/2) |
| `br/t_2b.s` | `2b` | TAKEN (3/3) |
| `br/t_40.s`, `t_50.s`, `t_1b.s` | `40` / `50` / `1b` | **FAULT** |
| `br/s0b,s4b,s8b,scb` (`0b/4b/8b/cb` + store) | 1 byte | TAKEN |
| `br/s3b,s5b,sdb,sfb` | 1 byte | FAULT |

**R-B2 - the marker is followed by a §8 compressed NOP covering part of the 6-slot delay
window.** Delay-slot counter probe (`br/dc_*.s`: `i1 = 1`, eight `IAU.ADD i1,i1,i1` after the
marker, the surviving store writes `2^k`):

| marker+fill | k (real slots executed) | => window covered |
|---|---|---|
| `1b 20` | 3 | 3 NOP + 3 real = 6 (2/2) |
| `1b 40` | 1 | 5 NOP + 1 real = 6 (2/2) |
| `1b 50` | 0 | 6 NOP + 0 real (2/2) |
| `1b 60`, `1b f0` | 0 | >=6 NOP (2/2) |

=> the branch delay window is **6 slots** (= `shave-branch-delays`); the vendor descriptor is
`marker + (6-n)`-slot NOP. **A `1b 50` marker makes the branch effectively immediate**, which
is why agent MedicalCrayfish measured "immediate branch" for the `1b 40`-style descriptors.
The backend must NOT assume "6 instructions after the branch always execute" - the fill
declares how many do.

**R-B3 - the not-taken path is unaffected by the fill.** `br/x_false.s` (`CMU.CMZ.I32 i2`
with i2=2 => false) + `1b 50` => the fall-through store runs; the same skeleton with a true
flag faults in the target garbage (`br/x_true.s`). `br/nf.s` shows the instruction
immediately after the marker executing on the not-taken path. Flag polarity re-confirmed:
`CMU.CMZ.I32 r` (or `CMU.CMII.U32 a,b`) defines the flag named by operand 0, and `BRU.BRA`
branches when it is **true** (FINDINGS R4).

**Gold's five descriptors** (gold `.text`):

| branch | direction | descriptor | fill slots | real delay slots |
|---|---|---|---|---|
| 0x53 -> 0xb7 | forward | `1b 40` | 5 | 1 |
| 0x66 -> 0xb7 | forward | `2b 40` | 5 | 1 |
| 0x73 -> 0x4a | backward | `eb 30` | 4 | 2 |
| 0x84 -> 0xb7 | forward | `1b 50` | 6 | 0 |
| 0xa4 -> 0x98 | backward | `eb` + `07` | 1 | 5 |

**Unresolved (MED):** the marker's *high nibble* (`1b`/`2b` forward vs `eb` backward in all
5 gold samples) - it did not correlate with the displacement, the target, or the instruction
count, and has no dependency on position mod 16 (class-byte corpus scan, §10). Candidate
reading: branch class (direction/predictor/pipe); unproven.

## 10. Class byte (LSU operand byte0) - status

Measured (`probes/br/cls_*.s`: verified LDO loads the out pointer into i0, then the 5-byte
`X 82 64 51 00` -> 6-slot fill -> `LSU1.ST.32 i8,i0`):

| X | result |
|---|---|
| `c2` | **PASS - i8 = out pointer `00 40 01 2e`** (the table's own opcode 0x2c2) |
| `01` | FAULT |
| `00`, `f0`, `b0` | FAULT |
| `bb` | executes, wrong value (`0f 00 00 00`) |

Vendor corpus scan (187 vendor `*.3720xx.elf`, 1.37 MB of `.text`): the 4-byte payload
`82 64 51 00` (dst 8, base i18, imm 0x28 = the out-pointer load every kernel needs) occurs
12x, preceded by `c2`(3), `01`(2), `f0`(2), `b0`(2), `bb`(2), `00`(1); the class does **not**
correlate with byte offset mod 16 (no alignment/slot-position law).
Table side: the CV3600xx builder for `LSU0.LDO.32` is `FUN_1804e4740`
(decompiled_all.c:220765-220990), reached from the CV3600xx dispatcher `FUN_18042d4c0`, which
is installed by `FUN_18007eed0` -> `CV3600xxPlatform::vftable` selected for the `3720xx`
chip string (decompiled_all.c:20319-20332, 83090-83100); all **7** of its LDO.32 shapes carry
opcode **0x2c2** (byte0 = `c2`) and none of the corpus classes (`01/f0/b0/bb/00`) appears in
the extracted record set (extraction covers 58 of that dispatcher's 78 callees).

=> **Actionable**: emit `c2`; do not attempt to reproduce the vendor's other classes.
=> **Unresolved**: what `01/f0/b0/bb/00` select. The mechanism is almost certainly the
vendor's per-platform **slot-descriptor** machinery (0x48-byte entries at `ctx+0x218+0x80`
consumed by `FUN_18005e7c0`/`FUN_1800b55c0`; `-noSlotAllPromo`/`SLOT_ALL`), i.e. an
instruction-level "which issue slot/queue" class. Evidence: the same mnemonic appears with
several distinct encoding classes in vendor output, and a class legal in gold's context
faults in a minimal kernel (context-dependence).

## 11. Revised gold `.text` byte map (0x00-0xbf, 192 B)

`HW` = silicon-verified form, `HW-class` = silicon-verified instruction *class* (bytes from
the shared decoder/table), `SEQ` = field layout verified, `?` = unresolved but byte-accounted.

```
off  bytes              len  instruction / role                                   conf
0x00 c2 a2 64 19 00      5   LSU0.LDO.32 i10,i18,0xc   (rank)                     HW
0x05 c2 72 64 21 00      5   LSU0.LDO.32 i7,i18,0x10   (dims)                     HW
0x0a 40                  1   compressed NOP 5 slots                              HW
0x0b 0e 88 ac            3   UNRESOLVED (4-byte-LDI reading refuted on HW)
0x0e 0a 08 32            3   UNRESOLVED
0x11 00 00               2   00 = NOP 1 slot  +  1 byte UNRESOLVED               part HW
0x13 6e ea 70            3   UNRESOLVED
0x16 0c 01               2   UNRESOLVED
0x18 42 0f 00            3   UNRESOLVED
0x1b 32 01 00            3   UNRESOLVED
0x1e a3 fc ff 0f 28 c8   6   UNRESOLVED (0x1e-0x23; 4-byte-LDI reading refuted)
0x24 50 10 00            3   NOP 6 + NOP 2 + NOP 1 slots                         HW
0x27 42 01 00            3   UNRESOLVED
0x2a d2 18 53            3   UNRESOLVED
0x2d ac ad               2   UNRESOLVED
0x2f 12 21 b4            3   UNRESOLVED
0x32 03                  1   UNRESOLVED
0x33 aa 4a 01            3   UNRESOLVED
0x36 00 10 21 03         4   UNRESOLVED (0x00/0x10 = NOP-shaped)
0x3a 81 80 12 05         4   CMU.CMII.I8 i8,i9                                   HW-class
0x3e 95 f1 01            3   UNRESOLVED
0x41 eb 50               2   UNRESOLVED (marker-shaped `eb` + NOP 6)
0x43 ea 42 10            3   UNRESOLVED
0x46 00 e0 9c 81         4   UNRESOLVED
0x4a c2 52 4e fb ff      5   LSU0.LDO.32 i5,i7,0x7ffd  (imm field large/-ve: role    MED
                                 unresolved; dst=i5/base=i7 fields decode cleanly)
0x4f 50                  1   compressed NOP 6 slots                              HW
0x50 41 50 00            3   CMU.CMZ.I32 i5          (rank==0?)                  HW-class
0x53 b5 64 00 05         4   BRU.BRA flag i5, +100 -> 0xb7                       HW SEQ
0x57 1b 40               2   branch marker + NOP 5 slots                         HW
0x59 81 9c 0c 08         4   CMU.CP.32 i6,i9                                     HW-class
0x5d 3c 06 25 03         4   SAU.DIV.U32 i9,i8,i6                                HW-class
0x61 c0                  1   compressed NOP 13 slots                             HW
0x62 81 50 12 00         4   CMU.CMII.U32 i5,i9                                  HW-class
0x66 b5 51 00 05         4   BRU.BRA flag i5, +81 -> 0xb7                        HW SEQ
0x6a 2b 40               2   branch marker + NOP 5 slots                         HW
0x6c c6 43 fd ff         4   IAU.INCS i10, -1                                    HW-class
0x70 41 a0 00            3   CMU.CMZ.I32 i5                                      HW-class
0x73 b5 d7 ff 09         4   BRU.BRA flag i9, -41 -> 0x4a  (loop back-edge)      HW SEQ
0x77 eb 30               2   branch marker + NOP 4 slots                         HW
0x79 bc e0               2   UNRESOLVED
0x7b 1c 12 a4            3   SAU.RCP ... (provisional)                           MED
0x7e 24 03               2   UNRESOLVED
0x80 07                  1   BRU.NOP 1 slot                                      HW
0x81 41 90 00            3   CMU.CMZ.I32 i9                                      HW-class
0x84 b5 33 00 01         4   BRU.BRA flag i1, +51 -> 0xb7  (!dims -> return)     HW SEQ
0x88 1b 50               2   branch marker + NOP 6 slots                         HW
0x8a e9 9d 24            3   UNRESOLVED (same 3-byte motif as vendor corpus `e9 ?? 24`)
0x8d 01 82 64 51 00      5   LSU0.LDO.32 i8,i18,0x28 (out ptr; class 0x01)       see 10
0x92 30                  1   compressed NOP 4 slots                              HW
0x93 86 c4 a8 02         4   IAU.LMULL i10,i6,i5                                 HW-class
0x97 07                  1   BRU.NOP 1 slot                                      HW
0x98 42 70 12 00         4   LSU0.LD.32.F16.F32 i7,i9   (fp16 load + convert)    HW-class
0x9c 10                  1   compressed NOP 2 slots                              HW
0x9d c6 43 fd ff         4   IAU.INCS i10, -1                                    HW-class
0xa1 41 a0 00            3   CMU.CMZ.I32 i5                                      HW-class
0xa4 b5 f4 ff 0e         4   BRU.BRA flag i14, -12 -> 0x98 (fp loop back-edge)   HW SEQ
0xa8 eb                  1   branch marker                                       HW
0xa9 07                  1   BRU.NOP 1 slot  (delay slot 2/6)                    HW
0xaa 3c e0 9c 30         4   SAU.ADD.F32 i7,i7,i1                                HW-class
0xae 10                  1   compressed NOP 2 slots                              HW
0xaf 49 74 10 08         4   LSU1.STI.16.F32.F16 i7,i8                           HW (byte-exact)
0xb3 c6 20 25 81         4   IAU.ADD i9,i9,2   (in-ptr += 2 bytes)               HW-class
0xb7 47 f1 01            3   BRU.JMP i30                                         HW
0xba 50                  1   compressed NOP 6 slots                              HW
0xbb 00 00 00 00 00      5   5 x NOP 1 slot (padding)                            HW
```

Coverage: the compressed-NOP rule (§8) + the branch markers (§9) account for **every** byte
in the previously undecoded runs at 0x0a, 0x23/0x25/0x26, 0x4f, 0x57, 0x61, 0x6a, 0x77,
0x88, 0x92, 0x9c, 0xa8/0xa9, 0xae, 0xba, 0xbb-0xbf - so the "odd single bytes" of §4.1 are
**not** cache/flush ops (the C12 guess) but the assembler's NOP-compression output. `0x40` at
0x0a is *exactly* a 5-slot NOP: a flush/flush-LSU op would not make a 5-slot fill FAIL while a
6-slot fill PASSES, whereas a counted NOP run does.
Still open: 0x0b-0x49 (the `rank>15`/`!dims`/dims-loop prologue) and 0x79-0x8c fragments.

Constraint gathered for that prologue [TBL]: the `0x42`-byte0 LSU family has **no 24-bit form**
in the extracted CV3600xx record set - the length histogram over every def whose low 8 bits are
0x42 is 28 bits (588 shapes) and 32 bits (280 shapes) only. Gold's 3-byte-looking `42 01 00` /
`42 0f 00` are therefore either 28-/32-bit instructions that continue past the third byte (the
stream is bit-packed) or different opcodes that merely share byte0 (`0x442`, `0xf42`, `0x1342`
all begin with 0x42; 0xf42 is the `LSU0.SWZMC/SWZMV` swizzle family). The 28-bit shape that
reproduces gold's verified fp16 load is
`LSU0.LD.32.F16.F32 [('D',0,1),('D',0,5),('IR',1,5),('IW',0,5),('D',66,12)]`
(= bits 0-11 opcode 0x42, 12-16 dst, 17-21 base, 22-26 fixed 0, 27 fixed 0 -> 3.5 bytes; gold's
`42 70 12 00` = dst i7, base i9 with a zero 4th byte).
`BRU.BRA` field evidence: target = `branch_addr + signed16(bytes[1:2])` (5/5 gold branches
exact), `byte3` = flag register (gold 5,5,9,1,14 == the CMU destinations); our assembler
emits the same form (verified by `br/t_*`, `br/x_true`, `br/x_false`).

## 12. Packet / pipe recipe for our assembler + backend (measured)

```
legal issue group = 1..3 ops, <=128 bits total, <=1 op per unit, no two LSU ops of the same
                    unit (LSU0/LSU1) in one window [R2/R5]
conditional branch:
    BRU.BRA <flag>, tgt      ; 4 B: b5 disp16_lo disp16_hi (flag&0x1f); target = own addr+disp
    <marker> <fill>          ; REQUIRED, else the device faults
        marker = 0x1b (forward) / 0xeb (backward)   ; any of 0b/2b/4b/8b/cb/eb also worked
        fill   = compressed NOP of (6 - k) slots    ; k = real instructions you want executed
                                                    ; inside the 6-slot delay window
                                                    ; k=0 -> 0x50 (branch immediate)
                                                    ; k=1 -> 0x40, k=3 -> 0x20, k=5 -> 0x00
other delay/latency gaps: ONE compressed-NOP byte ((n-1)<<4): 6 slots -> 0x50, 12 -> 0xc0,
                          16 -> 0xf0 (repeat 0xf0 for longer runs)
loads -> LSU0, stores -> LSU1; >=6 slots between an LSU writer and its first consumer
never emit the 3-byte `(reg<<3|2) lo16` LDIL; use `82 (reg<<3|2) lo16` / `82 (reg<<3|3) hi16`
```

Caution (measured): mixing a compressed NOP with several `07` NOPs is *not* reliably additive
in the load-latency gate (`20 07 07` = 5 slots faulted 3/3 while `40 07` = 6 passed 3/3). Use
one byte with the exact count, or a count clearly above the requirement.


## 13. Addendum — toolchain bug found while measuring §8-§12 (affects correctness, not the packet model)

`shave_asm.py obj` (the path our compiler uses) emits different bytes than
`shave_asm.py asm --hex` for the **AU-immediate** form: bit 31 of the 32-bit instruction word
(byte3 bit7, part of the vendor opcode mask `0x80000000|0xc6`) is dropped.

```
source            asm --hex  obj (.text)
IAU.ADD i9,i9,2   c6202581   c6202501
IAU.ADD i1,i1,1   c6208480   c6208400
IAU.ADD i1,i1,4   c6200482   c6200402
```
A 13-instruction mixed file shows *only* this form diverging between the two paths. `c6202581`
is byte-identical to gold's own `IAU.ADD i9,i9,2` (gold `.text` 0xb3), and hand-writing the
correct bytes on the NPU gives exactly +2 / +8 for imm 2 / 8 (`probes/br/g9_*.s`), so the field
map is right and the writer is wrong. Consequence: any kernel lowered to an AU immediate
(pointer/counter bump) executes a *different* instruction on silicon. Reported to the
assembler/encoder owners; interim rule: use the register form (`IAU.ADD rd,rs,rt`), which is
measured correct. Evidence table: `VERIFIED_EXAMPLES.md` Example K.4 + `probes/ROUND2_LOG.txt`.


### 12.1 Effective descriptor in our toolchain (cross-agent state, 2026-09-28)

The backend emits `.byte 0x1b, 0x40` (forward) / `2b 40` (backward) and the assembler's
backstop does not override an existing descriptor, so our *effective* branch semantics today is
**k = 1**: the first instruction after the descriptor executes whether or not the branch is
taken. The assembler's own default (`1b 50`, k = 0, truly immediate) applies only when the
source has no descriptor. If the lowering does not intend "one delay instruction always
executes", switch the emitted pair to `1b 50`.


## 14. Addendum 2 — LSU destination/immediate traps are PATTERN-dependent (register x immediate x form)

Context: a sibling proposed "r16..r31 alias onto r0..r15" for the split 5-bit register field.
That is refuted (a 5-byte `LSU0.LDO.32` into i25 demonstrably delivers the loaded pointer), but
the investigation exposed a *real* and reproducible trap class. **Correction to an earlier
version of this section: the trap is NOT a fixed set of register numbers — it depends on the
instruction form AND the immediate value.** All rows below: hand-written bytes, clean skeleton
(out pointer loaded into i0 by the verified `c2 02 64 51 00`, 6-slot compressed `50` fills,
fixed safe store `89 09 00 01`), 2/2 runs, deterministic.

| probe | form | immediate | result |
|---|---|---|---|
| `li{R}.s`, R = 19, 21, 23, 24, 30 | `82 (R<<3|2) ef be` + `82 (R<<3|3) ad de` (LDIL/LDIH pair) | 0xBEEF / 0xDEAD | **FAULT** |
| `li{R}.s`, R = 16,17,18,20,22,25,26,27,28,29,31 | same | same | ok |
| `lim{R}.s`, R = 19, 30 | same pair | 0x0001 / 0x0000 | **FAULT** |
| `lim{R}.s`, R = 21, 23, 24 | same pair | 0x0001 / 0x0000 | **ok** |
| `ld{R}.s` 5-byte `c2 <b1> 0x65 0x01 0x00` (`LSU0.LDO.32 iR,i18,0x0`) | LDO.32 | 0x0 | FAULT for R = 19, 21, 24, 30; ok for 16,17,18,20,22,23,25..29,31 |
| `p25.s` 5-byte LDO into i25 then `LSU1.ST.32 i25,i5` | LDO.32 + ST.32 | 0x0 / 0x28 | **correct** (`40 43 01 2e` = in pointer) — bit 16 of the dst field IS honoured |
| `ldw{R}.s` `82 ((R&15)<<4|5) ((18<<1)|(R>>4)) 01` (`LSU0.LD.32 iR,i18`) | LD.32 (2-operand) | – | no trap for any R in 16..31 |
| `ldop{R}.s` LD.32 + `LSU1.ST.32 iR,i5` | LD.32 + ST | – | R=21 traps; R=25 executes but lands nothing |

Consequences:
* The register field really is 5 bits and the 5th bit (byte2 bit0) is honoured, so no aliasing.
* Legality is a *pattern* property of the whole encoded word (register field + immediate bits):
  the same register traps with one immediate and is fine with another (i21/i23/i24 with
  0xBEEF/0xDEAD vs with 0x0001/0x0000), which is the same phenomenon reported independently for
  the LDIH immediate sweep (Example J(b): bit 15 plus lower bits). **Therefore the vendor table's
  field map is necessary but not sufficient: every (form, register, immediate) combination must
  be effect-checked on silicon** — including the combinations the backend will actually emit.
* Safe working defaults measured so far: destinations r0..r15 in LSU forms; `LDIL/LDIH` pairs
  with small immediates; the 5-byte `LSU0.LDO.32` form; the 2-operand `LD.32` only for dst < i16.
* Relayed to AsmSchedImpl (its D6 warning currently names {19,21,24,30} as a fixed fault set -
  that is the imm-0x0 LDO row only and should be widened to "pattern-dependent; verify per
  form/register/immediate") and to MedicalCrayfish.
* Probes: `probes/br/{li*,lim*,ld*,ldw*,ldop*,p21,p25,p5}.s`.


### 14.1 Store value fields with the 5th bit set — measured-good above r15

Full-skeleton probes (out pointer in i5, 6-slot fills, store `LSU1.ST.32 iD,i5` with byte1 =
(D&15)<<4|9 and byte2 = (5<<1)|(D>>4), i.e. bit16 = 1 for D >= 16), value register loaded by the
5-byte `LSU0.LDO.32`:
* `probes/br/p25.s` (D = 25) -> `40 43 01 2e` = 0x2E014340 = the in pointer.
* `probes/br/rd{16,17,20,22,26,31}.s` -> `40 43 01 2e` as well.
=> store **value** fields for i16, i17, i20, i22, i25, i26, i31 are legal; the rd21/rd24/rd30
rows fault but are fully confounded with the load-destination trap for the same register (they
share the same 5-bit field), so they say nothing about the store side. Store **base** fields are
a contiguous 5-bit field (bits 17-21) and the vendor anchor `ST i10,i29` = `42 a4 3a 00` uses a
high base. Recommended allocator rule (conservative, covers both directions): keep LSU register
operands in r0..r15 unless the exact (form, register, immediate) combination has been
effect-verified on silicon.

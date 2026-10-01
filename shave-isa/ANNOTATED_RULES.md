# ANNOTATED_RULES — vendor assembler packet/delay/NOP model (static RE, no wine)

Source: r2gvp04w-re/re_decompiled/moviAsm64/decompiled_all.c (Ghidra decompile).
Function names below are Ghidra FUN_ ids; annotations are mine. String evidence:
re_strings/moviAsm64.ascii.txt (switches 2419-2428, help 2755-2757, diagnostic
2492) + moviCompile64.ascii.txt (shave-load-latency=6, shave-branch-delays=6).

## State object (AsmCtx) field map (derived from usage)

| offset | meaning | evidence |
|---|---|---|
| +0x128 -> +0x50 | global LATENCY = 6 (shave-load-latency) | fn805/817 read as window/max |
| +0x1a0 | DELAY-SLOT COUNT (6; -forceDelaySlots:<n>) | fn812 `iVar3 = +0x1a0` loop counter; fn805 `+0x1a0 - 1` subtract |
| +0x1a8 bit2 | forceDelaySlots-present flag | fn805 `>>2 & 1` gates delay subtraction |
| +0x1a8 bits5,6,8 (0x160) | NOP-compression / final-slot / slot-promo options | fn805 `& 0x160` chooses counter source |
| +0x1c8 | delay-slots-remaining counter (decremented per fill) | fn805 entry `*p - 1` clamp |
| +0x1cc | latency-cycles-remaining (pending load side-effects) | fn805 entry + warn `+0x1cc < required` |
| +0x160..+0x168 | range over instruction records, stride 0x108 (264B) | fn805/812 loops |
| rec+0xa0 & 0x1e0 | PIPE/UNIT id field (bits5-9); bit1 = "has-delay/branch" gate | fn805 if/else; fn812 `>>1 & 1` |
| rec+0xa8 | promotion-eligibility flag (0 => candidate) | fn812 promote condition |
| rec+0x28, rec+0x38.. | encoding payload + locref | fn812 copy |
| +0x52 (AsmCtx) | BIT offset within current output byte (mod 8) | fn851 `& 7` shift-or packing |
| +0x218 -> +0x80..+0x88 | per-platform SLOT/PIPE descriptor table (stride 0x48=72B) | fn812 iterate + FUN_1800b55c0 |
| +0x1d8.. | pending slot buffer (promoted instrs) | fn812 append words |

## Annotated functions

### FUN_18005cf20 (fn805, 837B) — LATENCY CHECK after delay fill
- Decrements delay-slot counters (+0x1c8/+0x1cc).
- Scans upcoming records: `u14 = rec+0xa0 & 0x1e0 ? table[ctx+0x128]+0x50
  : FUN_1800a4e80(rec)` — required latency = per-record lookup (pipe-indexed
  table) else GLOBAL 6.  Accumulates MAX required.
- If `(cyclesRemaining < required)` or `(0x160-flags & globalLatency < max)`
  with records pending => emit `WARN_SIDE_EFFECTS_AFTER_DELAYSLOTS
  ("There are side effects after delay slots, remaining latency cycles: %d")`.
- Subtracts configured delay count (+0x1a0) from the pending latency when
  forceDelaySlots flag absent.  => **assembler ACCOUNTS but only WARNS:
  load-latency correctness is the compiler's scheduling job; it can be
  satisfied by inserting instructions (NOPs) — RoughFinch's 6x BRU.NOP test.**

### FUN_18005e7c0 (fn812, 1985B) — DELAY-SLOT FILL / SLOT PROMOTION
- Entry gate: first record `+0xa0 & 2 == 0` (branch/needs-delay record).
- Saves N = ctx+0x1a0 (=6), runs fn805 check, snapshots record encodings
  (payload = 0x28 = 40 bits => **instructions are <=40 bits / 5 bytes**).
- `while (N-- > 0 && progress)`: for each pending record with `+0xa8 == 0`:
  eligibility = FUN_1800a1e90 / FUN_1800b10d0; if eligible => PROMOTE into
  slot: fetch encoding FUN_1800a4ef0, run it through per-platform slot
  descriptors (0x48-stride entries, FUN_1800b55c0 = per-pipe NOP/validate),
  append to pending-slot buffer (+0x1d8).  Otherwise slot stays NOP.
  => **after every branch: the next N slots are filled by promoting real
  instructions (SLOT_ALL promotion) or NOPs; -noSlotAllPromo / -noCompressedNOPs
  change this.**

### FUN_18005f8e0 (fn817, 1185B) — LABEL-WINDOW SIZE ACCOUNTING
- Window size = global latency (6): keeps `u32[10]` size accumulator array
  (+0xf0..), index +0x118, window counter +0xec (reset from +0xac).
- Called per instruction after a label (fn851 tail):
  `bits = (*(u32*)(ctx+3) + ((x+1)/2)) * 4` — instruction size in bits
  (halfword-rounded).
- Accumulates total bits; **thresholds 0x80 = 128 bits**:
  - `WARN_DELAYSLOT_CYCLE_INSTR_AVG` when window average overflows 128b,
  - `WARN_BIG_INSTRUCTION_AFTER_LABEL` when first instr + partial > 128b.
  => **a PACKET <= 128 bits (16 bytes); label-window packing must not
  overshoot 128b** (consistent with gold packets: 5+5+1=11B, 3x3B=9B,
  5+3+4=12B, 5+5+5=15B — all <=16B).

### FUN_180069ab0 (fn851, 1170B) — BIT-STREAM PACKER (RAW address entries)
- Writes encoded fields at **BIT granularity**: `byte |= v << (bitpos&7);
  nextbyte |= v >> (8-(bitop&7))`; nibble-merge path when a partial byte
  (< 0x10) can absorb a 4-bit value (`b |= count<<4; next |= count>>4`) —
  this is where **compressed-NOP / small-count fields get packed into holes**
  (e.g. gold's `2b 40`, `1b 50`-style runs are aligned bit-fields, not
  standalone byte opcodes).
- Tracks output offset (+0x68), max (+0xe8), emits RAW_ADDRESS_ENTRY logs.

### Helpers located (sizes)
- FUN_18003cbd0 (163B): WARN_SIDE_EFFECTS formatter (args: ctx, remaining).
- FUN_18007a4e0 (93B): 'Invalid NOP syntax' + NOP SYNC text builder.
- FUN_18007c4a0 (103B): 'Invalid NOP in pipe' error.
- CLI parse: lines 16264-16450: -noSlotAllPromo, -forceDelaySlots:<n>,
  -noCompressedNOPs, **-forceLoadDelay:<n>** (extra switch not in help),
  -noFinalSlotCompression.

## RULES (implementable)

R1. Instruction encodings <= 40 bits (5B). Observed sizes 8/24/32/40 bits.
R2. Output is a BIT stream: instructions are bit-packed; byte emission may
    straddle bytes (offset mod 8). NOP/count fields may merge into partial
    bytes (nibble path).
R3. PACKET <= 128 bits (16 bytes). (Empirical: every gold window <=15B;
    fn817 thresholds = 0x80; label windows of 6 instrs must stay <=128b.)
R4. After a branch: fill up to N=6 delay slots (-forceDelaySlots) by
    promoting subsequent eligible instructions into the branch packet;
    unpromotable slots become NOPs (BRU.NOP = 0x07; compressed variants pack
    into partial bytes).
R5. Load latency = 6 cycles: the ASSEMBLER only verifies (warns via
    side-effects diagnostic); the COMPILER must schedule.  In OUR pipeline:
    insert 6 issue-slots (e.g. 6x BRU.NOP, or non-dependent work) between a
    load and its consumer — RoughFinch's sweep pins the exact distance.
R6. LSU policy defaults (moviCompile): prefer-lsu0 for loads, prefer-lsu1
    for stores; flush-on-enter inserts FLUSH_LSU0/FLUSH_LSU1 at function
    entry; NOP SYNC after inline-asm schedules.
R7. Label windows: after every label, the first 6 instructions are size-
    accounted; >128b average => warning (packet boundary must be reset at
    labels — explains Main's LDIL-shift experiments: alignment at labels
    decides packet grouping; SLOT_ALL promotion for label alignment).

## OPEN (need BiologicalMockingbird Ghidra side / fn812 deeper pass)

- Exact descriptor byte0 meaning for LSU ops ({01,c2,f9,fa,f0}) = pipe/
  promotion class chosen via the 0x48-stride slot-descriptor table
  (+0x218+0x80).  Empirical (Main): 01 works standalone, c2 wrong-values,
  f9/fa fail => byte0 = slot/pipe placement, not unit.
- Compressed-NOP value encoding (nibble counts: gold bytes 0x10/0x30/0x40/
  0x50/0x03/0x2b/0xc0 and pairs `1b 40`, `2b 40`, runs `1b 50 e9 9d 24`).
- Packet close/descriptor write (which function terminates <=128b window).

## Gold packet-alignment reference (best-guess boundaries, sec14-20 table)

```
0x00-0x09: [LDO 5B][LDO 5B]                      (10B)  then 0x0a NOP'40'
0x0b-0x10: [LDIL 3B][LDIL 3B]                    (6B)   0x11-0x12 hole'00 00'
0x13-0x1d: [LDIL 3][hole 2][LDIL 3][LDIL 3]      (11B)
0x1e-0x26: [3B subop3][3B subop7][3B subop0]     (9B)   = one packet
0x27-0x2c: [LDIL 3][LDIL 3] + 0x2d-0x2e 2B hole
0x3a..:    [CMII 4][5B unk][LDIL 3] ...
0x88-0x96: [unk 5B | LDO 5B | '30'+LMULL 5B]     (15B)  = Main's 3-slot pipe
0xba-0xbf: 6B data padding after BRU.JMP (not executed)
```

## EMPIRICAL CALIBRATION (RoughFinch oracle sweeps, 2x each — 2026-09-28)

C1. **Load latency = 6 INSTRUCTIONS (slots), not packets**: `LDO i0,i18,0x28`
    + K fillers + `LSU1.ST.32 i0,i0`: K=4 FAIL, K=5 FAIL, K=6/7/12 PASS
    (filler opcode irrelevant: BRU.NOP/IAU.ADD/CMU.CMZ all count; six 1-byte
    NOPs inside one 16-byte packet suffice).  => `--latency 6` default = R5.
C2. Packet/byte alignment does NOT gate loads (1-4 byte pre-shifts pass 2/2);
    the 3-B LDIL-before-load fault is the known LDIL-in-minimal-kernel
    artifact, not alignment.  (Gold's own 0x88-0x96 3-slot pipe transplanted
    FAULTS even with 6 fillers => that pipe is context-dependent; dropped as
    a template.)
C3. **Branch delay = exactly 6 INSTRUCTIONS EXECUTE after a taken branch**:
    `BRU.BRA Lend` + 8 storing-pairs -> stores of pairs 1..3 land (6th
    instruction = last executed), i.e. fall-through continues for 6 issues
    then redirect.  Matches shave-branch-delays=6 + fn812's +0x1a0 loop.
C4. **FLAG SEMANTICS (R8)**: CMU compares (CMII/CMZ/...) do NOT write the
    named GPR — the operand names a FLAG register consumed by
    `BRU.BRA <flag>, target`.  Proof: `CMU.CMII.U32 i1,i2(14)` + 6 fillers
    then `ST.32 i1,out` reads back the ORIGINAL i1 (04 00 00 00, 3/3);
    `CMU.CMZ.I32 i1` likewise leaves i1 unchanged.  Gold chain:
    `CMU.CMZ.I32 i5` @0x50 -> `BRU.BRA i5, 0xb7` @0x53 (distance 1) is
    flag->branch, GPR untouched.  **Any lowering of
    `if (cond) ...` that materializes cond into a GPR (e.g. CMU -> XOR on the
    operand reg -> BRA) reads a stale GPR and decides arbitrarily** — this is
    PleasedDog's kernel guard bug.  Encoding unchanged (same bytes); only the
    semantic annotation matters: CMU/CMZ "reg" = flag name.
C5. Runtime probe values (oracle): rank=4, params+0x10 = 0x80000050 (dims
    VALUES at params+0x50), out = 0x2E014000 / 0x2E014010 per invocation.

### Gate: LOAD-VISIBILITY (PASSED, oracle-verified)
Kernel (load_vis.s in this dir):
    LSU0.LDO.32 i0, i18, 0x28 ; fmt=0
    BRU.NOP x6                (or `shave_asm asm load_vis.s --latency 6`)
    LSU1.ST.32 i0, i0
RoughFinch: K=6 PASS 2x (ACTUAL[0:4] = 00 40 01 2e), K=4/5 FAIL => our
assembler emits byte-exact working sequences; gate = keep `--latency >= 6`
for load->use distance (vendor default 6).

C6. Unit rule (HW): never reuse the same LSU unit for load+store in one
    stream (vendor diagnostic "LSU0/LSU1 unit is used more than once in the
    same pipe"); LSU0.LDO + LSU0.ST => garbage, LSU1.ST correct (gold:
    LSU1.STI.16.F32.F16).  Vendor defaults: prefer-lsu0 loads / prefer-lsu1
    stores.
C7. LDI forms (HW): constants build ONLY from 4-byte forms
    `82|(reg<<3|2)|lo16` (LSU0.LDIL), `82|(reg<<3|3)|hi16` (LSU0.LDIH),
    `89|...|lo16` (LSU1.LDIL), `a9|(reg<<3|3)|hi16` (LSU1.LDIH);
    pair adjacent or >=2 slots apart; the 3-byte corpus patterns are
    mis-segmented payloads (silicon: no write / 0 / trap).  Encoded pins in
    cal_fix (encode_priority -1 on the 4B forms, 4 on all 3B).

C8. Vendor driver argv (InternationalUrial, moviCompile64 decompiled_all.c
    :3243041-49): the assembler job is `moviAsm --noSPrefixing --cv <cpu>
    [-Wa,...] in out` (cpu default literal "ma2150"; `--cv` value is a
    SEPARATE token).  No -a / -noCompressedNOPs / -force* are ever passed =>
    gold objects used assembler DEFAULTS: NOP compression ON, SLOT_ALL
    promotion ON.  `-no6thSlotCompression` occurs NOWHERE in either vendor
    DLL (stale upstream-clang Myriad.cpp switch) — accepting it as a no-op
    is correct; do not count it for parity.  Our parity risk is therefore the
    OPPOSITE: we do not implement NOP compression/SLOT_ALL yet (explicit
    bundles + BRU.NOP only) — matches bytes when no compression opportunity
    exists, may diverge where gold compressed delay-slot NOPs.
    Also: `-L` (label align 128b) == assembler twin of compiler's
    `shave-disable-slot-optimiser` (which has no reader in moviCompile —
    dead option); vendor driver `SHAVE::Assemble` recoverable from
    moviCompile64.dll if exact argv ever needed.

C9. Bundle text separator (InternationalUrial, moviCompile64 strings:88065):
    vendor asm-printer emits bundles as ONE line with literal
    `<instr>            || <instr>` = 12 spaces + `||` + 1 space between
    members (printed by FUN_180098ca0 for >=2-member bundles only; single
    member prints plain).  The canned abort snippet
    `BRU.SWIH 1 || LSU0.LDIL i18, 0xFFFF || ...  // 'abort'` uses single
    spaces but is reference text, not printer output.  Our tokenizer splits
    on `||` with strip() => whitespace-insensitive: byte output identical;
    for text-diff parity emit `            || ` between slots.

C10. Gold re-segmentation candidates (solver over runs; CV opcode-high set
    from our tables = 88 values; membership of load-bearing bytes:
    {0x40,0x03,0x2b,0x9c,0x00,0x01} IN, {0xc0,0x1b,0xeb,0x10,0x30,0x50,
    0xea} OUT).  Byte-aligned candidates for HW A/B (Mockingbird):
    - 0x0a-0x0d `40 0e 88 ac` LDI-class op0x40 subop6(HI) i1,0xac88 —
      byte1 satisfies (reg<<3)|{2,3,6,7}; t1 failure explained if placed as
      an orphan HI (needs a LO 0x82.. pair first)
    - 0x32-0x35 `03 aa 4a 01` LDI-class op0x03 subop2(LO) i21,0x014a
    - 0x40-0x43 `01 eb 50 ea` LDI-class op0x01 subop3(HI) i29,0xea50
    - 0x36-0x39 `00 10 21 03` generic 4B (fails (c) => not LDI)
    - 0x6a-0x6d `2b 40 c6 43` generic 4B (op in CV set, fails (c))
    - 0x85-0x88 `33 00 01 1b` = VAU.ADD.I16 fmt0 (mask match!) consumes
      load-bearing 0x88-`1b` as payload
    - 0x99-0x9c `70 12 00 10` = LSU1.LDD64.H fmt0 (mask18) consumes 0x9c-`10`
    Resistant (no byte-aligned candidate; fn851 bit-packed-start likely):
    0x57, 0x61, 0x77-0x7a, 0xa8, 0xae, 0x89-0x8c, 0x1e-0x21, 0x2d-0x2e.
    LDI span-scan: no 82/89/a9 LDI pattern spans any load-bearing byte.

C11. Silicon verdicts on re-seg candidates (Mockingbird Example H, proven
    probe placement: LDO i20 + 6 fills + <cand> + 6 fills + LSU1.ST + JMP):
    CTL2 `82 0a 08 32`+`82 0b ac 88` -> 0xac883208 PASS (LO/HI placement and
    0x82 class confirmed; subop7 is NOT the HI class).  REFUTED on silicon:
    C1a `40 0e 88 ac` alone, C1p paired-HI, C2 `03 aa 4a 01` (writes 0),
    C3p `01 eb 50 ea` paired-HI => the 4-byte LDI-class reading of gold's
    runs is DEAD (caveat: slot/pipe-dependent variants would also fail).
    chain.s expected string corrected (their doc typo): `... c6208800 ...
    89290001 47f101` (store LSU1.ST.32 i2,i0 = 89 29 00 01) — all
    cross-validation rows now match.
C12. Opcode-high -> CV-table forms (for silicon layout tests): 0x00 = 24
    control ops (per-unit CLOCK_ON/OFF, CFLUSH_ALL/CFLUSH_INV_ALL,
    CINVAL_ALL, BYPASS_ON/OFF — flush-on-enter family lives here); 0x01 =
    CPFL1 (5B) + LSU0.LDO.32 fmt110 (5B, unit nibble 2 rejects byte1=0xeb);
    0x03 = CPFL2 (5B); 0x2b = SWZMV.128.64 (4B)/SWZMV.256.64 (5B) swizzles;
    0x40 = CMU.FLUSHB (3B, 401404 for i1) + LSU0.ST.16.F32.F16 fmt0
    (40180400 for i1,i2) + LSU0.STI.16.F32.F16 fmt2 (4018040c);
    0x9c = SWZMV.256.128 (9cf40000)/SWZMV.512.128 (9cf8000000).
    Instantiations: CPFL1 0,0 -> 01c2070000; CPFL2 -> 03c2070000;
    CLOCK_ON -> 0028103a; CFLUSH_ALL -> 0020103a; SWZMV.128.64 -> 2bf40000.

C13. Bit-level candidate batch (for Mockingbird ±1-bit perturbation scans;
    generator: bit-sliced windows -> byte reinterpret -> mask/LDI/OP0 score,
    independent of byte alignment per fn851 packing):
    STRONG (mask>=10 hits on the bit-sliced bytes):
    - 0x89-0x8c run: bitoff 1122 (byte 0x8c +2b), len 32, sliced=49802059,
      enc8=1b50e99d2401826451 -> MASK18 LSU1.LDI.32.I8.I16 fmt3   [LOAD-family: top pick]
    - 0x77-0x7a run: bitoff 975 (0x79 +7b), len 40, sliced=c139244849,
      enc8=ff09eb30bce01c12a4 -> MASK16 CMU.CEX.U16 fmt1
    - 0x77-0x7a run: bitoff 974 (0x79 +6b), len 32, sliced=82734890 -> MASK11 LSU0.LDIH fmt2
    GRIDS (no byte- or bit-aligned candidate found; pure perturbation grids):
    - 0xa8: bitoffs 1344..1351, len 32/40, enc8=b5f4ff0eeb073ce09c30
    - 0xae: bitoffs 1392..1399, len 32/40, enc8=e09c301049741008c6
    WEAK (OP0-only starts, listed for completeness): 0x57 @698/699/700 len32
    enc8=b56400051b40819c0c; 0x61 @777/778/780 enc8=3c062503c081501200;
    0x1e-0x21 @242/243/245 enc8=00320100a3fcff0f28; 0x2d-0x2e @363/372/373
    enc8=00d21853acad1221b4.

C14. Silicon effect-matching protocol + hard IMM-BIT15 constraint (Mockingbird
    Example J): `82 73 imm16` sweep in the proven placement — executes for
    imm in {0000,0001,0048,0100,1000,1234,2000,4000,7fff,8000}; TRAPS when
    imm bit15=1 AND any lower bit set (8001,8048,9000,a000,ffff,...);
    fillers after do not rescue => in the ORPHAN-HI context the immediate's
    top bit behaves as control/variant, not data (paired LO+HI for such
    values is UNTESTED; z23's paired 0xffff ran => pairing may rescue).
    Y2 confirmed `82 73 12 34` writes i14 = 0x12340001 (dst = byte1>>3,
    imm16 = bytes2-3, HI half) => shape-correct + effect-verified.  Lesson:
    table shape NEVER proves executability — effect-read the destination.
    Encoder warning (routed to AsmSchedImpl + MedicalCrayfish): constants
    with imm16 having bit15 set and low bits nonzero need paired-context
    verification or a split build (LDIL+OR/shift); do not assume a single
    LDI emits any 16-bit value.
    Bit-grid verdicts: 0x77-0x7a and 0xa8/0xae have NO sub-byte offset
    yielding correct values (not self-contained instructions in context);
    B1/B2/B3 slices harmless-without-effect-read; E3 effect recipe below.
C15. Effect-match candidates queued for Mockingbird:
    E1 pairing-vs-orphan: `82 0a 08 32` (LO i1,0x3208) + `82 0b 90 00`
      (HI i1, imm 0x9000 - trap-class) -> readback i1 distinguishes
      orphan-HI-specific trap from intrinsic variant-bit.
    E1b control: same pair with imm 0xffff (z23-class, expected OK).
    E3 (bit-sliced candidate from 0x89-0x8c): `49 80 20 59` = mask-match
      LSU1.LDI.32.I8.I16 fmt3, ops (i8, i16) — emit + 6 fills, read back i8
      and i16 through the known-good store path; load semantics confirmed
      if either changes from a known seed.
    E4 (optional): CMU.CEX.U16 slice `c139244849` (5B) — dst extraction
      from fmt1 fields pending if wanted.

C16. Silicon-measured stream rules (MedicalCrayfish, 2-3x each; full tables
    llvm-14/BYTE_EXACT_LOG.md iteration 24):
    R1. CONDITIONAL BRU.BRA REQUIRES a direction-dependent delay descriptor
        IMMEDIATELY after it: forward = bytes `1b 40`, backward = `2b 40`.
        Wrong descriptor => silent fall-through; NO descriptor => branch
        never transfers.  With the descriptor the transfer is IMMEDIATE
        (the generic 6-slot branch delay does NOT apply to conditional BRA
        in this encoding).
        Gold cross-map (descriptor variants beyond fwd/bwd exist — open):
          BRA@0x53 ->0xb7 (fwd) desc `1b 40`      [matches R1 fwd]
          BRA@0x66 ->0xb7 (fwd) desc `2b 40`      [anomaly: fwd with bwd desc]
          BRA@0x73 ->0x4a (back) desc `eb 30 bc e0`
          BRA@0x84 ->0xb7 (fwd) desc `1b 50 e9 9d 24`
          BRA@0xa4 ->0x98 (back) desc `eb`
        => direction is necessary but not sufficient; condition/pipe may
        select among descriptor variants (EB/2B/1B families).  Encoder must
        emit raw descriptor bytes; variants for A/B pending.
    R2. FLAG predicates: CMZ(X)+BRA taken iff X != 0; CMII(a,b)+BRA taken
        iff a != b (no ordered compare in family).  Flag persists across
        intervening instructions, but REWRITING the flag-named GPR between
        CMU and BRA CLEARS the flag (aliasing nuance over C4).
    R3. LDIL+LDIH followed within <6 slots by ANY LSU op FAULTS => after a
        constant-materialization pair, keep >=6 slots before the next LSU
        instruction (in addition to load->use >=6).
    R4. SAU.DIV.U32 result unusable for ~12 slots: k<=10 stale, k=11/12
        fault, k>=13 correct => emit >=13 slots after DIV before consuming.
    R5. Their full-kernel symptom flip (lat=6 faults, lat=8/12 execute;
        branch-delays=0 faults, 6 executes) is consistent with R3+R1: the
        NOP count after a load cannot pay off an LDIL->LSU violation that
        sits EARLIER in the stream; minimal probes (RoughFinch K=6) have no
        LDI-pair so they pass at 6.

C14 RETRACTED (Mockingbird, Example J(b)): the "imm bit15 control bit" trap
    was a PROBE-SKELETON artifact.  Plain-context tests:
    `82 73 cd ab` -> i14 = 0xABCD0001 EXECUTES; `82 73 48 90` -> 0x90480001;
    `82 73 00 80` -> 0x80010001; T1 (sweep minus extra i21 setup) -> OK.
    LDIH immediates are plain 16-bit data; NO split-build path needed.
    C15 E1 VERDICT: paired context handles ANY imm16 with the a9-class:
    E1c LDIL i1,0x3208 + `a9 0b ff ff` -> i1 = 0xFFFF3208;
    E1d same + `a9 0b 00 90` -> i1 = 0x90003208.
    => COMPILER/ENCODER RULE: always emit LDIL(lo) + LSU1.LDIH(hi) pairs;
    the 82-class LDIH was flaky ONLY in their sweep skeleton - do not draw
    z23-style comparisons from that skeleton.  (Earlier warnings I routed
    to AsmSchedImpl/MedicalCrayfish are hereby retracted; the positive
    pairing rule stands.)
    E3 (0x89-0x8c slice `49 80 20 59` as LSU1.LDI.32.I8.I16, seeded i8/i16):
    execution FAILED => fmt3-load reading NOT confirmed.  5B variant slice
    for retest: `49 80 20 59 14` (bitoff 1122, len 40).
    E4 GO: `c1 39 24 48 49` = CMU.CEX.U16 fmt1 (mask 0xe00001fff, fixed
    0x8000019c1, MATCH) ops = (i20, i9, i1, i16) @fields 28/18/13/23 -
    seed+read all four.
    Method note (their words): pass/fail is weak; effect-read of a seeded
    destination is what produced every positive identification.

C17. IAU.ADD-immediate: RESOLVED (VendorRE2 silicon, 2/2) — field map was
    always correct; the bug was OUR obj PATH.
    - `asm` path bytes correct: c6202581 (i9,i9,2) == gold; c6208480 (+1);
      c6200482 (+4).
    - `obj` path emitted c6202501 / c6208400 / c6200402 — **bit31 of the
      word (fixed 0x80000000 opcode bit of mask 0x80001fff) dropped**; all
      other instruction classes identical between paths in a 13-instr mix.
      => every AU-immediate compiled through obj was a DIFFERENT opcode on
      silicon (the earlier "chaotic addend table" = mangled-bytes signature;
      the imm&3 model is RETRACTED).
    - Correct-byte controls: c6202581 -> +2, c6202580 -> +0, c6202584 -> +8:
      addend = FULL field value; no mask, no scale, no reg interaction.
    - fmt1/fmt3 layout stands: dst@18(5), src@13(5), imm@23(8),
      fixed 0x800000c6, mask 0x80001fff (chits 27).
    - INTERIM RULES: obj-path AU-imm bytes UNTRUSTED until assemble_object
      preserves bit31 (owner: AsmSchedImpl); reg-form + asm-path unaffected;
      the "imm <= 3" restriction is CANCELLED.  G3 anchor (i9,i9,2) =
      c6202581 remains.  PROVISIONAL imm&3 warnings in gates/docs: remove.

C18. Delta cross-refs (InternationalUrial, delta/ANNOTATED_DELTA.md; R/D =
    delta-namespaced, C = this file, no renumbering):
    - R1 latency: vendor latency fn = max(per-operand itinerary,
      MFI+0x190=6), hard 6 for opcodes with no itinerary (FUN_180138bd0);
      LDI-family lands in the same 6 bucket UNLESS opcode in the
      short-latency set {0x8e0, 0x8f9, 0xa4e, 0xa63, 0xa64} (latency 1).
      => explains C16-R3 (LDI->LSU >=6) mechanically.
    - R2/C16-R2 flag model confirmed: flag = operand-named register
      (SHAVEISD::SETCC / CMU_CM produce, BRU.BRA consumes); any write to
      that GPR invalidates the flag (hazard; rationale for
      shave-enable-interrupt-friendly-scheduling + anti-dep breaker).
    - Delta ancestry D7: vendor has NO packetizer pass; pipe legality lives
      in moviAsm64 (8x unit-once-per-pipe strings, .64/.128 blocks, SLOT_ALL,
      NOP compression) => backend emits MI-bundles, assembler validates
      pipes; do NOT port Hexagon DFA packetizer.

C19. Register/field hazards measured (AsmSchedImpl + VendorRE2 +
     StrangeSolve rounds):
    - dst-field in 4-byte LDO/LD/AU shapes = bits[12,16] but silicon reads
      only bits12-15 => r16..r31 ALIAS r0..r15 on destination registers
      (LDO i21.. behaves as i5; ADD likewise).  LDI forms keep 5 bits
      together => high regs safe there.  Assembler warns dst>=i16 in those
      shapes; sources (i18 etc) unaffected.  (Consistent with my sec-15
      "dst observed 0..15" note — now confirmed as silicon behavior.)
    - obj-path defect (fixed by owner): assemble_object lacked the
      operand-kind filter + [iN+off] expansion => AU-imm selected the
      reg-only form and dropped bit31; obj==asm now gated (G9, 262 lines).
    - D3b: an LSU op 1..5 slots after a memory load silently discards that
      load (auto-padded with BRU fillers by the assembler).
    - D6: memory-load destinations > i15 warn; measured FAULT set
      {i19,i21,i24,i30} in the 5-byte LDO form.
    - C17-imm<=3 and C14 warnings removed from code (both refuted).
C20. SAU.ADD.F32 operand mystery (StrangeSole via owner): gold triple
     (i7,i7,i1) encodes 3ce09c30 and computes correctly; other triples
     e.g. (i6,i4,i5) -> 3c809832 have byte-exact fields under our map
     (dst@18..22, a@13..17, b = byte2[7] + byte3[0..3]; all three invert
     to the named regs for BOTH triples) yet hardware computes a + 0.70710678
     (a hardwired SAU constant) instead of a + b => byte3 low bits behave
     like a SOURCE-SELECT, not pure b-low bits: gold byte3=0x30 selects REG
     (works), theirs 0x32 selects the constant.  Discriminator experiments
     (propose to StrangeSole): (A) vary b only: (i1,i1,iN) for
     N=1,2,3,4,8 — which b values read as registers maps the selector field;
     (B) fix b, vary a with dst!=a; (C) dst==a (gold shape) with b sweep —
     if only dst==a works, it's a read-port bypass, not an encoding field.
     DO NOT re-pin fmt3 bytes until (A)-(C) return.
C21. 0x42-byte0 family padding audit (VendorRE2 28-bit finding + my scan):
     of 955 4-byte 0x42-class forms, 10 have mask EXCLUDING byte3
     (28-bit/pad class, byte3 emitted 0 by construction) and 945 include
     byte3 (32-bit class).  SUSPECT: LSU0.LD.8 fmt14/fmt29 =
     fixed byte3 = 0x0e with maxfieldbit = 25 => emit `42 5d 00 0e`
     (`LSU0.LD.8 i2,[i0]`) — if the LD.8 def is 28-bit (VendorRE2), byte3
     must be 0x00 and my mask/fixed carry record-padding bits (mask includes
     bits24-30, fixed sets 25-27).  Pending: record-width evidence for LD.8
     + harness A/B `425d000e` vs `425d0000`; fix (zero byte3 / clear
     mask>=28) = cal_fix owner.

C21 update: first LD.8 byte3 A/B (owner-run) INCONCLUSIVE by observable
    degeneracy — byte3=0x0e and byte3=0x00 both read back 00 00 00 00
    because the input buffer's first byte is 0x00 (load-delivered-0 ==
    load-not-delivered).  Required redo = NONZERO-WITNESS variant
    (routed to StrangeSole): seed 0xAB via proven path, copy base into a
    second register, candidate-load, store-back to a DIFFERENT offset:
      LDO out(i0); 6 NOPs; LDIL i2,0; ADD i1,i0,i2 (base copy);
      LDIL i5,0x00AB; LDIH i5,0; LSU1.ST.32 i5,i0 (seed AB at out+0);
      6 NOPs; ADD i0,i0,i6 with LDIL i6,4 (advance to out+4);
      42 5d 00 <byte3> (candidate, base i1); 6 NOPs;
      LSU1.ST.32 i2, i0 (readback at out+4)
    Run for byte3 in {0x0e, 0x00}: out+4 == AB in both = both forms fine
    (padding class confirmed); only-0x0e-ok/only-0x00-ok/equal-fail each
    distinguish the leak/hypotheses (a follow-on integrity check: if 0x0e
    leaks into the NEXT instruction, the trailing store won't run and
    out+4 keeps the seed's neighbor — also observable).
    Owner keeps cal_fix unchanged until this runs; LD.32.F16.F32's genuine
    byte3=0xe0 (32-bit class) must stay untouched either way.

C21 update 2 (StrangeSole witness runs, 2x each): nonzero-witness executed
    cleanly — CONTROL (candidate replaced by LDO.32 i2,i1,0x0) reads back
    AB at out+4 => path live, so degeneracy excluded.  BOTH byte3 values
    (0x0e and 0x00) deliver NOTHING into i2, and the liveness variant proves
    the trailing stores after the candidate still execute (out+8 = AB) =>
    no half-byte leak shifting later instructions.  Field audit: emit
    `LSU0.LD.8 i2,[i1]` = 425d040e; fmt14 fields = dst5@13 (+2-bit@23),
    base@18 — inversion gives (dst=2, base=i1) CORRECT, base lands.  => the
    byte3 question is DOWNSTREAM: the 0x42-class 2-operand LD.8 as fielded
    is not a working byte-load on silicon (opcode class/offset semantics
    unproven; corpus has NO LD.8 instance at all — 82dd0e09 is
    LSU0.LD.32 fmt19, not LD.8).  Next discriminator (suggested by
    StrangeSole): test the c2-class DISPLACEMENT form `LSU0.LD.8 i2,[i1+1]`
    against a known nonzero byte (in-ptr+1 = 0xc4 witness); if that
    delivers, backends use the displacement form for byte loads and the
    0x42-class 2-op form retires.  No cal_fix byte3 pin possible (values
    indistinguishable because neither works).
CV sweep raw bytes delivered: shave-isa/cv_prototypes_raw.txt (79 classes,
hex + fmt + ops + text; 1 line repaired - VAU.ACCPW.U32 imm slot).
Silicon-confirmed CV classes so far (Mockingbird Example L): 0x82 LDIL,
0xc6 ADD, 0x86 AND, 0x46 BSF, 0x3c MUL.I8, 0x42 LD.64.H, 0xc2 LDO.64,
0x1c SUMC; harness negatives = non-evidence (context traps).

C21 update 3 (format-variant candidate set for StrangeSole's second matrix,
    base = i10/in-ptr, readback via proven ST.32 path):
      A) 822d1480            LSU0.LD.8 i2,[i10] fmt8  2-op 82-class
                             (inverts dst=2 @12(+4@22), base=i10 @17)
      B) 4220940d            LSU0.LD.8 i2,[i10] fmt12 2-op 42-class,
                             different layout than the dead 425d00XX
      C) 425d280e            4B, base=i10 @18, offset-2bit-class, off0
      D) 4251a820 / 42512821 / 4251a821   fmt13-class 4B at offsets 1/2/3
    controls: 425d000e / 425d040e (dead 2-op) and c22a542006 (dead c2 disp).
    Assembler quirk noted: `; fmt=N` for an npred-mismatched form is
    silently dropped (fmt8+displacement resolves to the 5B c2-class), so
    candidate attribution must come from the emitted bytes, not the request.
    Owner-side state: warning on the dead 2-op shape, no byte pins,
    G5 asserts warning presence/absence pairs; gates 185/185.

C21 FINAL (LD.8): round-2 matrix A-F ALL dead (StrangeSole, 9 distinct
    fieldings total incl. controls and offsets 0-3, nonzero witness
    in[0..3]=80/c4/de/c0, LDO.32 control proven): no LD.8 byte load writes
    its destination in any fieldable shape.  Honest framing: "no fieldable
    shape" (not "opcode absent") - delivery-vs-misdecode indistinguishable
    externally.  ADOPTED RULE: byte access routes through LSU0.LDO.32 /
    LD.16 + AND 0xFF (or the F16.F32 conversion path); byte3 question moot
    (0x0e/0x00/no-byte3-field shapes all identical); NO cal_fix pin; owner
    widens the warning to the whole LD.8 class (their plan on A-F failure).
C22. CV class silicon sweep — full 43-prototype hardened run (Mockingbird
    Example M, control probe validated 4/4 readback seeds, ~6s/probe,
    packets/probes/cv/):
    POSITIVE (effect-exact): 0x82 LDIL, 0xa9 LDIH (high-half=1, pairs with
    LDIL rule), 0x81 CMU.CP.32 (reg move), 0xc6 ADD+1, 0x86 AND, 0x46 BSF
    (14), 0x3c MUL.I8, 0x2c ABS.F32, 0x1c SUMC, 0x42 LD.64.H, 0x89 LD.32,
    0xc2 LDO.64, 0x49 LD.128.I16.I32 (base advanced +2), 0xfa
    (LDO.32 fmt111 class: i1=0x7FFFFFFF, differs from f0/f9 = class
    variants are real, corroborating C17/C21 class notes).
    TRAPS (pattern-dependent legality, NOT absence): 0x04, 0x0a, 0x20,
    0x47, 0xc5, 0xc7, 0xc8.  Corroborations: 0x0a = 3B LDIL-shape traps
    while sibling 0x0e executes-but-writes-nothing => gold's `42 0f 00`
    masquerade explained (same class that fooled our decode); 0x20 =
    ST.16.F32.F16 traps in this shape (matches earlier team result).
    NEUTRAL (predicate/store-class, unidentifiable by register readback):
    0x05, 0x08, 0x21, 0x22, 0x12, 0x13, 0x84, 0x85, 0x23, 0x33, 0x41,
    0x70, 0x80, 0x92, 0xc4, 0xf0, 0xf9.
    Harness lesson: address-chain construction must be control-probed
    first (their mis-built i6=i4+i5 chain caused phantom traps).

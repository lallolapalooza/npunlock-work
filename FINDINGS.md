# npunlock SHAVE toolchain reimplementation — findings ledger
Status as of the packet/latency discovery. Maintained by the main agent.

## Working (verified)
- Native toolchain root: `shave-toolchain-root/{bin/moviCompile,bin/moviAsm,bin/moviLLD,lib/mlibm.a}` (wrappers → llvm-14 clang SHAVE backend, shave_asm.py obj, patched lld). No wine.
- E2E: `add1-fp16.c` → our clang → .s → shave_asm obj → ld.lld → structure-valid ELF32 EM_SPARC passing `npunlock_parse_shave_elf`; npunlock default ctest 15/15 green; gated `shave_add1` byte-compare RED (900B vs 828B gold; truthful banner + ISel classes — accepted).
- NPU harness works: `tools/npu_add1_smoke.sh --elf <elf>` patches carrier, bundles, runs `npunlock-linux-probe`; gold .text bytes pass all 7 stages + oracle.
- Encoder ST/STI.16.F32.F16 field map fixed (vendor map: value@LE[16:12]|bit8<<4, base@LE[21:17], unit words 0x42040000 LSU0 / 0x49240000 LSU1, STI = LE bit27). Verified anchors: ST i6,i5=42640a00; ST i7,i8=42741000; STI i7,i8=49741008 (gold 0xaf); STI i6,i5=49640a08; ST i10,i29=42a43a00.
- Branch polarity: `BRU.BRA` branches iff reg==0; SETCC materializes 1=true; backend uses `ISD::getSetCCInverse` in LowerBR_CC (XOR hangs ISel — never reintroduce).

## The blocker: issued-stream model (root cause of zero-writes)
Our compiled kernel executes (all stages pass) but writes NOTHING to the output window (ACTUAL == carrier-init). Controlled probes (repeatable, shell-invoked):
1. **6-cycle load latency**: `LDO i0,i18,0x28` + K×`BRU.NOP` + `LSU1.ST.32 i0,i0` → K=0 fails; **K=6 PASSES and the window receives the real pointer** (00000000 0x2E014000, inv2 0x2E014010). Vendor string: `shave-load-latency` default 6 — "Number of cycles after issuing a memory load before the register is updated".
2. **Byte-stream packet alignment**: same load+6 NOP+store setup, varying what precedes the store:
   - `ST.32 i0,i0` → lands; `ST.32 i7,i0` (stale) → lands; `ST.32 i4,i0` → lands.
   - `LDIL i4,0x3C00` then `ST.32 i4,i0` → **does not land**; `LDIL+LDIH i4` then store → **does not land**.
   - `LDIL i5,0x0000 || ST.32 i4,i0` (one extra LDIL byte-run) → lands (stores 0).
   ⇒ the stream is parsed into FIXED PACKETS with descriptor/control bytes; shifting the stream by an LDI moves later instructions out of the slots where they execute. Vendor emits `LDIL||LDIH` in one bundle (template moviCompile64.ascii.txt:78385).
3. Vendor model strings: `shave-branch-delays` default 6 delay slots; `shave-aliased-store-distance`; `shave-lsu-flush-on-enter`; `NOP SYNC` ("wait for outstanding schedules"); moviAsm switches `-forceDelaySlots:<n>`, `-noCompressedNOPs`, `-noFinalSlotCompression`, `-noSlotAllPromo`; diagnostic "There are side effects after delay slots, remaining latency cycles: %d"; `BRU.NOP` = the NOP (our emitter encodes it as byte 0x07).
4. Wild stores to 0x70000000 do NOT fault on this hardware → fault channels are NOT valid evidence for reachability; only dump ACTUAL/EXPECT + smoke exit counts.
5. Graph facts: 2 invocations × 8 elements; params records at +0x00/+0x28 (inv1) and +0xc0/+0xe8 (inv2); rank=4, dims [8,1,1,1] at params+0x50; dims/strides pointers = params_section_base + RELA addend (0x50/0x60, 0x110/0x120, ...). Runtime rank is file-faithful (rank=0 gold test: that chunk wrote nothing).

## LATEST hardware-verified rules (2026-09-28, all measured on the MTL NPU via dump/smoke)
R1 CONSTANT MATERIALIZATION — our LSU0.LDIL encoding was WRONG (3-byte form the silicon doesn't execute as LDIL; every 32-bit constant = garbage → guards compare garbage → kernel returns before the store). Verified forms: `LSU0.LDIL r,lo` = 4B `82 | (r<<3|2) | lo16`; `LSU0.LDIH r,hi` = 4B `82 | (r<<3|3) | hi16`; `LSU1.LDIH r,hi` = `a9 | (r<<3|3) | hi16` (pair gives the exact 32-bit constant). Fix owned by AsmEncodingRE (shave-isa).
R2 LOAD LATENCY = 6 slots: an LSU-written register is visible only ≥6 instruction slots later (K=4/5 fail, K=6/7/12 pass; any slot-consuming filler counts; byte alignment irrelevant). Assembler now supports `asm/obj --latency 6`.
R3 BRANCH DELAY = exactly 6: the 6 instruction slots after a TAKEN branch execute before the jump (marker experiment m4). Our fp-loop backedge is followed by the epilogue `BRU.JMP i30` → kernel returns inside the delay window. Fill 6 slots after every branch.
R4 CMU FLAG SEMANTICS: `CMU.CMx.I32 <a>,<b>` does NOT write a 0/1 value — it sets a FLAG named by the operand register; `BRU.BRA <same reg>,target` branches on that flag (gold: `CMU.CMZ.I32 i5` @0x50 → `BRU.BRA i5,0xb7` @0x53, distance 1, no XOR). Our emitted `CMU → LDIL 1 → IAU.XOR a,a,1 → BRU.BRA a` branches on a register the compare never wrote ⇒ all guard decisions garbage. FIX: branch directly on the CMU flag with the desired condition sense; no materialization, no XOR.
R5 LSU UNIT CONFLICT: load and store must not use the same LSU unit in the same pipe window (LSU0.LDO+fillers+LSU0.ST ⇒ garbage; same store on LSU1 ⇒ correct; vendor: "LSU0/LSU1 unit is used more than once in the same pipe"). Gold: loads LSU0, stores LSU1. Our kernel emitted LSU0 stores ⇒ wrong.
R6 PACKETS: `a || b || c` = 1–3 ops, ≤128 bits total, ≤1 op per unit; byte stream = plain concatenation of member encodings (gold roundtrip 192/192). No standalone 1-byte packet/descriptor instruction exists; the odd bytes in gold are payload bytes of instructions our decoder mis-segments. Class byte0 of LDO is load-bearing but unresolved (BiologicalMockingbird, LOW confidence).
R7 Runtime values (invocation 1): rank=4 at params+0xc; params+0x10 = 0x80000050 (= params base 0x80000000 + 0x50 = dims VALUES region, deref'd by gold); out pointer = 0x2E014000 (inv1) / 0x2E014010 (inv2) at params+0x28; in pointer ≈ 0x2E014340.
R8 Wild stores do NOT trap; only dump_probe ACTUAL/EXPECT + smoke exit are valid evidence.

## REVISED model (2026-09-28, iter-24 measurements — supersedes R3/R5 where they conflict)
R9  BRANCH DESCRIPTOR: `BRU.BRA <flag>,tgt` NEVER transfers control unless the 2 bytes `1b 40` immediately follow it. WITH the descriptor the branch is IMMEDIATE (instructions at distance 0..8 after it do not execute). Gold emits descriptors after its conditional branches: `1b 40`, `2b 40`, `eb 30`, `1b 50`, lone `eb` — these are the previously-unnamed load-bearing single bytes. (The earlier "6 delay slots" measurement is the descriptor-less behaviour.) Action: assembler emits the descriptor (AsmSchedImpl); backend may emit plain BRU.BRA.
R10 FLAG/COMPARE SEMANTICS (measured): `CMU.CMZ.I32 X` + `BRU.BRA X,T` branches iff X != 0; `CMU.CMII.U32 A,B` + `BRU.BRA A,T` branches iff A != B; CMVV/CMII/CMIIABS behave identically; NO ordered-compare form exists. The flag survives intervening non-GPR-writing instructions but is CLEARED if the flag GPR is rewritten between CMU and BRA. Our `CMU → LDIL 1 → XOR X,X,1 → BRA X` is wrong (materialisation). Guard senses (EQ/UGT) must use a branch-over-branch idiom.
R11 LDI PAIR SPACING: `LSU0.LDIL lo` + `LSU1.LDIH hi` must be followed by >=6 slots before the next LSU op (else the device FAULTS); a lone LDIL or LDIH is fine. (This caused the fault after the 4-byte LDIL fix.)
R12 STATUS: with R11 + load-latency padding the kernel EXECUTES and writes 1 element (fp loop runs once). Remaining: drop the SETCC XOR, add branch descriptors, guard idiom → full loop iteration.

## Arbitrary-C gap (corpus report, npunlock-work/corpus/REPORT.md)
13/33 kernels pass compile+assemble+link+structure; 0 assembler/linker/validator rejections ⇒ all gaps are backend ISel/lowering. Top failure classes (each with minimal repros in corpus/repros/ and a runner corpus/run_corpus.sh):
C1 any function call (`calls are unsupported in wave 1`) — blocks all four shipped examples, varargs, recursion, libcalls, tanhf/mlibm.
C2 signed relational compares — a REGRESSION introduced by the SETCC rewrite (breaks npu3720_kernel.h's `exponent < 0`).
C3 int<->float conversions (uint_to_fp/fp_to_uint/fp_to_sint/sint_to_fp CannotSelect).
C4 div/mod by constant (mulhu) and sdivrem; runtime-divisor udiv works.
C5 globals/rodata/jump tables (GlobalAddress, br_jt CannotSelect).
C6 frame indices (runtime-indexed locals/struct copies) and register spills ≥35 live values → UNREACHABLE + SIGABRT.
C7 FP compares/selects; missing ctlz/cttz/ctpop/bswap/fabs patterns; lld misreports a missing entry symbol.
Priority: C2 (regression) → C1 (calls) → C3 → C4 → C5 → C6.

## ROUND-2 model (VendorRE2, hardware-verified; byte-parity requires these)
R13 COMPRESSED NOP: one byte = n issue slots, byte = (n-1)<<4 (low nibble 0), n=1..16 (0x00,0x10,0x20,0x30,0x40,0x50,…,0xf0). Load-latency gate: n=1..5 fault, n=6 (0x50) passes. Counts add (0x40+0x07 = 6 slots). Low nibble ≠ 0 is not a NOP. `BRU.NOP` (0x07) = 1 slot. Vendor: FUN_18005b9e0 (+0x5c = nr*0x10−0x10), printer FUN_1800642d0 decodes ((f>>4)&0xf)+1; `-noCompressedNOPs`/`-noFinalSlotCompression` = options+0x4c/0x4d. Gold was built with compression ON.
R14 BRANCH MARKER+FILL: `BRU.BRA` + marker byte (class ?b) + compressed NOP, fill count = 6 − k (k = real instructions executed inside the 6-slot delay window). Gold: `1b 40` (k=1), `2b 40`, `eb 30` (backward loop, k=3), `1b 50` (k=0 ⇒ IMMEDIATE), `eb+07`. Markers working alone: 0b/2b/4b/8b/cb/eb; `1b` needs a following byte; 3b/5b/db/fb fault; bare 4-byte branch faults. Safe default for us: `1b 50`.
R15 Gold's "odd bytes" = compressed NOPs (C12 flush-op theory refuted). Gold's remaining undecoded runs: 0x0b-0x49 (rank/!dims/dims-loop prologue), 0x79-0x7c, 0x7e-0x7f, 0x8a-0x8c.
R16 LDI pairing: always emit LO+HI pairs (LDIL + LSU1.LDIH) — materialises any imm16; no split path needed (C14 bit15-trap claim retracted as a probe artifact). IAU.ADD-imm: effective addend appears restricted to small values (gold uses 2) — pending sweep; reg-form unaffected.

## Workstreams in flight
- `shave-isa/` (agent AsmEncodingRE): packet/slot model implementation in shave_asm.py + annotated vendor-assembler rules (ANNOTATED_RULES.md); keeps gold roundtrip 192/192 green.
- `packets/` (agent BiologicalMockingbird): packet spec from moviAsm64 Ghidra/decompile (PACKET_SPEC.md, VERIFIED_EXAMPLES.md).
- `delta/` (agent InternationalUrial): differential "vendor vs upstream LLVM 14" delta + annotation (DELTA.md, ANNOTATED_DELTA.md) per user's strategy.
- `tmpdump/` (agent RoughFinch): measurement harness (latency K-sweep, branch delay count, LSU flush).
- Backend: /home/asdf/projects/llvm-14 (needs a fresh owner agent for the latency/delay-slot + packet-aware emission once the spec lands).

## Acceptance gate (unchanged)
`cd /home/asdf/projects/npunlock-work/npunlock && ./tools/npu_add1_smoke.sh --elf <our.elf>` → exit 0, oracle exact; then gated ctest record and BYTE_EXACT_LOG finalization.

## Correction + current prime suspect (turn ~50)
- CORRECTED: our compiled add1-fp16 kernel writes ZERO elements (its output == the do-nothing kernel's carrier-init bytes). Earlier "one element written" / "count resolves to 0" readings were carrier-init artifacts; every witness must use distinctive values (high registers i20+).
- Padding-authority question CLOSED: the assembler's auto-legalizer is a no-op on our backend's output (E1 == E2; E3/E4 fault) ⇒ backend padding is what keeps execution alive; no double-padding confound.
- PRIME SUSPECT: `shave_asm.py obj` drops BIT 31 (byte3 bit7) of AU-immediate forms while `asm --hex` is correct (c6202581 vs c6202501; c6208480 vs c6208400; c6200482 vs c6200402) ⇒ every AU-immediate in a compiled kernel is a different instruction on silicon. Fix owned by AsmSchedImpl (shave-isa). The earlier "AU imm&3" restriction was this bug's shadow (refuted).
- Also fixed this round: signed GT/GE/LT/LE compare regression in the backend (sign-flip formulation), verified on corpus repros r62 + the repo's own npu3720_kernel.h.
- Backend state: all measured legality rules implemented (SHAVEFillDelaySlots: LDI-pair→LSU spacing, branch descriptors 1b40/2b40, load 6-slot, DIV 13-slot; SETCC flag protocol without XOR; LSU1 stores). Structure gate + offline ctest 15/15 green; gated shave_add1 byte-compare RED (accepted class).
- Open for arbitrary C (corpus REPORT.md): calls (LowerCall fatal) → int↔float conversions → div/mod by constant + sdivrem → globals/br_jt → frame indices/spills.

## Turn ~55: retractions + hygiene + trustworthy value map
- RETRACTED: the "offset-0 LDO faults" defect — it was a probe artifact (witness registers r16..r31 do not round-trip in the 4-byte forms the encoder picks). Offset-0 `LSU0.LDO.32 d,base,0` works (`c252640100`), as does the 2-operand `LD.32` (`825d2401`). AsmSchedImpl's D5 rewrite (LDO.W offset 0 -> LD.W) is silicon-verified and now default; keep it (no behavioural change, verified form).
- PROBE HYGIENE (mandatory): witness payload registers must be r0..r15 for ALL units; high registers only for LDI payloads. Carrier-init must be subtracted from every ACTUAL before claiming a write. Only distinctive witness values count.
- TRUSTWORTHY RUNTIME MAP (low regs, carrier-init subtracted): params+0x0 = 0x2E014340 (input pointer), +0x4 = 0, +0x8 = 1, +0xc = 4 (rank), +0x10 = 0x80000050 (dims pointer), +0x28 = output pointer (0x2E014000 inv1 / 0x2E014010 inv2).
- CURRENT KERNEL STATE: .text ~496 B, structure gate green, offline ctest 15/15, execution PASS, oracle FAIL with ZERO writes (ACTUAL == carrier-init). Reads are proven correct; the store/loop path is not landing writes.
- ASSEMBLER: 171/171 gates green (incl. obj==asm G9 and the D5 rewrite); gold roundtrip 192/192. Compressed NOPs, branch descriptors, latency rules, LSU discipline all implemented and hardware-pinned.

## FINAL STATE (report to user)
DELIVERED
- npunlock cloned at npunlock-work/npunlock; the README paragraph about downloading proprietary MoviTools is REPLACED with the reverse-engineered/native toolchain story (README L113-125) plus wiki/GET_SHAVE_TOOLCHAIN.md, ABI and wiki sweeps; offline suite 15/15 green.
- Native, wine-free SHAVE toolchain: llvm-14 fork (clang + SHAVE target), shave-isa/shave_asm.py (assembler/obj writer, 171/171 gate suite incl. gold roundtrip 192/192 byte-identical), lld patched for elf32/EM_SPARC (lld-sparc/), packaged as shave-toolchain-root/{bin/moviCompile,moviAsm,moviLLD,lib/mlibm.a}.
- E2E: add1-fp16.c -> our clang -> .s -> shave_asm obj -> ld.lld -> structure-valid ELF32 EM_SPARC passing npunlock_parse_shave_elf (E2E_REPORT.md).
- NPU harness: npunlock-patchblob CLI + tools/npu_add1_smoke.sh + NPU_REPORT.md; GOLD's ELF executes 7/7 NPU stages with exact oracle; mlibm.a linking verified (all reloc types exact; 169/204 symbols resolve).
- Deep RE: delta/ (vendor-vs-upstream LLVM inventory + R1-R21 annotated rules; 54 -shave-* options, 11 passes, 36 SHAVEISD nodes, 41 CPUs), packets/ (packet/filler/branch-descriptor model, gold byte map), rptrace/ (vendor semantics), corpus/ (arbitrary-C gap analysis), FINDINGS.md (this ledger).

REMAINING (hardware-semantics last mile)
1. Our own compiled add1-fp16 kernel executes but writes ZERO elements (oracle FAIL). Reads are proven correct (in-ptr 0x2E014340, rank 4, dims ptr 0x80000050, out ptr 0x2E014000/0x2E014010); the store/loop path is not landing writes. Open lead: byte2 bit0 variant difference on the backend's offset-0 loads (`c2 52 65 01 00` vs verified `c2 52 64 01 00`) — needs a rebuild+diff with the current assembler; and a witness-in-loop test (store a distinctive constant inside the fp loop) to split control-flow vs data-path.
2. Arbitrary-C gaps (corpus/REPORT.md): calls (LowerCall fatal; would unlock the shipped examples and math), int<->float conversions, div/mod by constant + sdivrem, globals/br_jt, frame indices, spills>=35 live values, fp compares/selects, ctlz/cttz/ctpop/bswap/fabs.
3. Byte-exactness vs gold (192 B) remains open: gold's undecoded 0x0b-0x49 prologue + `e9 ?? 24` motifs, class-byte bit semantics, marker high-nibble semantics.

## ROOT CAUSE OF ZERO-WRITES (found, turn ~58) — flag polarity inverted
Measured on silicon (my runs; carrier-init subtracted; low registers):
- K1 (control): read out-ptr + 6 fillers -> LDIL/LDIH i5=0xAAAA -> 6 fillers -> LSU1.ST.32 i5,i0 -> JMP => ACTUAL `aa aa 00 00` (store path + pointer correct).
- K2: `LDIL/LDIH i2=0` -> `CMU.CMZ.I32 i2` -> `BRU.BRA i2, LEXIT` (fall-through stores 0xAAAA, LEXIT stores 0xBBBB) => ACTUAL `bb bb 00 00` ⇒ **CMZ(X)+BRA branches iff X == 0** (the earlier "iff X != 0" note was wrong).
- G3 (our exact rank-guard sequence, rank=4 => i1 = (4-1)/15 = 0) => ACTUAL `bb bb 00 00` ⇒ the guard EXITS when it must not.
⇒ Every guard in our kernel exits unconditionally: rank guard (i1=0 for legal ranks) and dims guard (1/(0x80000050+1) = 0) both branch to the epilogue. The fp loop is never reached; the kernel writes nothing. Single bug explains the whole residual failure.
FIX (backend, dispatched to MedicalCrayfish): materialise the flag as 0 exactly when the branch must be TAKEN (invert the condition feeding the CMU; keep CMU→BRA adjacency, no XOR, and the CMU must be the last writer of the flag register). Verify: V1 guard ladder (fall-through store must land), V2 full kernel oracle, V3 rank=0 negative control (must exit).

## Turn ~60: polarity fix verified — writes now land
After restoring `ISD::getSetCCInverse` in LowerBR_CC (flag = NOT(cond), branch fires iff the condition holds):
- ACTUAL is no longer carrier-init: bytes 0-1 = `00 00` (previously `00 01`) and bytes 16-17 = `00 00` (previously `20 01`).
- Interpretation: the kernel now REACHES and STORES (one f16 element per invocation), but the stored value is 0 and only one iteration ran.
- Two residuals: (a) fp-loop trip count should be 8 (count = product of dims = 8*1*1*1) — only one iteration observed; (b) the stored value should be in[i]+1.0 (≈0x3C00 for in=0) but is 0x0000 — suspect the LD.32.F16.F32 load path (address or conversion latency) or the add input.
- Probe hygiene: LSU destination registers must avoid the trap set {i19, i21, i24, i30} (VendorRE2 measured; r25 fine). Use r0..r15 for LSU ops.

## FINAL — acceptance gate PASSED (2026-09-28)
`cd npunlock-work/npunlock && ./tools/npu_add1_smoke.sh --elf ../our-add1-fp16.elf` -> **exit 0**; stages loader/device_selection/graph_extension/graph_create/initialization/execution/oracle all passed; `level_zero_result 0x0`; oracle `fp16_carrier_add1_exact` **exact, ACTUAL == EXPECT 32/32**. Verified twice independently (main agent, own work dir) + 10/10 by the owning agent.
ELF: `npunlock-work/our-add1-fp16.elf`, 1124 B, sha256 `bb25f4b58ca91e57d87409a9…`, rebuilt byte-identical through the shipped `shave-toolchain-root` integration path. Differs from gold's 828 B fixture (ours is our own compilation).
Gates: structure PASSED (`npunlock_parse_shave_elf`; .text 496 B vs gold 192 B = 2.58x, accepted); offline ctest **15/15**; assembler `gates.py` **181/181**; gold `.text` round-trip 192/192 byte-identical; gated `shave_add1` byte-compare RED (accepted class).

### Root causes closed in this phase
1. **Branch descriptor = SENSE, not direction**: `1b 40` = taken iff CMU flag source == 0, `2b 40` = iff != 0; our lowering pairs `getSetCCInverse` with `1b 40` everywhere. Fixing the old forward/backward split plus the polarity restored all guards/loops.
2. **Load write-back discard**: any same-license LSU op (incl. LDIL/LDIH) inside a load's window silently drops the load's result (dims deref read the address 0x80000050 instead of dim=8). Backend now counts only non-LSU slots (6 for LDO.32, 7 for LD.32.F16.F32) and pushes LSU ops past the window.
3. **`SAU.ADD.F32` third operand is a hardware constant SELECTOR, not a GPR**: i0→0.5, i1→1.0, i2→2.0, i3→3.0, i4→1.41494, i5→0.70604, i6→π, i7→e, i8→10.0 — register contents irrelevant (gold's `add1` is `3ce09c30` = `SAU.ADD.F32 i7,i7,i1`, and gold's `.text` contains no 1.0f bits). Backend now emits `FADD_F32_ONE` for `fadd(x,1.0f)`, dropping the constant-pool load.
4. **`SAU.ADD.F32` result latency ≥2 slots** before its consumer (backend pads 3, `-shave-fadd-latency`): a 0-slot store stored the pre-add value and hung the device.

### Remaining (declared, not blocking)
- Byte-exactness vs gold: gold's undecoded runs 0x0b-0x49, 0x79-0x7c, 0x7e-0x7f, 0x8a-0x8c; class-byte bit semantics unresolved (default c2 is the silicon-verified class).
- Arbitrary-C gaps (`corpus/REPORT.md`): calls (`LowerCall` fatal) → int↔float → div/mod-by-const + sdivrem → globals/br_jt → frame indices/spills → fp compares.

## Encoder closures (AsmEncodingRE, final)
- **LD.8**: the `42 5d 00 XX` 2-op form is NOT a working byte-load as fielded — StrangeSole's witness+control+liveness run shows the path is live (the `LDO.32` control reads 0xAB) yet BOTH byte3 variants deliver nothing, with no half-byte leak. Field audit says the fmt14 fields (dst@13, +2@23, base@18) invert correctly, so it is not a field-position bug. Corpus has **zero** LD.8 instances (`82 dd 0e 09` is LD.32 fmt19 — an earlier misattribution, corrected). Next: test the c2-class displacement form `[i1+1]` against the known 0xc4 witness; byte3 is not pinnable (indistinguishable). Pin-or-retire verdict routes to AsmSchedImpl.
- **CV/class sweep**: `cv_prototypes_raw.txt` (82 lines, hex-valid; ERROR line repaired → `23 49 04 00` = VAU.ACCPW). Silicon-confirmed classes: 0x82 LDIL, 0xc6 ADD+1, 0x86 AND, 0x46 BSF, 0x3c MUL.I8, 0x42 LD.64.H, 0xc2 LDO.64, 0x1c SUMC. Harness negatives are context traps, not evidence of absence.
- **Legality restated**: it is a pattern property of (form × register × immediate); backend rule = effect-check every emitted combination with conservative defaults (dst r0..r15, small LDI immediates, 5-byte `LDO.32`, 2-op `LD.32` < i16). The default class byte stays **c2** (silicon-verified).
- **Open**: `SAU.ADD.F32` discriminators (b-sweep with dst==a==i1 over i1..i8; dst!=a with fixed b; gold-shape dst==a b-sweep) pending from StrangeSole — no fmt3 re-pin until that table lands. Repro set `loopdiff/{fa1,fa1b,fa2,fa2b,fb}.s`.
- Gates after all encoder work: **183/183**, gold round-trip **192/192**, G9 obj==asm byte equality.

### LD.8 — CLOSED (StrangeSole, 9 fieldings with controls)
Nine distinct LD.8 fieldings tested with a proven base/readback control (`LDO.32` control delivering `80 c4 de c0` = in[0..3]): `42 5d 00 {0e,00}`, `82 2d 14 80`, `42 20 94 0d`, `42 5d 28 0e`, `42 51 {a8 20,28 21,a8 21}`, and the c2-class 5-byte displacement at offsets 0..3 (`c2 2a 54 20 06`) — **none deliver anything at any offset**. Conclusion: no LD.8 byte load is fieldable in these shapes; byte3 is moot. **Recommendation adopted: route byte access through `LDO.32`/`LD.16` + mask, and drop the LD.8 pin from `cal_fix`.** (Supersedes the earlier "test the displacement form" step in the encoder section above.)

## CALLS — mechanism recovered and HARDWARE-VERIFIED (CorpusWave1, pre-kill)
From vendor-linked kernels (375 in `/tmp/prebuilt3720`, e.g. `activation_abs…elf` at `0x1d000036`) and `mlibm.a` `math_lite.o` (`tanhf`→`expf`, `expf`→`floorf`):
```
a9 f3 00 1d     LSU1.LDIH i30, 0x1d00      ; hi16 of target (reloc type 202 at +1)
f2 c0 01        LSU0.LDIL i30, 0x01c0      ; lo16 of target (reloc type 200 at +3)
47 f2 01        BRU.SWP  i30               ; <-- THE CALL (swap PC <-> i30)
50              compressed NOP             ; 6-slot delay fill
c2 e2 67 39 00  ...                        ; fall-through = return point
```
- `47 f2 01` = **`BRU.SWP i30`** — the call instruction (3 bytes; encodings.json: `BRU.SWP`, fmt2, len3, fixed=0x00010247, mask=0x00ff07ff, reg field bits[15:11]).
- `47 f1 01` = **`BRU.JMP i30`** — the jump/return (used by the entry stub); `SHAVE_CALL` is the vendor's pseudo that expands to the `BRU.SWP` form.
- **Address materialisation into i30**: `LSU1.LDIH hi16` then `LSU0.LDIL lo16` (hi first), carrying relocs 202/200 exactly as `mathlink/MATHLINK_REPORT.md` documented.
- **Return value in i18** (measured on hardware by CorpusWave1 with probes in `/tmp/pb`).
- CorpusWave1 stated: "Call mechanism fully verified on hardware (incl. mlibm's `floorf`; return value in i18)" and was reading `SHAVERegisterInfo.td`/`SHAVEFrameLowering` to implement `LowerCall` when billing killed it.
- Args in i18.. onward is consistent with the observed use of i18 for the return value; the full argument-register order and the callee-saved set were the remaining measurement items (see the revived GapCalls task).

## Vendor ISel vocabulary recovered (GapCasts-2) — treat as the spec
`r2gvp04w-re/re_strings/moviCompile64.ascii.txt` is the vendor compiler's TableGen node/instruction table:
`CMU_CPII_i32f32`=SINT_TO_FP · `CMU_CPII_f32i32`=FP_TO_SINT · `CMU_CPII_u32f32_Myr2v3`=UINT_TO_FP · `CMU_CPII_f32u32_Myr2v3`=FP_TO_UINT · f16 family `CMU_CPII_f16_f32`/`f32_f16`/`i32f16`/`f16i32`/`f16i16`/`i16f32` · scalar select `SELECT_IRF32`/`SELECT_IRF16` (= the fix for "only i32 value-selects are supported") · `CMU_MIN_f32_Myr2v3`/`CMU_MAX_f32_Myr2v3` · flag forms `CMU_CMII_f32`/`CMZ_f32`/`CMVV_f32`.
**No scalar f32 sqrt/pow instruction exists** — `fsqrt`/`fpow` are libm calls: mlibm's `sqrtf` is a linked 309-byte function (`activation_sqrt`.3720xx.elf: `activation_sqrt` 194 B @0x1d000000, `sqrtf` 309 B @0x1d000760), and `halfmath.o` provides `__pows`/`__exp2s`/`__logs`/`__cbrts`/`__hypots`. Only f16 (`SAU_SQT_f16_l`) or Myr4-immediate (`SAU_SQTV_imm_Myr4`) sqrt forms exist; `SQRT_Fp32`/`FPOW` strings in that file are x86 names. Therefore `fsqrt`, `fpow` and `expf` all fall out of the one `LowerCall` fix.
Evidence discipline note recorded by GapCasts-2: byte-pattern scans are not evidence for these encodings (only ~13 significant bits → ~500 random hits in 3.9 MB); only verbatim hits inside known vendor math functions, or byte-exact `shave_asm.py` round-trips with legal operands, count.

## ABI + globals + fp-compare findings (GapGlobalsFrames-2, measured on the live clang-14 build)
**Stack/return ABI is PINNED:** stack pointer = **i19**, return address = **i30** (which is exactly what `BRU.SWP i30` swaps into on a call). Proven twice: (1) every CIE in mlibm/mlibcxx/mlibc_lite declares `DW_CFA_def_cfa: r19 ofs 0` with `Return address column: 30`; (2) real vendor code shows `IAU.SUB i19,i19,0x10` / `LSU1.STO.32 i30,i19,0xc` / `LSU0.LDO.32 i30,i19,0xc` / `IAU.ADD i19,i19,0x10`. All needed encodings already assemble byte-identically. **Bug found:** `GPRF16` in `SHAVERegisterInfo.td` has a 2-byte spill size (vendor frame slots are 4 bytes) — unsafe.
**Global addressing:** the assembler+linker half ALREADY WORKS (`shave_asm.py` emits reloc 200/202 for LDIL/LDIH+symbol; `ld.lld` merges `.rodata` into `.text` at 0x1d0000xx and the validator accepts). Only the compiler-side selection is missing: `SHAVEISelDAGToDAG.cpp Select()` (127-302) has no case for `ISD::GlobalAddress`/`BlockAddress`/`ExternalSymbol`/`JumpTable`. **Jump tables must be OFF as the vendor does**: the ctor falsely claims `ISD::BRIND` = Custom (`SHAVEISelLowering.cpp:110`) while BRIND is unimplemented (`:710-712`), and `TargetLowering::areJTsAllowed()` keys off that → `br_jt` gets created and then can't be selected. Fix = drop the false claim, do NOT implement jump tables.
**"Unsupported condition code 20" is STALE:** cc 20 = `ISD::SETLT`, implemented at `SHAVEISelLowering.cpp:982-1033` along with the other 9 integer codes (a02 and r62 compile today). The live gap is the **14 floating-point codes**, which abort earlier in `SHAVEISelDAGToDAG.cpp:245-252` ("unsupported fcmp"), plus the f32 value-select abort at `:206-212`. Every fp compare reduces to the existing i32 codes via a monotone IEEE key + orderedness test — no new condition codes required.

## SHAVE CALL ABI — measured spec (GapCalls-2, vendor DWARF/CFI + disassembly)
| item | value | evidence |
|---|---|---|
| arg registers | **i18, i17, i16, i15, i14, i13, i12, i11** (descending, 8 slots) | vendor DWARF formal-parameter locations (`copysignf` reg18/reg17, `fabsf` reg18, `__remquo_pos_only` [18,17,16], `mmap` 6 args -> i13, an 8-arg libcxx fn ends at i11) |
| return | **i18** (<=32-bit); 64-bit: **high in i18, low in i17** | DWARF + code |
| link register | **i30** (`BRU.SWP i30` call / `BRU.JMP i30` return) | disassembly |
| stack pointer | **i19**, no frame pointer | `DW_CFA_def_cfa: r19 ofs 0` |
| callee-saved | **i11..i18, i20..i29, i31** (+ VRF r48..r63) | per-object CIE `DW_CFA_same_value` list, byte-identical across independent objects |
| caller-saved | **i0..i10** | same CIE / ABI |
| frame | grows down, 8-byte aligned: `IAU.SUB i19,i19,N`; **RA saved at `[i19+N-4]`**; epilogue `LSU0.LDO.32 i30,[i19+N-4]` … `BRU.JMP i30`; 64-bit saves `LSU1.STO.64`/`LSU0.LDO.64` on even pairs | disassembly |
| SP adjustment | callee for its own frame; **caller** for outgoing stack args (`ADJCALLSTACKDOWN/UP`, ANNOTATED_DELTA.md:657) | vendor opcode table |
| call sequence | `LSU1.LDIH i30,hi16` ; `LSU0.LDIL i30,lo16` ; `BRU.SWP i30` ; 6 issue slots execute before the transfer | disassembly |

**Two defects found in the in-flight backend tree** (to fix): 64-bit return halves **swapped** in LowerCall/LowerFormalArguments/LowerReturn; call-preserved mask is `0x7ffc0000` but the measured ABI value is **`0xBFF7F800`**. Also `SHAVERegisterInfo.td` defines only i0..i30 — **i31 is missing despite being in the preserved set** (same area as the unsafe 2-byte GPRF16 spill size).
Scope note: the >=9-stack-argument path is documented but unverified and is NOT needed for the llama.cpp path (`expf`/`sqrtf` are 1-arg) — implement the register path correctly and report stack args as unsupported rather than guessing.

## 2026-09-28 — CALLS landed (CorpusWave1-2) + newly measured rules

ABI (all re-measured on the real NPU with `dump_probe`, corroborating
GapCalls-2's DWARF-derived spec): arguments i18,i17,i16,... (vendor never uses
a parameter register below i11); return in i18 (64-bit: hi i18, lo i17);
SP=i19; link=i30 (call = `BRU.SWP i30`, return = `BRU.JMP i30`); callee-saved
i11..i18 + i20..i29; scratch i0..i10.  Measured directly: i20,i22,i23,i25,
i26,i27,i28,i29 all survive an `fmaf` call; i10 is clobbered by `expf`
(keeping the output pointer in i10 across the call silently sent every later
store to a garbage address).

NEW HARDWARE RULE (blocker, now fixed in the backend's delay-slot pass):
**the callee-address `LDIL`/`LDIH` pair must be >=6 issue slots before
`BRU.SWP`.**  With the pair immediately before the call, `BRU.SWP` reads a
stale i30 and *no call happens* — the "result" is then just the argument
register, unchanged (probe: `out[1]=expf(in[0])` returned in[0]; with 6
fillers inserted it returned the exactly-rounded expf).  The vendor's own
sequence is `LDIH; LDIL; BRU.SWP`, which is only legal because of the same
forwarding; our `fixConstantToLSU` did not recognise the *symbolic*
(LDILSYM/LDIHSYM) pair, so no fillers were emitted.

Also re-measured (used to pick the emission order): `LDIL` is immediate and
**zero-extends** (a lone LDIL, or LDIH-then-gap-then-LDIL, yields the low half
only); `LDIH` inserts.  Hence the working materialisation order is
**LDIL (low half) first, then LDIH** — adjacent `LSU1.LDIH` / `LSU0.LDIL`
(hi first) loses the LDIL write entirely (probe: 0x56781234 requested, 0x56780000
observed; LDIL-first gives 0x56781234).

Assembler trap set re-confirmed: single `LDIL` into **i21** faults the kernel
(0x2121 and 0x2020 immediates both), while i20/i22/i23/i25..i29 are fine —
probe hygiene only, keep LSU destinations <= i15.

Verification of item 1 (calls): `corpus/kernels/l01_silu_f32.c` compiles,
assembles, links against vendor `mlibm.a` and runs on the NPU; a dedicated
probe (`out[1] = expf(in[0])` with `in[0] = -6.96148681640625` read at runtime)
returned `0x3a786e2a`, bit-identical to the correctly-rounded `expf`.

## 2026-09-28 (addendum) — l01 numeric proof + preserved-set decision

`corpus/kernels/l01_silu_f32.c` runs on the NPU with bit-exact numerics:
`in[0] = -6.96148681640625` (read at runtime) -> `out[0] = 0xbbd7f9d3` =
silu(in[0]) **exactly** (host double oracle); `out[1]` is 1 ULP off, i.e.
inside `expf`'s own error.  So the mlibm call executes and computes.

Preserved-register set used: **i18..i30** (`0x7ffc0000`), i.e. only the
directly measured i20..i29 plus the reserved registers; i0..i10 are scratch
and are declared as call-clobbered in the SHAVECALL pseudo (`Defs`).
GapCalls-2's DWARF-derived `0xBFF7F800` (i11..i18 + i20..i29 + i31) was
REJECTED: an `fmaf` probe that plants 0x1111..0x1414 in i11..i14 and reads
them back returned 0x33800000 / 0 / 0 / 0 for the two registers it could
observe, and declaring i11..i17 preserved regressed l01_silu_f32 from
producing silu values to writing nothing at all.  Re-measure with a correct
probe (reload the output pointer from the stack after the call) before
widening the mask.

## NPU OFFLOAD VIABILITY — ceiling REFUTED (MultiOpProbe, measured)
**1. New carriers of arbitrary shape ARE compilable on Linux with the installed driver** (no MoviTools, no Windows): `multiop/scripts/ircompile.c` uses `ZeGraphCreate2` (NGRAPH_LITE) + `GetNative2`; IR -> native blob in 0.13-0.41 s cold, 5-15 ms cached. Patchable carriers built with up to **8192 f16 elements per ACT invocation** (tensor N=16384), all outputs element-exact. The shipped path fails only because `CMakeLists.txt:162` builds `npunlock_worker` under `if(WIN32)` and `src/workers/ir_worker.c:1` includes `<windows.h>`.
**2. Multiple different kernels per invocation: YES.** `patchblob` appends one image, then rewriting the `.rlt.text.ActKernelRanges` RELA addend + range extent (12 bytes) points each ACT range at its own entry. Proven: 2 distinct kernels on `abs-add-1x16-tile1` (bit-exact vs host oracle, 3/3), 4 distinct on `abs-abs-1x32` (ranges 0/2/4/6), 2 distinct on IR-built 4-target carriers. **NEW RULE: the range code address is rounded DOWN to a 0x400-byte boundary — entries must be 0x400 apart.**
**3. Host cost (the decisive number):** one fresh call = create+initialize+first-execute = **22.9-55.4 ms** (essentially independent of tile size — matches the backend's 19.4/46/48 ms); **re-executing a LIVE graph = 0.14-0.5 ms**. Elements/s: warm 149k (N=32) -> **77.4M (N=16384)**; fresh-graph 670 -> 385k. Stages: patchblob 3-23 ms, bundle create ~180 ms, verify ~62 ms, linux-probe ~29 ms.
**4. Carrier menu at 1x1024 f16** — ACT-patchable: `Abs`(4x512), `Elu/Erf/Sigmoid`(2x1024), `Exp/Floor/Log/Sign/Sqrt/Tanh`(4x512), `Maximum/Minimum/Power/Divide` 2-input (4x256). **DPU-only, NOT patchable**: Add, Clamp, Multiply, Relu, Subtract. Compile failed: Ceil, Negate. (The carrier's own op is irrelevant — we overwrite the ACT code — so use e.g. an `Abs` carrier and patch our own kernel in.)
**Consequence:** with session reuse (re-execute a live graph) plus an 8192-element ACT invocation, **77.4M elements/s** is achievable — ~100x the earlier 46 ms/call figure. NPU offload is therefore viable for real tensors; the two blockers were per-call graph creation and the tiny fixture carriers, and both are now solved or have a concrete solution.

## 2026-09-28 (CorpusWave2) — f32 arithmetic, call-preserved set, reserved-register liveness

### A. f32 arithmetic was WRONG for every op except `+1.0f` (fixed, bit-exact verified)

Measured with single-op hardware probes (`one.s`/`lat.s`/`fpmix.s`; act input words
`a = 0x42334011 (44.812564849853516)`, `b = 0x481e442b (162064.671875)`):

| emitted | hardware result | verdict |
|---|---|---|
| `SAU.ADD.F32 d,a,b ; fmt=2` | `a+b` | correct |
| `SAU.MUL.F32 d,a,b ; fmt=2` | `a*b` | correct |
| `SAU.SUB.F32 d,a,b ; fmt=2` | `a-b` | correct |
| `SAU.DIV.F32 d,a,b ; fmt=1` | `a/b` | correct |
| `SAU.{ADD,MUL,SUB}.F32 d,a,b ; fmt=3` | `a <op> C(regnum)` | third operand is the CONSTANT SELECTOR |
| `VAU.MUL.F32` / `VAU.SUB.F32` (any fmt) | **destination never written** | unusable |

**RULE (opcode/format):** the 4-byte `SAU.*.F32` form has bit 28 inside the fmt nibble:
bit28 = 1 (fmt=3) makes the THIRD operand a small-constant selector addressed by the
operand's *register number* (0=0.5, 1=1.0, 2=2.0, 3=3.0, 4=1.41494, 5=0.70604, 6=pi,
7=e, 8=10.0) and the register's contents are ignored; bit28 = 0 (fmt=2) is a genuine
third register read.  `shave_asm`'s default `resolve_form` picks fmt=3, which is why
`x + 1.0f` happened to work (gold's own add-1 is `3c e0 9c 30`, fmt=3, selector 1) and
why every other f32 op silently computed `x <op> <selector>`.  `VAU.*` forms decode to
an instruction that does not write its destination at all.  `SAU.DIV.F32` is only
correct as fmt=1.
Backend now pins `; fmt=2` on FADD/FSUB/FMUL (FADD_F32_ONE keeps `; fmt=3`; FDIV uses
`; fmt=1`) and emits all of them on **SAU**.

**RULE (f32 result latency, re-measured for the register forms):** `SAU.MUL.F32` and
`SAU.SUB.F32` need **3 issue slots** before their first consumer, exactly like
`SAU.ADD.F32`: with 0 or 1 intervening slots the consumer reads the destination's
PREVIOUS value.  `SHAVEFillDelaySlots` only padded `FADD_F32`/`FADD_F32_ONE` before;
`FSUB_F32`/`FMUL_F32` are now covered too (`-shave-fadd-latency=3`).

Verification: `kernels/a05_scale_f32.c` (`out[i] = in[i]*1.5f + 0.25f`) is BIT-EXACT
for all 8 visible output words against an IEEE-float32 oracle; `l01_silu_f32`
(mul/add/div + `expf`) still produces its previously verified bit-exact output.
The earlier "f32 arithmetic is a copy / no-op" symptom was this selector semantics
plus the unwritten VAU destination, not a copy.

### B. The call-preserved register set is NOT the vendor DWARF CIE list

The CIE of every mlibm/mlibcxx object declares `DW_CFA_same_value: r11..r18, r20..r29,
r31`.  That list is NOT the callee-saved set: it also contains r18, which is the
argument AND the 32-bit return register, so it is clobbered by construction.

Measured on silicon (a value carried through a real `expf` call inside the otherwise
unmodified, previously bit-exact `l01_silu_f32` assembly, one register renamed at a
time, then run through `dump_probe`):

| register | survives `expf`? |
|---|---|
| i7, i9 | yes (this call does not touch them; they are still CALLER-saved) |
| i8, i10, i11 | **NO** (device fault / kernel writes nothing) |
| i12..i16, i17 | yes |
| i23, i25..i29, i31 | yes |
| i24 | device fault even as a pure ALU destination/loop counter |

`i11` is the striking one: the CIE claims it callee-saved, but mlibm's `expf` does not
honour that (a loop counter or an output pointer in i11 makes the kernel write nothing,
while the *identical* code with i12..i17 is bit-exact).  The new mask therefore
preserves **i12..i16, i19, i20..i29, i31** and treats i11, i17 (low half of a 64-bit
return) and i18 (argument/return) as clobbered.

**RULE (RegMask bit numbering — this is a trap):** a RegMask bit index is the *Register
enum* value, and TableGen numbers registers from 1, so hardware `i<n>` is **bit n+1**
(`SHAVE::R0 == 1`, `SHAVE::R31 == 32` = the first bit of the mask's second word).
Writing the hardware set as a literal (`0xBFF7F800` for i11..i18|i20..i29|i31) is
off by one and would name i10 (caller-saved) as preserved.  The new mask word0 is
`0x7FF3E000`.

### C. A reserved ABI register consumed as a bare physreg is silently clobbered by a call

`LowerFormalArguments` returned `DAG.getRegister($r18)` instead of a `CopyFromReg`, so
every use of the incoming argument was a direct read of the *physical* register and the
allocator had no virtual register it could move.  A kernel that uses `layerParams` after
a call therefore read the CALL's return value:

    LSU0.LDO.32 i12, i18, 0x0     ; in  = *(params)
    LSU0.LDO.32 i18, i12, 0x0     ; arg = in[0]      <- overwrites $r18
    ... BRU.SWP i30 ...
    LSU0.LDO.32 i0, i18, 0x28     ; out = *(params+0x28)  <- $r18 is now expf()'s result

The kernel then wrote nothing (measured).  Fixed by using `DAG.getCopyFromReg` (which
materialises `%arg = COPY $r18`); i18 is also no longer reserved.  After the fix the
same kernel returns `expf(-6.96148681640625) = 0x3a786e2a`, bit-identical to the host.

### D. add1-fp16.c: no "1012 B / executes as a copy" regression exists

Compiling `npunlock/tests/fixtures/npu3720/add1-fp16.c` fresh through
`NativeLibraries('npunlock/build/fresh').compile_shave(src, toolchain_dir=
'shave-toolchain-root')` yields a **1172 B** ELF and
`npu_add1_smoke.sh --elf <that ELF>` passes with `oracle: fp16_carrier_add1_exact`
(all stages passed).  Assembling the same `.s` with all four assembler copies (live
`shave-isa/shave_asm.py` plus `corpus/toolchain-snapshot{,,-b,-c}/isa/shave_asm.py`,
four different sha256s) produces a **byte-identical 1172 B ELF** in every case — so no
assembler copy turns the add into a copy.  The add is `SAU.ADD.F32 i2, i2, i1 ; fmt=3`
(the +1.0 constant selector) and it is real.

### CORRECTIONS (CorpusWave2, 2026-09-28) — apply these before quoting earlier numbers
1. **Corpus "PASS" is STRUCTURAL only.** `run_corpus.sh`'s validate stage is `tools/validate_elf.py` (compile/asm/link/ELF structure) — it is **not** hardware execution. Of the 27/33, numerically verified this round are only `a05_scale_f32` (bit-exact vs an IEEE-float32 oracle) and `l01_silu_f32` (byte-identical to its earlier verified output). `b16_fcmp`, `l03_softmax_f32`, `b01_int_arith` are structural-only, and since `b01`/constant-division now route through `mulhu` → **`IAU.LMULH` whose semantics are UNPROBED**, they must not be counted as hardware-verified.
2. **The stale-snapshot issue is in the CLANG binary, not the assembler.** One identical `.s` assembled by all four assembler copies (live + `corpus/toolchain-snapshot{,,-b,-c}`) yields byte-identical 1172 B ELFs and passes the gate. The live/snapshot delta is the older f32-broken backend inside the snapshot clang (`SAU.ADD.F32 i2,i2,i1` live vs `i2,i2,i4` snapshot). Also: a uniform `; fmt=` rewrite over `SAU.*.F32` is **unsafe in both directions** — `x + 1.0f` is legitimately `fmt=3` (hardware selector); other f32 ops need `fmt=2`.
3. **`l05_rope_f32`'s LLVM crash was a regression from our own progress** (past ISel into the greedy RA, needing a spill), not pre-existing. `storeRegToStackSlot`/`loadRegFromStackSlot` now `report_fatal_error` instead of `llvm_unreachable`, so it reports `SHAVE: register pressure forced a stack spill, which is not implemented yet`. Functionally still blocked on frame/spill support.
4. **fp16 store lane loss (backend-measured, ~3% of lanes; 2012/65536 untouched at one chunk size)** — root cause filed by CorpusWave2: the fused `LSU.ST.16.F32.F16` source register is rewritten by a following `LDIL` (missing aliased-store pad slot, register-pressure dependent). `LSU.ST.32` copies are exact on all 131072 bytes. This is why **no bit-exact f16 elementwise kernel is claimed**; it is now the top compiler priority.
5. **Backend-measured hardware rule: SHAVE fp32 treats a denormal operand as zero and stores a denormal result as zero** (CPU computes IEEE subnormals). Pinned by 6 asserting cases in `ggml-shave-selftest`. This is the only known domain where an NPU and CPU run can legitimately disagree for a claimed op.

## 2026-09-28 (CorpusWave2, cont.) — IAU.MUL latency + IAU.LMULH is not a widened multiply

**RULE (NEW, measured): `IAU.MUL` result latency = 2 issue slots** (i.e. one
filler instruction is the minimum). Hand probe sweeping fillers between
`IAU.MUL` and its first consumer, witness `a=0xc0dec480`, `b=0xba66bd78`,
oracle `a*b = 0x2a7e9c00`:

```
k=0 fillers -> stored 0x00000000   (WRONG: consumer reads a stale/zero register)
k=1 filler  -> stored 0x2a7e9c00   (correct)
k=2,3,4     -> stored 0x2a7e9c00
```

`shave_asm._producer_latency` returns 0 for `IAU.MUL` and `SHAVEFillDelaySlots`
had no rule for it, so the selector's back-to-back `IAU.MUL; LSU0.ST.32` (what
`out[0] = a*b` compiles to) silently stores a stale register.  Landed:
`-shave-mul-latency` (default 2) in `SHAVEFillDelaySlots`.  This is the same
failure class as the measured f32 `SAU.MUL/SUB.F32` 3-slot latency, and it is
the likely cause of "a few percent of output lanes never written, irregular
indices" in kernels whose loop bound or pointer step comes out of an integer
multiply (the stale product yields a wrong/unstable bound).

**RULE (NEW, measured): `IAU.LMULH` is NOT a plain unsigned widened multiply.**
`IAU.LMULH i4,i2,i3` corrupted an unrelated register of the same 64-bit
multiply expansion (`LSU0.ST.32 i3,i1` afterwards stored garbage), and the
unsigned high half of a 64-bit product never appeared.  It is therefore NOT
selected any more; `ISD::MULHU` is a Custom lowering
(`SHAVETargetLowering::LowerMULHU`) using 16x16 partial products built only
from IAU.MUL/AND/ADD and the immediate shifts.

**OPEN DEFECT (unresolved, handed over): the compiler-generated integer
multiply is still wrong.** `out[0] = a*b` returns `0x2e014004` and
`(unsigned long long)a*b >> 32` returns `0x02738083` where the oracle is
`0x2a7e9c00` / `0x8c6f4246`, *even with the `IAU.MUL` fillers in place*.
`out[1]=a` and `out[2]=b` are exact and `0xFFFFFFFFu/(a|1u)` is exactly 1, so
the operands and the division path are fine.  This is a SECOND, unisolated
defect beyond the latency.  Until it is fixed: integer kernels (`b01_int_arith`
and any `r*` that multiplies), `LowerMULHU`, and the mulhu-dependent
constant-division path used by the fp-compare NaN test (`b16_fcmp`,
`l03_softmax_f32`) are STRUCTURAL-PASS ONLY and numerically suspect.  Repro:
`corpus-w2-probes/{mulh.c,mlo.c,mhi.c,mdiv.c}` run through
`llvm-14/tmpdump/runp3.sh`.

### RESOLVED 2026-09-28 by IntMulFix — compiler-generated integer multiply is WRONG
(see the IntMulFix section at the end of this file for the root cause: the
`IAU.SHR.U32`/`IAU.SHR.I32` immediate/register forms were mis-encoded in the
assembler database, and stores were emitted on the non-functional LSU0 unit.
The multiply instruction itself was correct.  `(a*b)>>32` is now oracle-exact.)
Original measurement by CorpusWave2, repros left in `corpus-w2-probes/` (`mulh.c`, `mlo.c`, `mhi.c`, `mdiv.c`, `mk0.s`, `mk1.s`):
- **`IAU.MUL` needs >=1 filler slot** — hand probe sweeping fillers between `IAU.MUL` and its first consumer: `k=0 -> 0x00000000` (WRONG, reads a stale/zero register); `k=1..4 -> 0x2a7e9c00` (correct). Our model had `_producer_latency = 0` for `IAU.MUL` and no rule in `SHAVEFillDelaySlots`, so the selector's `IAU.MUL; ST.32` backwards-adjacent pair silently stored a stale register. **Landed**: `-shave-mul-latency` (default 2).
- **A second defect remains**: with the fillers emitted, the compiler-generated `out[0] = a*b` still returns `0x2e014004` (oracle `0x2a7e9c00`) and `(unsigned long long)a*b >> 32` returns `0x02738083` (oracle `0x8c6f4246`), while `out[1]=a` and `out[2]=b` are exact and `0xFFFFFFFFu/(a|1u) == 1`. Operands and the division path are fine; the multiply itself is wrong.
- **`IAU.LMULH` is measured BAD** (not a plain widened multiply: it corrupted an unrelated register of the same expansion and never produced the unsigned high half) → removed from selection; `ISD::MULHU` now lowers via an **unverified** 16×16 partial-product decomposition (`LowerMULHU`).
- **Impact:** `b01_int_arith` and every integer kernel that multiplies, plus `LowerMULHU` and the `mulhu`-dependent constant-division path behind the fp-compare NaN test (`b16_fcmp`, `l03_softmax_f32`), are **structural-PASS only and numerically suspect**. Numerically verified in this round: **only `a05_scale_f32` and `l01_silu_f32`** (neither uses an integer multiply).
- **Likely explains the backend's "dropped fp16 lanes"**: kernels deriving loop bounds/address steps from integer multiplies get an unstable bound — the check is `grep -n -B2 -A2 'IAU.MUL' <kernel>.s` for a consumer within 1 slot. Owner `IntMulFix` is on it.

### Corrected verification ledger (do not overstate)
| Item | Status |
|---|---|
| add1 gate (`npu_add1_smoke.sh`) | exit 0 + oracle exact 32/32 — verified after every landed edit |
| `ctest` | 16/16 |
| Corpus 27/33 | **structural only** (compile/asm/link/validate) |
| Numerically verified on hardware | `a05_scale_f32` (bit-exact vs IEEE f32 oracle), `l01_silu_f32` (byte-identical to its verified baseline) |
| Landed but unverified | `LowerMULHU` decomposition, fp-compare numerics, all integer-multiply kernels |
| Not done | `SDIV`/`SDIVREM` + bit builtins, `GlobalAddress`/BRIND, frame/spill on SP=i19 |

---

## 2026-09-28 (IntMulFix) — RESOLVED: the multiply was fine, the SHIFTS were mis-encoded

Owner: IntMulFix.  All probe values below are `dump_probe` ACTUAL on the MTL NPU,
carrier `abs-add-1x16-tile1` (2 invocations x 8 elements).  Witness = the fixture's
default host input: the ACT delivers `a=0xc0dec480 b=0xba66bd78` for invocation 0
and `a1=0x42334011 b1=0x481e442b` for invocation 1 (`a*b = 0x8c6f4246_2a7e9c00`).

### The three defects in the integer-multiply path (all measured)

**D1 — `IAU.SHR.U32 d,a,imm` was assembled to a word that is not a shift.**  This is
the direct cause of the wrong high half of a 64-bit product.  `shave-isa/encodings.json`
carries *two* encodings of every schema-16 record: the `gen_encodings.py`
"table-derived" one (records packed bottom-up — correct only when the record list has
a single leading literal) and the `patch_encodings.py` "first-at-top" one, which is the
model validated against real vendor binaries.  For `IAU.SHR.U32` the two disagree and
**neither had corpus hits, so `resolve_form` preferred the broken twin**:

| source | bytes | word | hardware result (a=0xc0dec480) |
|---|---|---|---|
| `IAU.SHR.U32 i5,i3,16` | `c6 6e 14 18` | `0x18146EC6` (fixed `0x10000EC6`, bit 28) | `0x3037b120` = **a>>2**; imm 4 and imm 1 return `a` unchanged; imm 8 returns `0x00018bbd` — the word matches no entry in the encoding database at all |
| `IAU.SHR.U32 i5,i3,16 ; fmt=3` | `c6 6e 14 88` | `0x88146EC6` (fixed `0x80000EC6`, bit 31) | `0x0000c0de` = **a>>16** ✓, imm 8 → `0x00c0dec4` ✓, `IAU.SHL a,4` → `0x0dec4800` ✓ |

Why: the vendor record list for `IAU.SHR.U32` imm is
`[D:1][D:0][X:0][D:0][imm:5][IW:5][IR:5][D:0xEC6:13]` (**first record at the top**),
which packs to `fixed=0x80000EC6` — exactly the repacked entry.  `IAU.SHL` imm has the
same twin but its repacked form carries 22 corpus hits and therefore *won*, which is
the only reason SHL looked healthy.  `SHR_U_K` (and `SHL_K`) are now pinned with
`; fmt=3` in `SHAVEInstrInfo.td`, and the four vendor-true shift entries carry
`encode_priority: 0` in `encodings.json` (rule mirrored in `patch_encodings.py`).

**Effect:** `ISD::MULHU` is lowered by `LowerMULHU`'s 16x16 partial products, whose
every step is `IAU.SHR.U32 ...,16` or `IAU.MUL`.  With D1 the high half was garbage:
compiler-generated `(unsigned long long)a*b >> 32` returned **0xd77dfa80**
(oracle `0x8c6f4246`); `out[0]=a*b` (low half) was already correct after the landed
`-shave-mul-latency` fillers.  After the fix both halves are exact — the earlier
"`out[0]=a*b` returns 0x2e014004" observation was a measurement artifact of D3 below.

**D2 — `IAU.SHR.I32 d,a,rb` (arithmetic shift) shifted LOGICALLY.**  The assembler's
preferred form (fmt 0, `0x20000EC6`) is the wrong twin again; the vendor-true form is
fmt 2 (`0x40000EC6`).  Probe `shrs`, a=0xc0dec480, count 16: fmt=0 → `0x0000c0de`
(logical!), fmt=2 → `0xffffc0de` (arithmetic) ✓.  `SHR_S` is now pinned `; fmt=2`.

**D3 — stores were emitted on the LSU0 unit, where they do not store at all.**
Two otherwise identical kernels differing only in the unit prefix:

| kernel | bytes | ACTUAL |
|---|---|---|
| `LSU0.ST.32 i3,i0` | `82 69 06 08` (fixed `0x08000982`) | the whole output tensor is uninitialised memory (nondeterministic) |
| `LSU1.ST.32 i3,i0` | `89 69 06 01` (fixed `0x01000989`) | `80 c4 de c0` = 0xc0dec480 stored exactly, rest = carrier-init |

The vendor itself only ever stores on LSU1 (`shave_asm`'s legality pass rewrote the
prefix), but the backend *printed* `LSU0.ST.*` and depended on that rewrite.  In the
mandated `--no-legalize` probe style every compiler-generated store silently vanished,
which is why the previous owner's multiply probes read as garbage/`0x00000000`.  All
five store defs now emit `LSU1.ST.*` directly; because the legality pass rewrote them to
exactly these forms, **the assembled bytes — and the add1-fp16 ELF — are unchanged**
(verified byte-identical), and the compiler's own assembly now works with and without
the legalizer.

### Verification (all on the MTL NPU, mandated `--no-legalize` style unless noted)

`corpus-w2-probes/{mlo,mhi,mulh}.c` compiled by the fixed backend (raw assembler):

| probe | out | before | after | oracle |
|---|---|---|---|---|
| `mlo` | `out[0]=a*b` | `0x2a7e9c00` ✓ | `0x2a7e9c00` ✓ | `0x2a7e9c00` |
| `mhi` | `(a*b)>>32` | `0xd77dfa80` ✗ | `0x8c6f4246` ✓ | `0x8c6f4246` |
| `mulh` | low / high / `a*b` / `0xFFFFFFFFu/(a|1u)` | `0x2a7e9c00` / `0xd77dfa80` ✗ / `0x2a7e9c00` / `1` | `0x2a7e9c00` ✓ / `0x8c6f4246` ✓ / `0x2a7e9c00` ✓ / `1` ✓ | same |

`out[1]=a`, `out[2]=b` exact in both invocations (`0xc0dec480/0xba66bd78` and
`0x42334011/0x481e442b`), so operands and selection were always right.

- **MULHU sweep** (`mulhsw.c`, 10 host inputs x 2 invocations, operand values read
  back from ACTUAL and the high half compared against a host 64-bit oracle):
  **20/20 exact**, including the `0xf000f000^2` and `0x7bff7bff^2` carry cases
  (low-half product `> 2^32`).
- **`IAU.SHR.I32`**: compiler probe `sra.c` emits `IAU.SHR.I32 i1,i3,i1 ; fmt=2` and
  returns `0xffffffc0` for `0xc0dec480 >> 24` (a logical shift would give `0x000000c0`).
- **New corpus kernel `b27_int_mul`** (added this round, honours
  `invocation.element_count` so it fits the 32-byte porting-probe carrier): computes
  high half, low half, 32-bit product and `0xFFFFFFFFu/(b|1u)` per invocation.  Built
  by `run_corpus.sh` (structural PASS, 720 B text) and **numerically verified** against
  a host oracle: 24 host inputs x 2 invocations = **48/48 exact** (the host oracle's
  ramp model was itself validated against a word-copy probe: model == ACTUAL for both
  invocations on every input).  This is the permanent numeric regression for D1/D2/D3.

### Candidates from the ticket, checked and cleared
- **`IAU.MUL` operand form**: the register form is a plain 32x32→low-32; `IAU.MUL rd,ra,rb`
  with rd the destination is confirmed by the hand probe (`mk1.s`) and by the compiler
  bytes matching it.  `out[0]=a*b` is exact for both invocations.
- **Register form**: MUL destinations read back correctly from i5, i16, i17, i20, i25, i31
  (and i20 works as a source).  `i21` and `i24` fault — that is the known register-file
  trap set `{i19,i21,i24,i30}` (FINDINGS line 107), *not* a multiply limitation.
- **Latency rule**: re-measured. `IAU.MUL` + 0 fillers → stale `0x00000000`; 1..4 → exact.
  `SHAVEFillDelaySlots` emits 2 `BRU.NOP`s; they are present in the failing kernels' `.s`.
- **No other instruction clobbered in the bundle** (the multiply bundle is one IAU op
  plus fillers); the clobbering came from D1/D2/D3 above.

### Probe-harness hazard found while measuring
Both invocations write the *same* output tensor (invocation 0 at +0, invocation 1 at
+16 for the 1x16 carrier).  A probe that writes more than 16 bytes per invocation has
its upper half overwritten by the sibling invocation; the winner is deterministic here
(invocation 0 lands last) but must not be relied on — keep probe stores <= 4 words.

### Still open (not touched)
- ~1000 more database mnemonics have the same "mangled table-derived twin preferred over
  the repacked one" ambiguity (2242 ambiguous (mnemonic, operand-kind) pairs).  Only the
  IAU shift family was fixed here, because it is the one the backend actually emits with
  an immediate and the only one measured wrong.  Re-deriving `fixed_bits` for all forms
  from the vendor records (`patch_encodings.pack_top`) with corpus hits intact is the
  real root fix and should be a separate, fully re-verified task.
- The high-register trap set: the RA still allocates i21/i22/i23/i24 in
  corpus kernels (`build/intmul4.*.s`); untouched, separate defect class.

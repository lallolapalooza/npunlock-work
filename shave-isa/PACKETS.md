# PACKETS — slot/delay/NOP model as IMPLEMENTED in shave_asm.py

Full derivation with annotated vendor decompile excerpts: ANNOTATED_RULES.md.
Vendor rule list (compiler + assembler side): delta/ANNOTATED_DELTA.md R1-R21.
Hardware measurements: packets/PACKET_SPEC.md, FINDINGS.md, VALIDATION.md here.
This file = what `shave_asm.py asm|obj` does **now**, automatically.

## The legality pass (D1-D4)

Runs inside `asm`/`obj` before encoding (`legalize_text`), on the tokenized
item stream.  One op per line = one issue packet; `a || b || c` = one VLIW
pipe (<=3 ops, <=128 bits, <=1 op per unit over
`BRU CMU IAU LSU0 LSU1 PEU SAU VAU`).

| rule | what it does | switch |
|---|---|---|
| **D1 branch delay** | **unconditional** branch (`BRU.JMP/SWIH/SWIC/SWP/RPI/RPL/RPS/RFB*`) must be followed by 6 slot-consuming instructions; missing slots are filled (one compressed byte `0x50` by default).  A leading explicit filler run (`BRU.NOP`/`BRU.INC` lines, `.byte 0x07`, or compressed `(n-1)<<4` bytes) of >= 6 slots makes it a no-op, so the pass is idempotent; a shorter run is topped up. | `--delay-slots N` (0 off), `-forceDelaySlots:<n>` |
| **D1b conditional descriptor** | `BRU.BRA` only transfers with a delay descriptor right after it; the second descriptor byte is a compressed NOP covering `6-k` slots for the `k` real instructions that execute inside the window.  **Effective combination today:** the backend (`SHAVEFillDelaySlots`) emits `.byte 0x1b, 0x40` fwd / `0x2b, 0x40` bwd => k = 1 real delay instruction.  The assembler's backstop fires only when the source has NO descriptor and then writes `1b 50` / `2b 50` => k = 0 (immediate).  Descriptor bytes are never doubled; `; desc=<hex>` supplies vendor variants per branch. | `--bra-descriptor` (default on), `--no-bra-descriptor` |
| **D2 load latency** | dataflow-aware: the first consumer of an LSU memory load's destination (LDO*/LD*/LDXV*, LDIL/LDIH excluded) must be >= 6 issue slots later; fillers are inserted only for the shortfall.  Slot counting includes everything that issues: real instructions, NOPs, `.byte 0x07` runs (a `.byte` run of anything else, and a new `.section`, reset the analysis). | `--latency N` (0 off), `-forceLoadDelay:<n>` |
| **D2b DIV latency** | a `SAU.DIV*` result needs **13** slots before its first consumer (measured: k<=10 stale, 11/12 fault, >=13 correct); dataflow-aware, same machinery as D2. | `--div-latency N` (0 off) |
| **D2d SAU.ADD.F32 latency** | a `SAU.ADD.F32` result needs >= 2 slots before its first consumer (gap 0/1 reads the stale pre-add value; measured by ResultingBear).  The third operand is a hardware constant selector, not a GPR. | `--fadd-latency N` (default 2) |
| **D2c LDI pair** | an `LDIL`+`LDIH` pair (same register) needs >= 6 slots before the next LSU access (x4/x5 fault).  Backend-owned today: the assembler only **warns**; `--ldi-pair-pad` makes it pad. | `--ldi-pair-pad` |
| **D3 LSU units** | loads prefer **LSU0**, stores prefer **LSU1** (`shave-lsu-load/store-policy`).  `prefer` (default) rewrites the unit prefix to the structurally equivalent form of the other unit and pins it with `; fmt=`; `error` refuses a non-preferred unit and any remaining same-unit load/store pair; `off` disables.  A surviving same-unit load/store pair is reported ("LSUx unit is used more than once in the same pipe window") because the silicon corrupts it. | `--lsu-policy {prefer,error,off}`, `--lsu-window N` (0 = whole section) |
| **D6 high load destination (warning)** | a memory-load destination > i15 is warned: VendorRE2's direct sweep shows **{i19, i21, i24, i30} FAULT** in the 5-byte `LSU0.LDO.32` form while i16/i17/i18/i20/i22/i23/i25..i29/i31 execute (i25 verified loading correctly), and the 2-operand `LSU0.LD.32` form traps for i21 / silently writes nothing for i25.  LDI forms tolerate high registers.  Register allocator constraint: load destinations must be r0..r15. | always on (warning only) |
| **D5 zero-offset load** | `LSUx.LDO.W d, base, 0x0` is rewritten to the silicon-verified 2-operand `LSUx.LD.W d, base` (measured: `825d2401` for `LD.32 i5,i18` loads correctly; the vendor `LDXV` shape as our tables encode it does NOT write its destination; applied only for destinations <= i15 because the LD form is unsafe for high registers). | `--zero-offset` (default on), `--no-zero-offset` |
| **D3b load write-back isolation** | an LSU op 1..5 slots after a memory load silently discards that load (measured: `LSU0.LDO.32` slots 4-5, `LSU0.LD.32.F16.F32` slots 0-6); the gap is topped up to 6 slots with BRU-unit fillers. | `--lsu-policy off` disables |
| **D4 aliased store** | a store followed by an aliased load (same base register, other LSU unit, 0 slots between) gets `shave-aliased-store-distance` (1) filler slot. | `--aliased-store-distance N` |

Defaults (round 2, Main directive): compressed one-byte filler for gaps >= 2
slots (`(n-1)<<4`; 6 -> `0x50`), conditional-branch descriptor safety net
(`1b 50` fwd / `2b 50` bwd = immediate k=0) that fires only when the backend
did not already emit one, and `--div-latency 13` for `SAU.DIV*`.
`--no-compressed-nops` and `--no-bra-descriptor` restore the plain behaviour
(n x `0x07`, warn-only); `--exact`/`--no-legalize` disable the whole pass.
Net effect on the current backend stream (`/tmp/mm/FINAL/ours.s`): zero pads,
zero rewrites, zero warnings - the backend already satisfies every rule, and
its descriptors keep k = 1 for taken conditional branches.

Verbatim mode: a disassembly listing (`raw=` hints) is re-encoded
byte-for-byte with **no** padding/rewrites — that is the 192/192 gold
roundtrip path.  `--exact` / `--no-legalize` request the same for any input.
Vendor compression switches (`--noCompressedNOPs`, `--noFinalSlotCompression`,
`--noSlotAllPromo`, `-no6thSlotCompression`, `-a`) are accepted and ignored:
we never compress NOPs (plain 0x07) and never promote instructions into delay
slots, so "compression off" is already our behaviour.

## Flag surface (asm + obj)

```
--latency N            required load->consumer slot distance   (default 6, 0=off)
--div-latency N        required SAU.DIV* ->consumer distance    (default 13, 0=off)
--delay-slots N        fillers after an unconditional branch   (default 6, 0=off)
--lsu-policy P         prefer | error | off                    (default prefer)
--lsu-window N         same-unit load/store window             (default 0 = section)
--aliased-store-distance N                                    (default 1)
--bra-descriptor       emit 1b 40 after a descriptor-less BRU.BRA (default off)
--ldi-pair-pad         pad the LDIL/LDIH pair before an LSU op  (default off)
--exact / --no-legalize  pure encoder (no padding, no rewrites)
-forceDelaySlots:<n> / -forceLoadDelay:<n>   vendor spellings (honoured)
```

## Implemented + verified (this session)

1. VLIW bundle `slot0 || slot1 || slot2` — concatenated bytes, rejected when it
   exceeds 128 bits, has >3 slots or repeats a unit (the vendor's
   "unit is used more than once in the same pipe"); enforced in `assemble()`
   and in the legality pass.
2. `--latency N` is now dataflow-aware (it was "N NOPs after every load"):
   distance 1 -> padded to 6; distance 6/12 -> untouched; unconsumed load ->
   untouched.
3. Gates: gold disasm->asm roundtrip **192/192 IDENTICAL**; 13/13 mlibm .o
   roundtrips byte-identical; 39 encode anchors; `gates.py` 128/128.
4. Hardware (runp3/runp4 + dump_probe, MTL NPU): a naive kernel
   `LSU0.LDO.32 i0,i18,0x28 / LSU0.ST.32 i0,i0 / BRU.JMP i30` assembled with
   the default legalizer yields the *byte-identical* stream of the hand-written
   working recipe k1 and reproduces its window (`00 40 01 2e` / `10 40 01 2e`);
   with `--exact` the same source faults (no write) - failing-before /
   passing-after on hardware.
5. `LSU1.ST.16.F32.F16 i3,i0` (the D3 rewrite target, bytes `49 34 00 00`)
   writes the same value as the k3b recipe's `LSU1.STI.16.F32.F16 ;;
   fmt=6` (`49 34 00 08`), 2 runs each; the previously default hintless pick
   (`fmt=0`, bytes `20 39 00 00`) **faults** (2 runs) - hence the `cal_fix.py`
   pin that keeps that store off the hintless path.

## Vendor model (unchanged, rules, ANNOTATED_RULES.md has the code evidence)

- Instructions <= 40 bits; byte stream is bit-packed; packets <= 128 bits.
- After a branch the assembler fills up to `shave-branch-delays` = 6 slots by
  promoting instructions (SLOT_ALL) or inserting NOPs (BRU.NOP = 0x07;
  compressed forms pack into partial bytes).  We only emit plain 0x07 NOPs:
  the compressed-NOP byte encoding (gold's `1b 40`/`2b 40`/`eb 30`/`1b 50`)
  is not recovered; `1b 40` after a conditional branch is its delay
  *descriptor* and is emitted by the backend.
- Load latency = 6 cycles, branch delay = 6 slots, aliased-store-distance = 1
  (all `shave-*` options, defaults confirmed on hardware).
- LDIL -> LSU0, LDIH -> LSU1, same pipe where possible.

## Pipe/slot formation: what it can and cannot do (measured)

Main's hypothesis was that the assembler must perform the vendor's
pipe/slot formation because "identical instruction sequences change behaviour
with the surrounding bytes".  Measured, on this assembler:

* **Grouping is byte-invisible.**  Two sources with the *same* instruction
  sequence, one flat (one op per line) and one grouped into 3-slot pipes,
  assemble to the **same bytes**:
  `c20264510007c620a50441f00107c600210441e0018909000147f101070707070707`
  (encoder concatenates members in slot order; R6).  Their hardware ACTUALs
  are identical too (`00 40 01 2e` / `10 40 01 2e`).  => implementing
  "real pipe formation" as a *text* transform cannot change kernel behaviour;
  only byte-level padding/alignment could.
* **Compressed filler works on hardware**: the naive kernel assembled with the
  compressed default (`c202645100 50 89090001 47f101 50`) and k3b with the
  compressed default both give the same ACTUALs as their uncompressed
  counterparts.
* **Position and mix do not flip the k1/LDO+ST recipe.**  Store at byte 12
  with 7 filler slots, and store at byte 12 with a different filler mix
  (5x `BRU.NOP` + 4-byte `IAU.ADD`), both give the same correct ACTUAL.
* Therefore the residual kernel failure is most likely *encodings/ops we do
  not model* (gold's 5-byte `1b 50 e9 9d 24 | 0182645100 | 3086c4a802 | 07`
  region, byte0 class fields {01,c2,f9,fa,f0}, unmodeled descriptor-bearing
  instructions), not packet grouping.  Tracked with AsmEncodingRE /
  VendorRE2; the assembler does not silently reinterpret those bytes.

Padding pipes to 8/16-byte boundaries is NOT implemented: no measurement
supports it (C2: 1-4-byte pre-shifts pass; G2/G3 above), so it would be
speculative byte bloat.  If a future measurement demands it, the natural place
is a `--align-pipes` switch in `legalize_items`.

## Diagnostics (never change bytes)

* dropped `; fmt=N` hint (operand-shape mismatch) - G10
* load destination > i15 (pattern legality) - D6
* 0x42-class 2-operand `LSUx.LD.8` (measured non-working) - use the displacement
  form or widen+mask
* missing conditional-branch descriptor / LDIL-LDIH pair too close to an LSU
  access / same-unit LSU load-store pair

## obj writer == asm writer (round 3 fix)

`assemble_object()` used to resolve forms WITHOUT the operand-kind filter and
without expanding `[iN+off]`, so `IAU.ADD d,s,imm` picked the register-only
form and dropped the `0x80000000` fixed bit (asm `c6202581` vs obj
`c6202501`).  Fixed; `gates.py` G9 now asserts byte equality between the two
writers for the AU-immediate anchors and for every mnemonic line of the current
compiler output (262 lines, 0 mismatches).  Any object built before this fix
mis-executed AU-immediates on the NPU.

## Pending / open

- Vendor compressed-NOP byte encoding (what `2b 40` / `eb 30` / `1b 50` /
  `eb` mean, and which descriptor variant each branch form needs) — needed for
  byte-parity with gold's delay-slot padding.  Requested from VendorRE2.
- LDO byte0 class field (`01/c2/f9/fa/f0`) semantics — the rewrite paths avoid
  touching it; only corpus/HW-anchored forms are picked.

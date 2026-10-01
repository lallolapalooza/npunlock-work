# DIFF.md — op-by-op alignment of OUR emitted add1-fp16 stream vs GOLD, and the root cause of `count == 0`

Owner: StrangeSole (diagnosis only; fixes routed to MedicalCrayfish / AsmSchedImpl).
All evidence below is shell-invoked hardware evidence through
`tmpdump/runp3.sh` (ELF → carrier patch → bundle → `dump_probe` ACTUAL) and
`npunlock/tools/npu_add1_smoke.sh --elf` (oracle). Only ACTUAL/EXPECT bytes and
exit codes are used (no trap channels). 2–3× per data point where it matters.

---

## 0. TL;DR — the answer

**Two independent divergences, either one of which alone produces the symptom
(zero writes / `count` never accumulating; `count` reads 0 or 1).**

| # | Divergence | Effect on our kernel |
|---|---|---|
| **A** | A **forward** conditional branch emitted with descriptor `1b 40` branches **iff the CMU flag source register == 0** — the *inverse* of the backend's contract (`SETCC32`/`LowerBR_CC` deliberately produce "non-zero == condition true"). `2b 40` is the non-inverted (== "!=") form, in **both** directions (direction is irrelevant to the sense). | Every forward guard fires on the **false** condition → the entry `rank` guard returns immediately → **zero writes**. `t2` (verbatim guard-1 sequence of our kernel, DIV result measured = 0) branches to the exit. |
| **B** | A **same-license LSU op issued inside a load's visibility window silently discards the load's write-back**; the destination register keeps its **previous** value. For `LSU0.LDO.32`: LDIL at slot 4–5 after the load ⇒ lost (slots 0–3, 6, 8 ⇒ kept). For `LSU0.LD.32.F16.F32`: LSU0.LDIL *or* LSU1.LDIH at slots 1,2,6 ⇒ lost. | The backend's `fixLoadLatency` **counts the `LSU0.LDIL`/`LSU1.LDIH` constant pairs it emits as legitimate latency fillers**. The dims deref `LSU0.LDO.32 i2,i2,0x0` + 4 NOPs + `LDIL i4,0`/`LDIL i5,1` therefore *keeps the address* `0x80000050` instead of `dim = 8`. Guard 4 then computes `0x80000050 / 17 == 0x0787878C` (exact) ⇒ returns ⇒ **`count` is never accumulated**. |

Both fixes together (in the `.s`) make the kernel compute the correct
`count = 8` at the fp-loop preheader (witness `k3cnt`), reach `LBB0_8` with the
correct out/in pointers (`k3stop`), and pass every guard with its intended
sense. A **third** wall remains in the fp loop itself (§6) — it is *not* the
`count` bug.

---

## 1. Method / artifacts

Commands (all from `/home/asdf/projects/llvm-14/tmpdump`, outputs in
`loopdiff/out/<name>-<tag>/`):

```
OUT=/home/asdf/projects/npunlock-work/loopdiff/out ./runp3.sh <file>.s --tag <t>
# plus: /home/asdf/projects/npunlock-work/npunlock/tools/npu_add1_smoke.sh --elf <elf>

# our straight-line dump:
clang -cc1 -triple shave -target-cpu 3720xx -S -O2 -x c .../add1-fp16.c -ffunction-sections -fdata-sections -o -   → ours.s
python3 shave-isa/shave_asm.py obj ours.s -o ours.o ; ld.lld ... ; shave_asm.py asm ours.s --hex
```

* `ours.s` = current backend output (283 lines). `ours.s`/`fix_pol.s`/
  `fix_r.s`/`fix_both.s`/`w_count.s`/`trace*.s`/`k*.s`/`m_*.s`/`sw*.s`/… in this dir.
* Carrier-init baseline (`empty` kernel) = `00 01 00 00 ff ff ff 00 00 00 80 3f
  00 00 00 bf 20 01 00 00 ff ff ff 00 00 00 80 3f 00 e0 dd be` — every
  “our kernel writes X” reading in earlier iterations must be compared to this.
  **`ours.s` (current backend) writes NOTHING** (ACTUAL == carrier-init, i.e. the
  24.3 “one element” reading was the baseline artifact).
* Gold (`tests/fixtures/npu3720/add1-fp16.elf`, unmodified): oracle **PASS**,
  ACTUAL == EXPECT (run in this session, rc=0).

Witness recipe that works (verified, `w0`/`w3`): `LSU1.ST.32 <reg>, <outptr>`
lands; to log several values, advance the base with `LSU0.LDIL r,4` +
`IAU.ADD base,base,r` (works at distance 1). `LSU1.ST.32 a,b,0x4` (5-byte
offset form) **faults** — do not use it.

---

## 2. Our stream ↔ gold's stream, region by region

Gold addresses are `packets/PACKET_SPEC.md` §11; ours are the `.text` byte
offsets of the assembled stream (`shave_asm.py asm ours.s --hex`).

| role | GOLD (vendor) | OURS (backend, current) | verdict |
|---|---|---|---|
| rank load | `c2a2641900` LDO i10,i18,0xc @0x00 | `c202641900` LDO i0,i18,0xc @0x0e | same form, class c2; **6 NOPs after** ⇒ *kept* (t2: rank=4 ✓) |
| dims-ptr load | `c272642100` LDO i7,i18,0x10 @0x05 | `c212642100` LDO i1,i18,0x10 @0x49 | **only 4 NOPs then `LDIL i2,0`+`LDIL i3,1`** ⇒ **load lost** (bug B, `t_h`/`sw4/5`) |
| dims deref | inside gold's unresolved 0x0b–0x49 run | `c222440100` LDO i2,i2,0 @0xa3, then 4 NOPs + `LDIL i4,0`+`LDIL i5,1` | **load lost**, `i2` keeps the **address** `0x80000050` (trace7 witness) |
| rank>15/`!dims` guard | 0x3a–0x56: `CMU.CMII.I8`, `CMU.CMZ.I32 i5`, `BRA i5→0xb7` `1b 40`, … | division formulations + `CMU.CMZ` + `BRU.BRA` `1b 40` | shape differs (backend's own lowering, corpus C4/C6-follow-up) but **semantically fine once A+B are fixed** |
| dims-loop header | ~0x4a (`LDO i5,i7,0x7ffd`) | LBB0_3 @0x8e | ours is DIV-guarded (§3) |
| per-dim guards | `CMU.CP.32 i6,i9` / `SAU.DIV.U32 i9,i8,i6` / NOP13 / `CMU.CMII.U32 i5,i9` (0x59–0x62) | `LDIL i4,0x10` / `SAU.DIV.U32` / 13 NOP / `LDIL i5,1` / `ADD` / `SAU.DIV.U32` / 13 NOP / `CMU.CMZ` (0xdc–0x109) | **13-slot DIV padding present ✓** (R-E); guard *value* is wrong only because of the lost dim load |
| `count *= dim` | `IAU.LMULL i10,i6,i5` @0x93 | `IAU.MUL i4,i2,i1` @0x120 folded into `IAU.AND i1,i4,i4` @0x8e | correct in form; multiplies the *stale* `dim` |
| dims-loop counter | `IAU.INCS i10,-1` @0x6c | `LDIL/LDIH i5=0xFFFFFFFF` + `IAU.ADD i0,i0,i5` @0x131, `CMU.CMZ`+`BRA` `2b 40` → LBB0_3 | works (back-edge taken; trace3/trace7 show 4 iterations) |
| out-ptr load | `01 82645100` LDO i8,i18,0x28 @0x8d (class **0x01**) | `c202645100` LDO i0,i18,0x28 @0x180 + **4 NOPs + a second LSU0 load** | **load lost** (bug B) ⇒ store base was stale (that is the source of the `0x70000001` fault in `fix_both`) |
| fp16 load | `42701200` LD.32.F16.F32 i7,i9 @0x98 | `42200600` LD.32.F16.F32 i2,i3 @0x197 + `LDIL i4,0`+`LDIH i4,0x3F80` at slots 1,2 | **load lost** (bug B, `fpl`/`fpl2`) ⇒ `i2 = 0` at the add |
| fp add | `3ce09c30` SAU.ADD.F32 i7,i7,i1 @0xaa | `3c400832` SAU.ADD.F32 i2,i2,i4 | encoding for gold's exact operands is **byte-identical** (`SAU.ADD.F32 i7,i7,i1` → `3ce09c30`) but the `b` operand does not read the named register (§6) |
| fp store | `49741008` LSU1.STI.16.F32.F16 i7,i8 @0xaf | `49240000` LSU1.ST.16.F32.F16 i2,i0 @0x1ac | field map verified (value@bits16:12|bit8<<4, base@bits21:17) |
| return | `47f101` BRU.JMP i30 @0xb7 | `47f101` @0x1e8 | same |

---

## 3. Divergence A — branch-descriptor polarity (measured 2×, 4-cell matrix)

Kernels differ **only** in one byte: `LSU0.LDIL i1,0x0000` vs `0x0005`
(verified by byte-diff: sole difference at byte 27). Construction:
`… LDIL i1,X; 6 NOP; CMU.CMZ.I32 i1; BRU.BRA i1,Lbad; .byte d0,d1; 6 NOP;
<store 0x600D0001>; <store 0x600D0002>; JMP … Lbad: <store 0xBAD0BAD1>; JMP`.

| X (flag source) | descriptor `1b 40` | descriptor `2b 40` |
|---|---|---|
| 0 (`IAU.XOR i1,i1,i1`) | **TAKEN** (`d1 ba d0 ba`) | not taken (`01 00 0d 60`) |
| 4 (`LSU0.LDO i1,i18,0xc`) | not taken | **TAKEN** |

Files/ELF: `m_1b_z`, `m_1b_nz`, `m_2b_z`, `m_2b_nz` (also `v1`/`v2` with LDIL
sources). Reproduced twice, deterministic.

⇒ `1b 40` = branch iff flag **== 0**; `2b 40` = branch iff flag **!= 0**.
The byte is a **polarity** selector, *not* a direction selector. Gold's own
cross-map is consistent: `CMU.CMZ.I32 i5` + `BRU.BRA i5,0xb7` + `1b 40` @0x50/0x53
("!= 0 → return"-style guard reads naturally as `== 0`).

### In-context proof (`t2.s`)
Verbatim copy of our kernel's entry guard (LDIL/LDIH −1, rank load, `(rank−1)/15`,
13 NOPs), then witnesses before the CMU:

```
ACTUAL[0:4]  = 00 00 00 00   DIV result = 0        (correct)
ACTUAL[4:8]  = 04 00 00 00   rank = 4              (correct)
ACTUAL[8:12] = 0f 00 00 00   divisor = 15          (correct)
ACTUAL[12:16]= d0 ba d0 ba   the branch was TAKEN  <-- WRONG (0 means "do not return")
```
⇒ with the current backend output, `ours` returns at the very first guard on
every legal input ⇒ **zero writes**, `count` never computed.

### Fix (verified)
In `ours.s`, replace all five forward `.byte 0x1b, 0x40` by `.byte 0x2b, 0x40`
(loop backedges already use `2b 40`): `fix_pol.s`. All guards then fall through
for legal input; the kernel advances to the dims loop (`w_count.s` still shows
nothing, because of B — see §4).

**Owner:** backend (`SHAVEFillDelaySlots.cpp` emits `BR_DELAY`/`BR_DELAY_BWD`
by *direction*). Rule: choose the descriptor byte by **condition polarity**,
not by direction; `2b 40` for "branch when the flag is non-zero / condition
true", `1b 40` for the inverted sense. Or drop the SETCC inversion and emit
matching descriptors — the assembler must not confuse the two.

---

## 4. Divergence B — a same-unit LSU op inside a load's window discards the load

### 4.1 Minimal, deterministic sweep (`sw0..sw8.s`)
```
LDO i13, i18, 0x28 ; 6 NOP                      ; cursor = out ptr
LDIL i1,0x3344 ; LDIH i1,0x1122 ; 6 NOP         ; SEED 0x11223344 in i1
LDO i1, i18, 0x10                               ; the load under test
NOP x k
LDIL i2,0x00 ; LDIL i3,0x01
6 NOP
LSU1.ST.32 i1, i13                              ; witness i1
```

| k (NOPs between load and the LDIL pair) | witness i1 | meaning |
|---|---|---|
| 0,1,2,3 | `50 00 00 80` = 0x80000050 | load value kept |
| **4,5** | `44 33 22 11` = **seed** | **load write-back DISCARDED** |
| 6,8 | `50 00 00 80` | kept |

(`t_h` = k=4, 3× deterministic; `t_i` = k=6, correct; `u3`/`u4`/`u5` show the
kill is *positional* — 3 NOPs or 6 NOPs before the pair is safe, an LDIL at
slot 4/5 is not; a lone LDIL, two LSU1.LDILs, and two IAU fillers were checked.)

### 4.2 It is not "6 slots of general padding" — the filler *class* matters
`u2` (4 NOPs + 2× `IAU.XOR`) keeps the value; `sw4/5` (4–5 NOPs + LSU0 LDILs)
loses it. So the backend's `fixLoadLatency` ("≥6 slots") is satisfied in both
cases, yet the load result differs ⇒ **the fillers must not be same-license LSU
ops; LDIL/LDIH do not count as latency fillers.**

### 4.3 Load-class dependence (fp16-converting load is worse)
`LSU0.LD.32.F16.F32 i2,i3` + `LSU0.LDIL i4,0` at slot 1 → kept (`fpl1`);
+ `LSU1.LDIH i4,0x3F80` at slot 1 → **lost** (`fpl2`);
+ `LSU0.LDIL` at slot 6 → **lost** (`fpl4`).
⇒ the FP16-converting load needs a longer, LSU-free window than the integer load.

### 4.4 In-context effect (our kernel)
* `trace5.s` (= `fix_pol` + slot-based guard-value wheels on out[0..15]):
  guard-2 value (`1/(dims+1)`) reads **1**, i.e. `dims` read as **0**.
* `trace6.s` (= `trace5` + 6 fillers before the LDILs of the dims-*pointer*
  load): guard-2 = 0 ✓, guard-3 = 0 ✓, **guard-4 = `8c 87 87 07` =
  `0x0787878C`**.
* `trace7.s` (= `trace6` + witness of `i1`,`i2` at `LBB0_4`):
  `i1 = 1` (count, correct for iter 1) and **`i2 = 0x80000050`** — the dim
  deref returned *the address*, not `dim = 8`.
  `0x80000050 / 17 = 0x0787878C` — **exactly** guard-4's value ⇒ arithmetic
  proof of the causal chain: lost load ⇒ `dim` = pointer ⇒ `dim > 16/count`
  guard fires ⇒ return ⇒ `count` stays 1 ⇒ the fp loop runs once.
* Fixing the three killer loads in the `.s` (dims pointer, dim deref, out ptr):
  `k3cnt.s` = **`i1 = 8` (count) and `i2 = 1`** at the fp-loop preheader;
  `k3stop.s` = out `0x2E014000`/`0x2E014010`, in `0x2E014340`/`0x2E014350`.

**Owner:** backend (`SHAVEFillDelaySlots.cpp` `fixLoadLatency`, plus the
emission order of `LSU0.LDIL`/`LSU1.LDIH`). Rule: a load must be followed by
≥6 **non-LSU** issue slots (BRU.NOP / IAU / SAU), and for
`LSU0.LD.32.F16.F32` by ≥7 — constant materialization must be hoisted above
the load or pushed past the window. The assembler (`AsmEncodingRE`) should
*warn/reject* when an LSU-license op lands in slots 0–5 of a load (a legality
check, not a reorder).

---

## 5. Which fix moves the needle (experiment ladder)

| # | .s variant | change | ACTUAL[0:4] | conclusion |
|---|---|---|---|---|
| 0 | `ours.s` | — | carrier-init | zero writes |
| 1 | `fix_pol.s` | 5× `1b 40`→`2b 40` | carrier-init | A alone un-inverts guards but B still returns at guard 2/4 |
| 2 | `w_count.s` (+A) | store `count` at fp preheader | nothing | never reaches the preheader |
| 3 | `trace5/6/7` | +A, guard/dim witnesses | guard4 = 0x0787878C; dim = 0x80000050 | B isolated in context |
| 4 | `ib_g2`, `ib_g3`, `ib_l7` | +A, fix one load each | carrier-init, executes | each individually safe |
| 5 | `ip_g2_g3`, `fix_both`, `fix_r` | +A, all loads fixed | **FAULT 0x70000001** | the fp loop (below) now executes with a stale operand |
| 6 | `ip_g2g3_stop.s` | +A, g2+g3, return at `LBB0_5` | **8** | dim deref correct ⇒ chain proven |
| 7 | `k3cnt.s` | +A, g2+g3+l7, stop at `LBB0_8` | **8** (count) / 1 (last dim) | **`count = 8` recovered** |

Repro of the failure before the fix (smallest): `t2.s` (guard polarity) and
`sw4.s` (load loss); repro *in the real kernel*: `trace5.s` (guard-2 = 1),
`trace7.s` (dim = dims pointer).

---

## 6. Remaining wall (reported, NOT the `count` bug)

### 6.1 Bisection — the loop-top LOAD across iterations
Using the current (fixed) backend output with the fp-loop decrement patched to a
*terminating* 2-iteration count (`ours2_4.s`, decrement −4: 8 → 4 → 0):

| variant | loop body | 2 iterations |
|---|---|---|
| `ours2_cnt.s` | loop replaced by a `count` witness | executes, **count = 8** |
| `ours2_nomem4.s` | load+add+store all NOP'd (witness `ST.32 i1`) | **executes** (iter1 stores 8, iter2 stores 4 → the counter/back-edge machinery is sound) |
| `ours2_4_noload.s` | store only | **executes** (store lands) |
| `ours2_4_nostore.s` | load only | **FAULT** |
| `ours2_4_intload.s` | `LSU0.LDO.32 i2,i3,0` instead of the F16 load | **FAULT** |
| `ours2_4.s` | load + add + store | **FAULT** |
| `ours2_l1.s` / `ours2_first.s` | 1 iteration, full body | executes, real fp16 stored (`80 c4` / `11 40`) |

⇒ the fault is the **32-bit load at the loop top when the loop iterates ≥2
times** (not F16-specific, not the store, not the counter, not the pointer
advance — `ldila.s` proves `LDIL i2,0x02` → `IAU.ADD i12,i12,i2` at distance 1
yields exactly +2). Hand-off: reproduce with `ours2_4_nostore.s`.

### 6.2 `SAU.ADD.F32` does not read its `b` operand
`2.0f + 3.0f` → `0x402D413D` (= 2.0 + 0.70710678), `1.0f + 0.0f` → `0x3FDA827A`
(= 1.0 + 0.70710678) — `fa1b.s`/`fa2b.s`. The constants themselves are correct
(`fa1`/`fa2` show `LDIL+LDIH` yields 0x40000000 / 0x3F800000). Gold's own
operand triple encodes byte-identically (`SAU.ADD.F32 i7,i7,i1` → `3CE09C30` ==
gold 0xaa), so this is a **field/b-unit decode divergence** (b reads as 1/√2 in
both probes).

### 6.3 Load-window: layout sensitivity caveat (honest)
The `LSU0.LDO.32` sweep in §4.1 is clean and monotone in a fixed layout, and the
conservative rule (no LSU op anywhere in the window, use non-LSU fillers) is
validated by the backend fix (count = 8). However the **F16.F32 family did not
reproduce a clean slot boundary**: with a seeded destination (`f16_{s,x}{3..9}.s`,
14 runs, both units) an `LSU0.LDIL`/`LSU1.LDIH` at slots 3..9 **never** loses the
value, while the *unseeded* `fpl2`/`fpl4` layouts (same slot positions, different
surrounding bytes) did. So the effect is partly packet/layout-dependent; keep the
"no LSU op in the window" rule (it costs nothing) rather than a slot number.

---

## 7. Routing

* **MedicalCrayfish (backend)**:
  1. emit the branch descriptor by **polarity** (`2b 40` = "condition true"),
     never by direction; `SHAVEFillDelaySlots.cpp` `fillBranchDelaySlots`.
  2. `fixLoadLatency`: do **not** count `LSU0.LDIL`/`LSU1.LDIH` (or any LSU-op)
     as load-latency fillers; require ≥6 non-LSU slots (≥7 for
     `LSU0.LD.32.F16.F32`); prefer hoisting the constant pair above the load.
* **AsmSchedImpl / AsmEncodingRE (assembler)**:
  1. add a legality warning for an LSU-license op at issue-distance 0–5 after a
     load (same behavior as the existing LDIL→LSU and LSU0/LSU1 conflict checks);
  2. audit `SAU.ADD.F32`'s `b`-operand field (gold `3CE09C30` parity holds for
     `(i7,i7,i1)` but b is read as 1/√2 for other registers — see `fa1b`/`fa2b`);
  3. **crash repro**: `shave_asm.py asm ours2_4.s` raises
     `UnboundLocalError: cannot access local variable 'load_ctx'` at
     `legalize_items`/`bump` (the new D3b load-window legalizer) — file is
     `loopdiff/ours2_4.s` (each `ours2*.s` variant in this dir is a runnable
     input; `ours2.s` and `ours2_cnt.s` assemble cleanly with the same code).

---

## 8. Post-fix verification against the *current* backend build (this session, after
MedicalCrayfish landed A+B and AsmSchedImpl landed D3b)

`ours2.s` = fresh `clang -cc1` dump of the fixture with the landed fixes.
Descriptors are now `1b 40` on **every** conditional branch, and the branch
*condition values are inverted* in the IR lowering (e.g. the dims back-edge is
`i5 = 1/(counter+1)`; `1b 40` branches iff the value == 0 ⇒ loop while
`counter != 0` ✓). Load windows: dims-ptr 6 NOPs, out-ptr 6 NOPs, fp16 load 7
NOPs before the `LDIL/LDIH`.

| check | file | result |
|---|---|---|
| count at the fp-loop preheader | `ours2_cnt.s` | **i1 = 8** (ACTUAL[0:4] = `08 00 00 00`), both invocations |
| one fp iteration | `ours2_first.s` | executes; `ST.16.F32.F16` lands a real fp16 (`80 c4` / `11 40`, inv1/inv2) |
| full kernel | `ours2.s` | execution **FAULT** `0x70000001` (unchanged symptom class) |

⇒ both root causes are confirmed *fixed in the emitted stream* (guards take the
intended sense; the dims loop accumulates `count = 8`). The residual fault is in
the fp-loop path (fp16 load window / `SAU.ADD.F32` b-operand, §6) — it is **not**
the `count` bug.

Caution: my earlier "2 iterations fault" datapoint was a broken patch of mine
(a non-terminating counter that never reaches 0), not evidence — the loop's exit
requires `count == 0` exactly.

---

## 9. Side investigation for AsmEncodingRE: is any `LSU0.LD.8` shape fieldable? (all runs 2x where noted)

Skeleton (base = the real input buffer, readback via the proven store path):
```
LSU0.LDO.32 i10,i18,0x0 (+6 NOPs)   ; i10 = in ptr  (0x2E014340)
LSU0.LDO.32 i0, i18,0x28 (+6 NOPs)  ; i0  = out ptr (readback base)
<candidate load> (+6 NOPs)
LSU1.ST.32 i2, i0                   ; [out+0] = i2
BRU.JMP i30
```
* Control `LSU0.LDO.32 i2,i10,0x0` -> ACTUAL[0:4] = `80 c4 de c0`
  ⇒ **in[0..3] = 80 c4 de c0** (in[1] = 0xc4), and the base + readback path are proven.
  Cross-check: in[0:2] = `80 c4` = fp16 0xc480 = −4.5, exactly what the kernel's own
  `LD.32.F16.F32` produced (0xc0900000 = −4.5f).
* Every LD.8 candidate delivered **nothing** (ACTUAL[0:4] = `00 00 00 00`, which is
  not the carrier-init `00 01 00 00` ⇒ the readback store ran and wrote i2 = 0):

| shape | bytes | result |
|---|---|---|
| 2-op 42-class (`42 5d 00 XX`, XX = 0x0e and 0x00) | `42 5d 00 0e` / `42 5d 00 00` | 0 |
| fmt8 2-op 82-class | `82 2d 14 80` | 0 |
| fmt12 2-op | `42 20 94 0d` | 0 |
| 2-op, base i10 @18, off=0 | `42 5d 28 0e` | 0 |
| fmt13 4B off=1 / off=2 / off=3 | `42 51 a8 20` / `42 51 28 21` / `42 51 a8 21` | 0 |
| c2-class 5B displacement, off=0..3 | `c2 2a 54 20 06` (same emit for off 1/2/3) | 0 |

⇒ 9 distinct LD.8 fieldings (formats 0/2/4/6/8/12/13 + c2 displacement) never write
their destination; byte3 (0x0e vs 0x00) and the offset field are both moot because
**nothing** is delivered at any offset including 0 (where in[0] = 0x80 is nonzero).
Honest limit: "delivers nothing" cannot be distinguished from "decoded as another op
that does not write i2"; the actionable conclusion is that no LD.8 byte load is
fieldable in these shapes — route byte access through `LDO.32`/`LD.16` + mask.

Reference for the shared substrate: candidate list and controls supplied by
AsmEncodingRE (`ANNOTATED_RULES.md` C21); raw run dirs `out/ld8*-*/`, `out/ldb*-*/`,
`out/ldw0-*/`; liveness variant = `ld8v2_*.s` (trailing stores both execute, so no
half-byte leak shifts the following instruction stream).

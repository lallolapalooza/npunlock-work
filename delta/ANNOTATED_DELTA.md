# ANNOTATED_DELTA.md — annotated SHAVE-only logic in `moviCompile64.dll`

## TL;DR — the five mechanisms that decide our code generation

| # | mechanism | evidence (this file) | effect on our emission |
|---|---|---|---|
| 1 | **Load latency = 6 cycles**, used as a scheduling *distance* (and default latency for any unstaged opcode) | §1, §2 (`FUN_180145fd0`, `FUN_180138bd0`) | a consumer may not be scheduled <6 slots after its LSU producer; our kernel's zero-writes came from ignoring this |
| 2 | **Branch delay = 6 slots**; the 6 slots after a taken branch execute (hardware-verified). The vendor's register table has a dedicated branch-state register `B_STATE` (`[INFERENCE]` it tracks the pending branch slots) | §1, §2, §A5 of `DELTA.md` | every branch (incl. the epilogue `BRU.JMP`) needs its 6-slot delay window filled with real/legal work |
| 3 | **VLIW pipes**: one op per unit over `BRU CMU IAU LSU0 LSU1 PEU SAU VAU`, ≤128 bits, printed `a \|\| b \|\| c`; NOP opcode `0xaa9` compresses to *no text but still a slot* | §5 (`FUN_180098ca0`), §11.1/§11.2 (moviAsm rules) | slot/FU assignment is semantic: inserting an unrelated `LDIL` shifts the store into a different slot ⇒ it never executes |
| 4 | **`LDIL`→LSU0 + `LDIH`→LSU1 in the same pipe** (two pipes if the part lacks LSU1); constants are how the vendor materialises 32-bit immediates | §6 (`FUN_180098410`, the `'abort'` reference packet) | the vendor's own 3-slot template `BRU.SWIH 1 \|\| LSU0.LDIL i18,0xFFFF \|\| LSU1.LDIH i18,0xFFFF` shows the required shape |
| 5 | **LSU discipline**: load and store must not share an LSU in a pipe window (`shave-lsu-load/store-policy`, `shave-aliased-store-distance`, `shave-lsu-flush-on-enter` ⇒ `FLUSH_LSU0/LSU1` on entry; gold = loads LSU0 / stores LSU1) | §4, §11.1 | a load and a store on the same LSU produce garbage; the post-RA scheduler resolves this by *mutating the FU* in the order `LSU0,LSU1,IAU,SAU,VAU,CMU` |

All line numbers are in
`/home/asdf/projects/r2gvp04w-re/re_decompiled/movicomp/decompiled_all.c`
(Ghidra output; 277,822,431 B, one `/* ===== Function N: FUN_<addr> @ <addr> size=<n> ===== */`
header per function). `FUN_<addr>` names are Ghidra's; the names in `**bold**`
are the descriptive renames this document introduces. Addresses are vendor-image
VA (imagebase `0x180000000`), so they can be re-dumped with
`tools/strva.py … --addr <va>`.

Fields referenced repeatedly:

| field | meaning (established below) |
|---|---|
| `shaVEInfo+0x18c` | **branch delay slots** = `min(max(shave-branch-delays, subtarget_default), fn_override)` |
| `shaVEInfo+0x190` (`400`) | **memory load latency** = same clamp of `shave-load-latency` |
| `shaVEInfo+0x50` | pointer to the **SHAVESubtarget** |
| `subtarget+0x22c` | hardware memory-load latency from the feature bits (6, or 8 on some parts) |
| `subtarget+0x208` | CPU/arch revision enum (Myriad2=`2/3`, Myriad4=`4` …) |
| `MI+0x10` | **instruction descriptor** (`*desc` = opcode, `desc[1]` = operand count, `*(uint*)(desc+8)` = 32-bit flag word, `*(longlong*)(desc+0x28)` = pointer to a 6-byte-stride per-operand byte array) |
| `MI+0x20` | operand array, 0x20-byte stride |
| `MI+0x28` | operand/bundle count |

---

## 1. **`shave_recompute_schedule_constants`** — the 6/6 clamp
`FUN_180145fd0 @ 0x180145fd0` (103 B) — `decompiled_all.c:223120-223150`

```c
void FUN_180145fd0(longlong param_1)                 /* shaVEInfo */
{
  int local_18 [6];
  local_18[0] = DAT_183c6e138;                       /* cl::opt<int> shave-branch-delays VALUE (obj 0x183c6e0b8 + 0x80) */
  piVar2 = local_18;
  if (DAT_183c6e138 < *(int *)(param_1 + 0x194))     /* firmware/CPU floor from ParseSubtargetFeatures */
    piVar2 = (int *)(param_1 + 0x194);
  iVar3 = *piVar2;
  if (*(int *)(param_1 + 0x198) < *piVar2)           /* per-function ceiling (fn attribute / -forceDelaySlots) */
    iVar3 = *(int *)(param_1 + 0x198);
  *(int *)(param_1 + 0x18c) = iVar3;                 /* branch delay slots */

  uVar1 = *(uint *)(*(longlong *)(param_1 + 0x50) + 0x22c);   /* subtarget hardware load latency */
  uVar4 = DAT_183c6eb98;                             /* cl::opt<uint> shave-load-latency VALUE (obj 0x183c6eb18 + 0x80) */
  if (DAT_183c6eb98 < uVar1) uVar4 = uVar1;
  if (*(uint *)(param_1 + 0x1a4) < uVar4) uVar4 = *(uint *)(param_1 + 0x1a4);
  *(uint *)(param_1 + 400) = uVar4;                  /* memory load latency */
}
```

Renamed → **`SHAVEMachineFunctionInfo::recomputeScheduleConstants()`**; the two
*value fields* prove the `cl::opt` layout (`obj+0x80`) and the defaults:
`shave-branch-delays` = `min(max(6, hw_default), per_fn)`,
`shave-load-latency` = `min(max(6, hw_default), per_fn)`.

Caller: `FUN_180123080 @ 0x180123080` (`decompiled_all.c:199354`), the
**`ParseSubtargetFeatures`-style** routine that turns a feature bitmask
(`uVar3 = param_1[0x1b]`) into hardware parameters — `decompiled_all.c:199307-199330`:

```c
  if (((uVar3 >> 0xf & 1) != 0) && (*(uint *)((longlong)param_1 + 0x22c) < 6))
    *(undefined4 *)((longlong)param_1 + 0x22c) = 6;   /* feature bit 15 → load latency ≥ 6 */
  if (((uVar3 >> 0x10 & 1) != 0) && (*(uint *)((longlong)param_1 + 0x22c) < 8))
    *(undefined4 *)((longlong)param_1 + 0x22c) = 8;   /* feature bit 16 → load latency ≥ 8 */
  …
  *(undefined4 *)((longlong)param_1 + 0x194) = uVar4; /* default branch delays, from the CPU table */
```

→ **Two hardware latency classes: 6 (Myriad2 family) and 8 (newer part)**; a
feature bit also selects ±(`0x41` ∈ {0,1,2,4} …), `+0x224` ∈ {2,0x20} (vector
width?), `+0x44/+0x45/+0x46` (register/other counts).

---

## 2. **`shave_get_instr_latency`** — where "6 cycles before first use" lives
`FUN_1801387b0 @ 0x1801387b0` (299 B) — `decompiled_all.c:213788-213840`

```c
uint FUN_1801387b0(longlong param_1 /*shaVEInfo*/, longlong param_2 /*MachineInstr*/)
{
  puVar3 = *(ushort **)(param_2 + 0x10);        /* desc */
  uVar8  = 0;
  uVar1  = *puVar3;                             /* opcode */
  if (uVar1 < 0xf4) return 0;                   /* low opcodes: zero latency */
  iVar2  = *(int *)(param_2 + 0x28);            /* operand count */
  for (iVar7 = 0; iVar7 != iVar2; ++iVar7) {    /* per-operand latency, take the max */
    cVar4 = FUN_1801388e0(param_1, param_2, iVar7, local_38);
    uVar5 = cVar4 ? local_38[0] : 0;
    if (uVar8 < uVar5) uVar8 = uVar5;
  }
  /* (A) control-flow ops: the flag word bit 0x250 (excluding 0x24b/0x24c/0x24f/0x250/0x251) */
  if (((*(uint *)(puVar3 + 4) & 0x250) != 0) && opcode not in {0x24b,0x24c,0x24f,0x250,0x251}
      && uVar8 < *(uint *)(param_1 + 0x18c))
    uVar8 = *(uint *)(param_1 + 0x18c);          /* → 6 branch delay slots */
  /* (B) LSU ops: table predicate at 0x1800b4df8 */
  if (FUN_1800b4dc0(*puVar6) && uVar8 < *(uint *)(param_1 + 400))
    uVar8 = *(uint *)(param_1 + 400);            /* → 6 memory load latency */
  /* (C) short-latency LSU family */
  if (opcode ∈ {0x8e0, 0x8f9, 0xa4e, 0xa63, 0xa64} && uVar8 == 0) uVar8 = 1;
  return uVar8;
}
```

and its default-filler `FUN_180138bd0 @ 0x180138bd0` (50 B) — `decompiled_all.c:213950-213968`:

```c
int FUN_180138bd0(longlong shaVEInfo, undefined8 MI, int table_latency)
{
  if (-1 < table_latency) return table_latency;   /* itinerary table entry wins */
  iVar1 = FUN_1801374c0();                        /* sched class */
  iVar2 = 6;                                      /* HARD DEFAULT LATENCY = 6 */
  if (iVar1 == 6) iVar2 = *(int *)(shaVEInfo + 0x18c);  /* sched class 6 == branch → delay slots */
  return iVar2;
}
```

**Rule (the blocker, confirmed from the vendor side):** *every* instruction whose
latency is not pinned by the itinerary table gets **6 cycles**, and instructions
that are LSU-shaped get `max(latency, load_latency)` = **6 cycles between the
load and the first consumer**. The scheduler adds these as `SDep` latencies
(`FUN_1801388e0`, `decompiled_all.c:213846-213905`, walks the per-operand byte
array `desc+0x28`, stride 6, `byte[2] & 2` marks "use" operands and reads the
latency from a table indexed by `(operand_index + use_index)`).

**Opcode predicate table** (`FUN_1800b4dc0 @ 0x1800b4dc0`, 47 B, broken
decompilation but readable): for `op ∈ [0x15e, 0x9d5]`, `pred = (table[op-0x15e] == 0)`
with the table at VA `0x1800b4df8`, 0x878 bytes, **543 selected opcodes**:

```
0x15e-0x166, 0x73e-0x73f, 0x7ae, 0x7bd-0x828, 0x82f-0x9d5
```

i.e. everything in the LSU/LDI/vector space *except* the small hole
`0x829…0x82e` (a 6-member family that the flag builder below also treats as a
group — `[INFERENCE]` these are the paired `@HIopt/@LOopt` variants with their
own latency). `[INFERENCE]` the predicate is best read as
**`isLSUResultLatencyOpcode`** ("result only visible after the load latency").

---

## 3. **`shave_build_instr_flags`** — the 43-bit instruction property word
`FUN_1800dbac0 @ 0x1800dbac0` (3891 B) — `decompiled_all.c:148358-149380`
(frozen extract: `raw/flagbuilder_1800dbac0.c`)

Shape: for one opcode, a chain of

```c
  if (<opcode set>) bVar10 = true; else bVar10 = false;
  uVar4 = uVar5 | 0x2000000;
  if (!bVar10) uVar4 = uVar5 & 0xfffffffffffdffffff;
```

43 bits were recovered by parsing the chain; the exact bit → opcode-set mapping:

| bit | condition source |
|---|---|
| `0x10 0x20 0x40` | predicate functions (`cVar2`), values not recovered |
| `0x80` | opcode set around `0x250/0x251/0x252` |
| `0x1000` | `op-0x5e7 < 0x44` (opcode range) |
| `0x8000`, `0x10000` | `op-0xf31 < 0x1f` |
| `0x20000` | `{0x7d9, 0x7ef, 0x803, 0x807}` |
| `0x100000` | `{0x861, 0x8bc}` |
| `0x200000` | `{0x862, 0x8bd}` |
| `0x400000` | `{0x863, 0x8c2, 0x8fa}` |
| `0x800000` | `{0x864, 0x8c7, 0x8fb}` |
| `0x1000000` | `{0x8e0, 0x8f9, 0xa4e, 0xa63, 0xa64}` (the short-latency LSU family) |
| `0x2000000` | predicate `FUN_1800d22d0(op)` |
| `0x4000000` | predicate `FUN_1800b4dc0(op)` (§2) |
| `0x8000000` | predicate `FUN_1800d24b0(op)` |
| `0x10000000` | `{0x829, 0x82a, 0x82b}` |
| `0x20000000`, `0x40000000` | `{0x829 … 0x82e}` |
| `0x100000000`, `0x200000000`, `0x800000000`, `0x1000000000` | `op-0xfc6/0xfc7 < 0xb5` ranges |
| `0x8000000000` | `{0x7ae}` |
| `0x80000000000` | `op-0xf3 < 0x140` |
| `0x40000000000` | `{0xbcd, 0xbd0, 0xcc7}` |
| `0x100000000000` | `op-0x10d9 < 0x41` |
| `0x200000000000` | `op-0x121d < 0x5b` |
| `0x400000000000` | `op-0x121d < 0x5b` |
| `0x800000000000` | `{0x9ff}` |
| `0x1000000000000` | `{0xa00, 0xa01, 0xa25, 0xa38, 0xa4f}` |
| `0x2000000000000`, `0x4000000000000` | `{0xa01, 0xa02, 0xa38, 0xa39, 0xa4f, 0xa50}` |
| `0x8000000000000` | `op-0xd7d < 0x13` |
| `0x10000000000000` | `op-0xd7d < 0x13` |
| `0x80000000000000` | `{0xf2d}` |
| `0x100000000000000` | `op-0x23f < 0x16` |
| `0x200000000000000` … `0x1000000000000000` | predicates |
| `0x2000000000000000` | `{0xd3c…0xd3f, 0xd46, 0xd47}` |
| `0x8000000000000000` | `op-0xae7 < 0x267` |

The **high dword is the `desc+8` flag word** used by §2(A): bit `0x1000000000`
= dword bit 36 (0x10), etc. `*(uint*)(desc+8) & 0x250` = 64-bit bits 36/38/41 →
"this instruction occupies the branch/delay machinery".

**Actionable:** this table is the vendor's `TSFlags`; it is how the packetizer
knows slot legality, latency class and operand classes. Reproducing only this
table would let our scheduler/packetizer behave like the vendor's.

---

## 4. **`shave_postra_scheduler::run`** — trace scheduler driver
`FUN_1800e8e80 @ 0x1800e8e80` (3010 B) — `decompiled_all.c:157698-160750`
(extract: `raw/postsched_1800e8e80.c`)
Reached from the pass vtable slot `+0x88` of `shavepostraschedulerpass`
(vtable `0x1830719d8`, see `DELTA.md` §A2).

```c
  /* feature/option bits packed into the scheduler object at +0x158 */
  cVar6 = FUN_180146680(STI);  uVar21 |= 2   if set;      /* STI+0x50 → +0x118 bit field */
  cVar6 = FUN_1801466a0(STI);  uVar21 |= 4   if set;      /* subtarget+0x208 - 2 < 2  → Myriad2 */
  cVar6 = FUN_1801466c0(STI);  uVar21 |= 8   if set;      /* subtarget+0x208 == 4     → Myriad4/VPU4 */
  cVar6 = FUN_1801466c0(STI);  uVar21 |= 0x10 if (set && DAT_183c6f718 != 0);
  /* classify each block for the LSU-distance tracker  [INFERENCE: load/store naming] */
  if (opcode == 0x82d && operand_kind == 8)  block_flags_A[bb_id] = 1;   /* load-family */
  if ((opcode == 0x240 || opcode == 0x242) && operand_kind == 0x2e) block_flags_B[bb_id] = 1;  /* store-family */
  FUN_1800ed130(param_1);          /* LSU policy / store-distance pass */
  FUN_1800d9fa0(param_1);
  uVar25 = max(DAT_183c70448 /*shave-aliased-store-distance*/, DAT_183c6f718);
                                                 /* DAT_183c6f718 == obj(0x183c6f698)+0x80
                                                    == shave-vpu4-load-store-distance */
  uVar21 = *(uint *)(*(longlong *)(param_1 + 0x1b0) + 400);   /* ← shaVEInfo+0x190 = load latency */
  local_f0 = max(uVar25, uVar21);                             /* scheduling bound ≥ load latency */
  …
  FUN_1800daaf0(param_1);          /* trace generation (max-trace-blocks / direction) */
  FUN_1800d4ee0(param_1);
```

**Rules:**
1. The post-RA scheduler is **trace based** (`shave-postra-scheduler-max-trace-blocks`,
   `…-trace-generation-direction` = `both|backwards|forwards`), weighted by
   inner/outer-loop block weights.
2. **The memory-load latency (6) is used as a per-instruction scheduling
   distance** — not just as a hazard stall. Any emission model that ignores it
   will schedule a consumer 1 instruction after the producer.
3. Blocks are pre-classified by scanning MIs for `opcode==0x82d` (load) and
   `opcode∈{0x240,0x242}` (store) with operand kind `0x2e` → the
   **aliased-store-distance** tracking (`store on one LSU followed by an aliased
   load on the other LSU` ⇒ ≥1 extra cycle, default 1, max 6).
4. Instructions can be **re-targeted to another functional unit** during
   scheduling: `shave-postra-scheduler-functional-unit-mutation-order` default
   string `"LSU0,LSU1,IAU,SAU,VAU,CMU"` (`decompiled_all.c:2263`) is parsed and
   consumed by `FUN_1800e0a70 / FUN_1800e76f0` (+ `shave-inplace-mutation`).
   ⇒ *A store can legitimately move LSU0→LSU1 (or vice-versa) when the packet's
   LSU0 slot is taken.* This is the mechanism behind "adding an LDIL in front of
   a store stops the store from executing" when slot/FU occupancy is not modelled.

---

## 5. **`shave_emit_instruction`** — NOP compression + VLIW packet printing
`FUN_180098ca0 @ 0x180098ca0` (1116 B) — `decompiled_all.c:96154-96375`

```c
void FUN_180098ca0(longlong asmPrinter, ulonglong *MI)
{
  if (*(short *)MI[2] == 0xaa9) {                 /* MI->MCID->opcode == 0xaa9  == the NOP slot */
    if (DAT_183c6d520 == '\0')                    /* shave-disable-nop-compression VALUE == false */
      *(int *)(asmPrinter + 0x350) += 1;          /*   ⇒ compress: count & emit nothing */
    else {                                        /* compression disabled: */
      local_1368 = "        ";                    /*   print 8 blanks for the NOP */
      FUN_180c27030(*(undefined8 *)(asmPrinter + 0x110), &local_1390);
    }
    return;
  }
  … /* collect this MI and its bundle partners into a SmallVector of 0x14-byte entries */
     /* partner scan: while ((flags >> 2) & 1) prev = …; while ((flags >> 3) & 1) next = … */
  if (bundle_size < 2) { /* single instruction: straight through */ }
  else {               /* ≥2 : build one packet, entries carry a separator tag (low byte 6) */
    FUN_180091490/FUN_180091b00(…);                 /* dedup/sort of the bundle entries */
    … local_13a0.low = 6; local_358[local_350++] = {tag, entry}; …
  }
  FUN_181766700(asmPrinter, out_stream, &bundle_structure);  /* → MC layer packet printer */
}
```

`FUN_181766700 @ 0x181766700` (33 B, `decompiled_all.c:4222539`) is a thunk to
`(*(*out + 0x480))(out, bundle, asmPrinter+0x118->+0x10)` — i.e. the packet is
handed to a **packet-aware instruction emitter**, which is what joins the slots
with the separator literal `"            || "` (present in the vendor string table
next to `// Unable to print debug meta-instructions for opcode '`):

```
            || 
 // Unable to print debug meta-instructions for opcode '
' inside a bundle
 // Unable to print debug meta-instructions
```

**Rules:**
1. **NOP compression** = "if the instruction is the NOP opcode `0xaa9`, print
   nothing (compress) unless `-shave-disable-nop-compression` is given, in which
   case print 8 spaces". A compressed NOP therefore still **occupies a slot**
   (it exists as a MachineInstr) but contributes no text.
2. A packet is a *bundle* of MIs linked by the bundle flags (bits 2/3 of the MI
   flags byte at `MI+0x2c`); `≥2` MIs ⇒ one line with `||` separators.
3. Only `≥2`-member bundles go through the packet path — a 1-member bundle is
   printed normally (this is why "one instruction per line" input can still be
   *accepted* by moviAsm while differing from the vendor stream).

---

## 6. **`shave_emit_function_body`** — the `'abort'` reference packet (LSU0/LSU1 split)
`FUN_180098410 @ 0x180098410` (2183 B) — `decompiled_all.c:95690-96130`,
abort site at `decompiled_all.c:96050-96070`:

```c
  if (*(char *)((longlong)asmPrinter + 0x354) != '\0') {          /* "function is empty → force abort" */
    lVar14 = param_1[0x22];
    if (DAT_183c6d8a0 == '\0') {                    /* DAT_183c6d8a0 = shave-has-LSU1-fu VALUE (obj 0x183c6d820+0x80) */
      local_148 = "LSU0.LDIL i18, 0xFFFF\n";                      /* packet 1 */
      …emit…
      local_148 = "BRU.SWIH 1 || LSU0.LDIH i18, 0xFFFF  // 'abort'\n";   /* packet 2 */
    } else {
      local_148 = "BRU.SWIH 1 || LSU0.LDIL i18, 0xFFFF || LSU1.LDIH i18, 0xFFFF  // 'abort'\n"; /* ONE packet */
    }
    …emit…
```

**Rule (exactly what is failing in our stream):**
* The canonical constant materialisation is **`LDIL` on LSU0 + `LDIH` on LSU1**,
  which *can* occupy the same packet as a `BRU` instruction — but only when the
  part has **LSU1** (`shave-has-LSU1-fu`). Without LSU1 the two halves must be
  split into **two consecutive packets**, with the branch bundled with the
  `LDIH` packet.
* Same packet, 3 slots: slot order in text is `BRU… || LSU0… || LSU1…`.
* Blowing up the payload by always emitting `LDIL`+`LDIH` as a pair on LS1-less
  parts (our wave-1 behaviour) changes the packet count and therefore all
  downstream byte offsets.

Adjacent empty-function diagnostics quoted here (`decompiled_all.c:96073-96105`)
also show the printer emits `" The function '"` + name +
`"' contains no instructions, so force an immediate 'abort'"`.

Function alignment is applied in the same routine
(`decompiled_all.c:96008-96035`): alignment =
`1 << ((bb_align_bits & 0x3f) - 1)` with `DAT_183c70368` (=
`shave-align-function-targets` VALUE, obj `0x183c702e8`+0x80) overriding to 16.

---

## 7. **`shave_emit_alignment`** — the 1-or-16-byte rule and `.lalign`
`FUN_180097e00 @ 0x180097e00` (1147 B) — `decompiled_all.c:95520-95690`,
decision at `decompiled_all.c:95669-95688`:

```c
      uVar15 = 1L << (*(byte *)(param_2 + 0xb8) & 0x3f);      /* required alignment (bytes) */
      iVar9  = (int)uVar15;
      if ((iVar9 != 1) && (iVar9 != 0x10)) {                  /* ONLY 1 or 16 are legal */
        … "Not expecting to see the Basic-Block alignment not equal to zero or 2^^4 for SHAVE; got 2^^" …
        pcVar1 = (code *)swi(3); (*pcVar1)();                 /* fatal */
      }
      if ((DAT_183c70528 != '\0') ||                          /* shave-align-all-branch-targets VALUE */
         (((DAT_183c6de98 != '\0' || (DAT_183c6f8e0 != '\0')) && (iVar9 == 4)))) {
        local_188 = "    .lalign";                            /* SHAVE-only directive */
        FUN_180c27030(param_1[0x22], &local_188);
      }
```

**Rules:**
1. **Basic-block alignment must be exactly 1 or 16 bytes.** Setting any other
   `.align`/`.p2align` for a SHAVE BB makes the vendor compiler die; our printer
   must not emit `.align 4/8` on BBs.
2. Aligned branch targets are marked with the SHAVE-only directive **`.lalign`**
   (not `.align`) — moviAsm64 understands it; a generic assembler does not.
3. Options that drive it: `shave-align-all-branch-targets` (all 16-byte targets),
   `shave-align-jump-targets`, `shave-align-loop-targets`, `shave-align-function-targets`.

---

## 8. **`shave_subtarget_predicates`** — CPU revision + FU availability
`decompiled_all.c:213788-213830` (`FUN_1801465c0/630/660/680/6a0/6c0`),
`FUN_180145f90/fa0` (`decompiled_all.c:223088-223118`)

```c
/* FUN_180145f90 @ 0x180145f90 */  return DAT_183c6ec78;
        /* DAT_183c6ec78 == obj(0x183c6ebf8)+0x80 ==> it *is* the value of cl::opt<bool> `shave-has-SAU-fu` */
/* FUN_180145fa0 @ 0x180145fa0 */
  uVar1 = (uint7)((ulonglong)*(longlong *)(param_1 + 0x50) >> 8);      /* MI/SUnit → subtarget */
  if (((*(uint *)(*(longlong *)(param_1 + 0x50) + 0x118) & 0xc00) != 0) /* subtarget feature bits 0x400|0x800 */
      && (DAT_183c6f9c0 != '\0'))                                     /* == value of `shave-has-VAU-fu` */
    return CONCAT71(uVar1,1);
  return (ulonglong)uVar1 << 8;
        /* ==> a vector unit is available only if BOTH the subtarget feature bit AND the option are set */
/* FUN_180146680 @ 0x180146680 */  (STI+0x50)->+0x118 >> 0x14 & …     /* FU-enable bit field */
/* FUN_1801466a0 @ 0x1801466a0 */  ((STI+0x50)->+0x208 - 2) < 2        /* CPU ∈ {2,3} */
/* FUN_1801466c0 @ 0x1801466c0 */  ((STI+0x50)->+0x208) == 4           /* CPU == 4 (VPU4/Myriad4) */
/* FUN_180146830 @ 0x180146830 */  float cost model: `(n+5)/w1 * n_blocks * K + n/w2 <= 2n/w3`
                                   — decides whether to extend a trace (block weights at
                                   shaVEInfo+0x180/0x184/0x188 = inner/outer loop weights) */
```

Consumers: `shave-has-SAU-fu` → `FUN_180145f90`; `shave-has-VAU-fu` → `FUN_180145fa0`
and `FUN_180146b30`; `shave-has-LSU1-fu` → the `'abort'` packet split in §6 and the
scheduler's feature bits in §4. `FUN_180146680/6a0/6c0` gate the scheduler feature
bits; `FUN_180146630` shows the `(flags & 0x140000) == 0 && cpu-2 > 1` shape used
by the ISA-extension predicates.

`[INFERENCE]` `subtarget+0x208` is the `SHAVE::CPU` enum
(0x15e-ish string lists `myriad2.1/2/3/4`, `ma2x5x/2x8x/2x9x`, `vpu20-nn`); the
`SHAVE_SETCC_*_Myr4` DAG nodes and `-shave-vpu4-load-store-distance` confirm the
"revision 4" feature axis.

---

## 9. Inline-assembly contract (why moviAsm tolerates our stream)
`FUN_180098410` / `FUN_180149a20 @ 0x180149a20` / `FUN_18015f870 @ 0x18015f870`
(`decompiled_all.c:225793`, `241659-241680`; all three read
`shave-nop-sync-after-inline-asm` = obj `0x183c6ecd8`):

```
 +------------------ Begin - User's Inline Assembly Code ----------------------+
 |                                                                             |
 // | !!! Have you ensured that there are enough trailing NOPs to satisfy     !!! |
 // | !!! all outstanding instruction latencies in your assembly code?        !!! |
 // | !!! See 'moviCompile.pdf' section 11.2.1 Inline Assembly and Scheduling !!! |
 // +-------------------- End - User's Inline Assembly Code ----------------------+
        NOP SYNC  // Wait for outstanding schedules to complete
```

**Rule:** inside inline asm the *user* is responsible for padding the 6-cycle
latencies and branch delay slots; the compiler only optionally appends
`NOP SYNC` (`-shave-nop-sync-after-inline-asm`) and emits the banner (with
`shave-max-inline-assembly-lines` / `shave-inline-assembly-warning-threshold`
diagnostics, frontend side `FUN_1814618a0`).

---

## 10. Consolidated RULES list (all evidence-backed)

| # | rule | evidence |
|---|---|---|
| R1 | Memory **load latency = 6 cycles** by default (`shave-load-latency`, min 6, max UINT_MAX; some parts force 8 via feature bit 16). A consumer may not be scheduled closer than that. | `FUN_180145fd0` (223120-223150), `FUN_180138bd0` (213950), `FUN_180123080` (199307-199330) |
| R2 | **Branch delay = 6 slots** (`shave-branch-delays`, min 0, max INT_MAX); branch-sched-class instructions take the value at `shaVEInfo+0x18c`. | `FUN_1801387b0` (213788), `FUN_180138bd0` (213950) |
| R3 | Any instruction without a table latency defaults to **6**. | `FUN_180138bd0` (213950) |
| R4 | Functional units = **`LSU0, LSU1, IAU, SAU, VAU, CMU`** → ≤6 slots/packet; the post-RA scheduler may **mutate an instruction's FU** in that priority order. | option desc/string `decompiled_all.c:2263,2266`; `FUN_1800e0a70`, `FUN_1800e76f0` |
| R5 | **`LDIL`→LSU0, `LDIH`→LSU1**, same packet when the part has LSU1; otherwise **two packets**. | `decompiled_all.c:96057-96065`, gated on `shave-has-LSU1-fu` (`0x183c6d8a0`) |
| R6 | A packet is emitted as one line with `||` between slots; single-instruction "bundles" print normally. | `FUN_180098ca0` (96154-96375), separator string `"            || "` |
| R7 | **NOP compression**: NOP opcode `0xaa9` prints nothing when compression is on (default), 8 blanks when `-shave-disable-nop-compression`. Compressed NOPs still occupy a slot in the MachineInstr stream. | `FUN_180098ca0` (96208-96222) |
| R8 | **Basic-block alignment ∈ {1, 16} bytes only**, else fatal; aligned branch targets use the SHAVE-only directive **`.lalign`**. | `FUN_180097e00` (95669-95688) |
| R9 | Function/branch/loop/jump target alignment (16 B default) from `shave-align-*`. | `FUN_180097e00`, `FUN_180098410` (96008-96035) |
| R10 | Aliased store→load on the *other* LSU needs `shave-aliased-store-distance` extra cycles (default 1, 0..6); loads/stores are pre-classified per block (opcodes `0x82d` load, `0x240/0x242` store). | `FUN_1800e8e80` (157800-157870), option desc strings 86657-86660 |
| R11 | LSU policy options constrain/`prefer` LSU0/LSU1 for loads, stores, and volatile accesses; volatile defaults to LSU1. | `shave-lsu-load-policy`, `shave-lsu-store-policy`, `shave-lsu-volatile-load-store-policy` (obj `0x183c6efb0/0x183c708e0/0x183c6e6e0`), consumers `1800e1b80`, `1800ed130` |
| R12 | `-shave-lsu-flush-on-enter` inserts code that **flushes both LSUs on function entry** (pseudo ops `FLUSH_LSU0`/`FLUSH_LSU1`). | strings 79290-79310, 87762-87764; consumer `FUN_1800fbca0` |
| R13 | Inline asm: the **user must pad outstanding latencies with trailing NOPs**; the compiler optionally appends `NOP SYNC`. | strings 88085-88087, 79130; `FUN_180149a20`, `FUN_18015f870` |
| R14 | The post-RA scheduler is **trace based** (max-trace-blocks, direction `both|backwards|forwards`, inner/outer-loop block weights) and uses `max(load_latency, …)` as a scheduling bound. | `FUN_1800e8e80` (157800-158000); option strings 86746-86768 |
| R15 | A `≥2`-MI bundle is printed as one packet; the packet's slot tags are per-slot entries with a separator byte (`6`) set by `FUN_180098ca0`. | `FUN_180098ca0` (96284-96300) |
| R16 | NOP opcode (for the printer) = **`0xaa9`**; the short-latency LSU family = `{0x8e0,0x8f9,0xa4e,0xa63,0xa64}`; the excluded LSU-family hole = `{0x829..0x82e}`. | `FUN_180098ca0` 96208, `FUN_1801387b0` 213820-213835, predicate table `0x1800b4df8` |
| R17 | `shave-disable-slot-optimiser` and `shave-inline-assembly-warning-threshold` have **no code reader** in this build — do not rely on them. | `raw/opt_xrefs.json` (0 consumers) |
| R18 | A pipe may contain **at most one op per unit**, over **8** units: `BRU CMU IAU LSU0 LSU1 PEU SAU VAU`. | `moviAsm64` strings 2557/2559/2570/2616/2617/2630/2632/2641; compiler FU list ≈ subset |
| R19 | A pipe is **≤128 bits** (`.code` → `.64`/`.128` blocks, `.endb`); gold pipes hold 1–3 ops. | `moviAsm64` 2553-2568, 2628, 2642, 2747; `ANNOTATED_DELTA.md` §11.1 |
| R20 | The assembler can **compensate** the compiler's hazards (`-forceDelaySlots:<n>`, `-forceLoadDelay:<n>`) and has its own analyzer (`-a` disables it) that reports `Instruction %s is conflicting: %s`. | `moviAsm64` 2421-2431, 2478, 2681, 2737-2741 |
| R21 | **Correction after binary check:** `-no6thSlotCompression` exists *nowhere* in `moviCompile64.dll` or `moviAsm64.dll` — it is a stale upstream-clang switch. The vendor's own assembler argv is `--noSPrefixing --cv <cpu>` (+ `-Wa,` pass-through); the live compression knobs are `--noCompressedNOPs`, `--noFinalSlotCompression`, `--noSlotAllPromo`. | `strva.py` on both DLLs → "not found"; vendor driver argv at `decompiled_all.c:3243041-3243049`; moviAsm64 strings 2425-2428, 2755-2757 |

---

## 11. Cross-check against `moviAsm64` (assembler side) and against the peer's hardware measurements

Sources: `re_strings/moviAsm64.ascii.txt` (assembler strings/help/diagnostics) and
the team's hardware-verified ledger `npunlock-work/FINDINGS.md` (§"LATEST
hardware-verified rules") + `packets/PACKET_SPEC.md`. **No conflicts found**; the
assembler sides add three rules the compiler side cannot show, marked ★.

### 11.1 Agreements (three independent sources)

| rule | compiler side (this document) | assembler side (`moviAsm64`) | hardware (peer-verified) |
|---|---|---|---|
| load latency = 6 | `FUN_180138bd0` hard default 6; `FUN_180145fd0` `max(shave-load-latency=6, hw)` → `MFI+0x190` | `-forceLoadDelay:<loadDelay>` exists (option table) | K=4/5 fail, K=6/7/12 pass |
| branch delay = 6 | `FUN_1801387b0` uses `MFI+0x18c` = `shave-branch-delays` = 6 | `-forceDelaySlots:<delaySlots>`; warning *"There are side effects after delay slots, remaining latency cycles: %d"* | exactly 6 slots execute after a taken branch |
| `LDIL`→LSU0, `LDIH`→LSU1, same pipe | `'abort'` packet string + `shave-has-LSU1-fu` split | pipe syntax with unit-conflict checks | gold roundtrip: LDIL=LSU0 4 B, LDIH/LSU1 4 B |
| one op per unit per pipe | FU list `LSU0,LSU1,IAU,SAU,VAU,CMU` (mutation order) | ★ **8** units: `BRU CMU IAU LSU0 LSU1 PEU SAU VAU unit is used more than once in the same pipe` | LSU0-load + LSU0-store ⇒ garbage; LSU1-store ⇒ correct |
| pipe ≤ 128 bits | multi-instruction bundles printed with `"            || "` | ★ `.code` section holds `.64` / `.128` blocks (`.endb` ends them); `-no6thSlotCompression`; `-L` aligns labels to **128 bits** | `a || b || c` = 1–3 ops, ≤128 bits |
| NOP compression | NOP opcode `0xaa9` prints nothing unless `-shave-disable-nop-compression` | `--noCompressedNOPs` = *"No NOP compression in delay slots (ma2xxx only)"*, `--noFinalSlotCompression` = *"…final delay slot"*, `--noSlotAllPromo` = *"SLOT_ALL promotion for labels alignment"* | compressed NOPs still consume slots |
| 16-byte alignment | BB alignment must be 1 or 16, else fatal; `.lalign` | `Unaligned target instruction might generate a stall (> 128 bits): %d.`; `-L` = align labels to 128 bits; `.lalign is valid for .code sections only` | — |
| no standalone packet-descriptor byte | compiler emits only `.code` + `.lalign` (strings 79297/78398) — never `.128`/`.64`/`.endb` | the block directives belong to the assembler; `"1234567890abcdef…"`-style tables are *payload* tables | *"the odd bytes in gold are payload bytes of instructions our decoder mis-segments"* |

### 11.2 ★ New facts only the assembler side reveals

1. **The pipe-conflict rule set is 8 units, not 6:**
   `BRU`, `CMU`, `IAU`, `LSU0`, `LSU1`, `PEU`, `SAU`, `VAU`
   (`moviAsm64` strings 2557/2559/2570/2616/2617/2630/2632/2641). The compiler's
   *mutable* FU priority list only names the 6 data-path units; **BRU and PEU are
   also single-op-per-pipe units** — i.e. a pipe can hold at most one branch and
   at most one predicate/permute op, and the total is bounded by 128 bits (≤3 ops
   in the gold corpus).
2. **Block syntax**: `.code` sections are built from `.128`/`.64` blocks closed
   by `.endb`; `.lalign`, `.ealign`, `.salign` are `.code`-only. Diagnostics
   `.128 found outside a .code section`, `Already in a 64 Bit Block`,
   `No previous start of 128 or 64 bit block`, `Code lines inside a non .code section`.
3. **Label-address optimisation** is what "SLOT_ALL promotion" is for:
   `-L : Optimize labels' address (align to 128 bits)` and
   `--noSlotAllPromo : Disable the SLOT_ALL promotion for labels alignment (ma2xxx only)`.
   This is the assembler-side twin of the compiler's `-shave-disable-slot-optimiser`
   ("Disable the VLIW slot optimiser") — and it explains why that compiler option
   has no reader (§4 of DELTA.md): the real work happens in moviAsm, driven by
   `-L`/`--noSlotAllPromo`, which upstream clang does **not** pass (it passes
   `-no6thSlotCompression`, `-noSPrefixing`, `-a` instead).
4. **The assembler can synthesise the latency padding itself**
   (`-forceDelaySlots:<n>`, `-forceLoadDelay:<n>`) and has an *analyzer*
   (`-a : Disable the code analyzer`, `Analyzer fail: %s`,
   `Instruction %s is conflicting: %s`, `Invalid NOP in pipe`) that checks the
   hazards the compiler is supposed to have handled.
5. **`BRU.NOP`** is the assembler's NOP mnemonic (moviAsm string 2968) — this is
   what padded delay slots look like in assembly text, and the peer's emitter
   already encodes it as byte `0x07`.

### 11.3 Consequence for the toolchain-root wrapper

Two different assembler argv templates exist, and they are **not** the same:

```
# upstream clang (git show HEAD:clang/lib/Driver/ToolChains/Myriad.cpp)
moviAsm -no6thSlotCompression -cv:<cpu> -noSPrefixing -a <in>.s -o:<out>.o
#   ↑ "-a" documented as "Disable the code analyzer"; "-no6thSlotCompression" is
#     a *stale* switch (see below)

# the vendor's own driver (decompiled_all.c:3243041-3243049, moviCompile64.dll)
moviAsm --noSPrefixing --cv <cpu>  [<user -Wa,… args>]  <in>.s  …
```

Binary check: **`-no6thSlotCompression` / `6thSlot` do not occur anywhere in
`moviCompile64.dll` or `moviAsm64.dll`** (`tools/strva.py` → "not found"), so the
vendor toolchain never passes it and moviAsm64 does not implement it; it is an
artefact of the upstream driver that the fork dropped. `-cv:` exists only in
moviAsm64 (option table), `--cv` is what the vendor driver actually emits, and
`-forceDelaySlots` / `-forceLoadDelay` exist only in moviAsm64 — the vendor
driver never passes them either.

Our `shave-toolchain-root/bin/moviAsm` wrapper forwards
`--cv <cpu> --noSPrefixing`, which **matches the vendor's own spelling** — good
for parity. What remains open for byte parity is the slot/NOP-compression state
(`--noCompressedNOPs`, `--noFinalSlotCompression`, `--noSlotAllPromo`) and
whether the vendor's driver adds any compression-related switch we have not
recovered (only `--noSPrefixing` and `--cv` are visible as literals in the
driver; everything else is a `-Wa,` pass-through).

---

## 12. What this means for our generator (`llvm/lib/Target/SHAVE`, wave 1)

Wave-1 state (from `TARGET_NOTES.md` / `BYTE_EXACT_LOG.md`): one instruction per
line, always-paired LDIL/LDIH, no bundle `||`, no delay-slot NOPs, no 16-byte
function alignment, `.align 16` and provisional `BRU.*` forms. Against the rules
above, the deltas to close, in value order:

1. **Emit real packets.** Group MIs into pipe slots and print `… || …`; a bundle
   is the unit the assembler encodes, and R5/R7/R15/R18/R19 say slot assignment
   (not just order) is semantically visible. The unit set is
   **`BRU CMU IAU LSU0 LSU1 PEU SAU VAU`** (≤1 op each per pipe, ≤128 bits), of
   which the compiler's mutation order only names the six data-path units
   `LSU0 LSU1 IAU SAU VAU CMU`. A broken bundle is exactly the observed
   "store does not execute".
2. **Constant materialisation `LDIL`/`LDIH` split across LSU0/LSU1 in the same
   pipe** (R5), with the LSU1-less variant emitted as two pipes. Always pairing
   them into *two separate* packets (wave 1) doubles the packet count.
3. **Honour the 6-cycle load latency** (R1/R3): our scheduler must not place a
   consumer within 6 slots unless the producer is one of the short-latency
   opcode families (R16).
4. **Respect the LSU unit conflict** (R18): a load and a store must not use the
   same LSU in the same pipe window — the vendor's own rule
   (`moviAsm64`: `LSUn unit is used more than once in the same pipe`), and the
   scheduler's FU-mutation (`LSU0,LSU1,…`) exists precisely to move one of them.
5. **Avoid `.align` on basic blocks** (R8) and use `.lalign` for aligned branch
   targets; use 16 bytes for function targets (R9); remember an unaligned target
   *stalls* (`> 128 bits`), so the alignment options are performance-relevant,
   not cosmetic (R19: `-L` aligns labels to 128 bits).
6. **Compressed NOPs still consume slots** (R7): when we fill a pipe to keep
   alignment, emit the NOP MI (compressed) rather than dropping the slot.
   The live assembler knobs are `--noCompressedNOPs` / `--noFinalSlotCompression`
   / `--noSlotAllPromo`; the upstream-clang switch `-no6thSlotCompression` is
   **not implemented** by moviAsm64 and the vendor driver never passes it (R21),
   so it must not be relied on for parity.
7. **Do not rely on the assembler to fix our hazards** (R20): `-forceDelaySlots`
   / `-forceLoadDelay` exist, but the vendor driver never passes them — the
   compiler is expected to have emitted correct slot distances itself.

---

# Part II — general-C codegen delta (sections D1–D6)

Purpose of this part: everything below is what our own backend
(`llvm/lib/Target/SHAVE`) and assembler (`shave-isa/shave_asm.py`) must implement
so that **arbitrary C** compiles correctly, not just the `add1` fixture.
Each item carries a confidence tag:

* **[V]** verified in this engagement (binary bytes / decompiled code / upstream `git show`);
* **[S]** inferred from a vendor string or table (strong: generated from the vendor's own `.td`);
* **[H]** independently hardware-verified by a peer (see `npunlock-work/FINDINGS.md`);
* **[I]** inference, stated as such.

---

## D1 — Target identity, data layout, ABI, frame rules

### D1.1 Target identity
* Target name `shave`, short desc `Myriad SHAVE Processor`, backend name `SHAVE`
  — `FUN_180160d90` → `FUN_180c3ba60(&DAT_183c72780,"shave","Myriad SHAVE Processor","SHAVE",&LAB_180160d80,0)`,
  `decompiled_all.c:242755`. **[V]**
* `Triple::shave` and the *driver* side already exist **upstream** in
  `clang/lib/Driver/ToolChains/Myriad.{h,cpp}` (tools `moviCompile`, `moviAsm`,
  `sparc-myriad-rtems-ld`; `-DMYRIAD2`; `-EL`). **[V]**
* `-mcpu` names (41): `3000xx … 5010xx`, `ma2080 … ma2495-nn`, `ma2x5x/2x8x/2x9x`,
  `myriad2.1/2/3/4(-nn)`, `vpu20-nn` (strings 86572-86631, 78506). **[S]**
  The driver's assembler job defaults the CPU to the literal `"ma2150"`
  when `-mcpu` is absent (`decompiled_all.c:3243054-3243060`). **[V]**
* Predefined macros: `__myriad2`, `__myriad2__` (209928-209929), `-DMYRIAD2`
  (upstream driver), `-D__SOFTFP__` (151934/203798), and the two exported
  scheduling constants `__SHAVE_BRANCH_DELAY_SLOTS__`,
  `__SHAVE_MEMORY_LOAD_LATENCY__` (151905-151906). **[V/S]**

### D1.2 Data layout **[V]** (strings 78366 — byte-exact)
```
e-m:e-p:32:32-f64:64-i1:8:8-i64:64-v128:64-v64:64-v32:32-n8:16:32-S64
```
Consequences we must honour: little-endian; 32-bit pointers; **i64/f64 8-byte
aligned**; `i1:8:8` (i1 stored in 8 bits); vectors 32/64/128-bit with 64-bit ABI
alignment (any 256/512-bit WVRF type is *not* an IR type — it is reached through
intrinsics only); native integer widths `8/16/32`; `S64` = 64-bit stack alignment.
Our `TARGET_NOTES.md` already carries this string. **No 64-bit FP**: `f64` is
soft-float (`-D__SOFTFP__`), and `__adddf3/__divdf3/__extendsfdf2/__truncdfsf2`
are the runtime entry points to link. **[S]**

### D1.3 Register file model
* The ABI/ISA exposes **two register files**: `IRF` (integer, what the asm prints
  as `iN`) and `VRF` (vector). Register-class names generated by the vendor's
  tablegen (strings 79290-79300, 86500-86540): `VRF32_q0_1`, `VRF64_h`,
  `VRF32_qN`, and the **write-only** classes `WVRF64_0`, `WVRF128_0`,
  `WVRF256_0`, `WVRF512`, plus `SELECT_WVRF64_0/128_0/256_0` pseudo-instructions
  that copy a WVRF value into a readable VRF register. **[S]**
* `shave-stash-retrieve` — *"Enable the new SHAVE VRF stash-retrieve logic to
  avoid spilling IRF registers to stack"* (86718): the vendor has a dedicated
  mechanism to move live IRF values into VRF instead of the stack. Our backend
  must either implement equivalent spill avoidance or accept stack spills
  (see D1.4). **[S]**
* Special registers exist and are addressable in the ISA: `B_STATE`, `I_STATE`,
  `S_STATE`, `V_STATE`, `C_CMU0..15`, `C_IAU0`, `C_SAU0..3`, `V_ACC0..3`,
  `S_ACC0/1`, `B_IP_0`, `B_ISR_SA`, `B_LBEG/B_LEND`, `P_SVID`, `G_GALOIS`,
  `B_MREPS/B_SREPS`, `TRF_17` (strings 88020-88058). **[S]**
  ⚠ These are architectural state our wave-1 model does not have at all.
* `Generic::ScalarRC` / `Generic::VectorRC` / `Generic::Unknown Register Class`
  (78363-78365) are **upstream** (`llvm/include/llvm/Analysis/TargetTransformInfoImpl.h`)
  — not SHAVE classes. **[V]**

### D1.4 Calling convention, calls, frames
* Argument/return: `i18` = 1st integer argument, `i30` = return (npunlock ABI
  notes; corroborated by the vendor's `'abort'` stub using `i18` as scratch at a
  point where no argument is live: `LSU0.LDIL i18, 0xFFFF`, strings 78386). **[V/S]**
* The backend has the machinery for real calls and frames, at MI level:
  `ADJCALLSTACKDOWN` / `ADJCALLSTACKUP` are **SHAVE-specific names inside the
  target's opcode-name array** (strings 82700-83000, which holds 119
  target-specific names: `IAU_*`, `LSU_*`, `VAU_*`, `CMU_*`, `SAU_*`, `PEU_*`,
  `BRU_*`, `SHAVE_*`, `ADJCALLSTACKDOWN/UP`), and `SHAVEISD::CALL` /
  `SHAVEISD::RET_FLAG` / `SHAVEISD::DYNALLOC` are target DAG nodes (78470-78510).
  **[V]**
* ⚠ **Varargs is *not* proven.** The only `VAARG`/`VASTART` strings are LLVM's
  *generic* `TargetOpcode` names (`G_VAARG` at 82742, `G_VASTART` at 82921) which
  every target's opcode-name array starts with, plus the X86 ones. There is **no**
  `SHAVE_VAARG`-style target opcode. **[V]** ⇒ `setjmp/va_list` support is an open
  question; do not assume the vendor supports it ([I] it probably does, via generic
  lowering + `ADJCALLSTACK*`, but we have no direct evidence).
* Call-preserved masks are named `CSR_EmptySHAVE`, `CSR_OrigSHAVE`, `CSR_StdSHAVE`
  (79290-79293) — **three** conventions, i.e. the vendor has more than one
  callee-saved policy (empty = all caller-saved). **[S]**
* Stack alignment default **8 bytes** (`shave-align-stack`, 86708); the corpus
  shows **no frame/SP usage in the kernels we target**, but the option exists, so
  the backend does have frame lowering. **[S]**
* Stack instrumentation symbols the vendor references: `__stackMaximumExtent`,
  `__stackHighWater` (`shave-track-stack-usage`, `shave-stack-overflow-checking`;
  decompiled_all.c:170412, 172315, 177043, 3245053). **[V]**

### D1.5 Target-lowering limitations stated by the vendor **[V]** (strings 78465-78470)
```
__stackMaximumExtent / __stackHighWater / no-jump-tables
Funclet EH is not implemented for this target
__clear_cache
Named registers not implemented for this target
```
→ **no jump tables**, **no funclets/EH**, **no named registers**, `llvm.clear_cache`
→ `__clear_cache`. Any switch statement in arbitrary C therefore lowers to
compare/branch chains (or a `__builtin_jumptable`-less form), which is exactly
what the vendor does.

> **D1 — we must implement:** the exact DataLayout string (already done); `i18`
> arg / `i30` return with `ADJCALLSTACKDOWN/UP`-style call sequences; a stack
> pointer + frame lowering (at minimum a local spill area for real C); varargs via
> `VASTART/VAARG`; no jump tables; no funclets; f64 = soft-float libcalls.

---

## D2 — ISel / lowering coverage (what is native vs expanded)

The vendor's custom DAG surface is the authoritative list of "native" operations
(`SHAVEISD::*`, 36 nodes, strings 78383-78420 + 78470-78510; plus the
`SHAVE_SETCC_*` family) and the intrinsic surface
`__builtin_shave_<unit>_<op>_<operands>_<type>` catalogued by unit:

| unit | # builtins | evidence |
|---|---|---|
| VAU (vector ALU) | **744** | strings 785xx-79xxx |
| SAU (scalar/ALU) | **306** | ″ |
| CMU (coproc/compare/clamp/convert) | **294** | ″ |
| IAU (integer ALU) | **54** | ″ |
| LSU (load/store) | **42** | ″ |
| BRU (branch) | **4** | ″ |

**[S]** (counts via `grep -c __builtin_shave_<unit>_`).

### D2.1 Native arithmetic (must map ordinary IR here)
* 32-bit integer: `IAU.ADD/SUB/AND/OR/XOR/MUL*` (`shave_iau_mulsi_rr`,
  `mulsi_ri`, `mulsu_rr` = mul signed×unsigned), shifts `IAU.SHL/SHR.I32/SHR.U32`
  (as strings 9070x in moviAsm + our ISA table), `IAU.CLAMP*`.
* Divide **is native but not in IAU**: `SAU.DIV.i32/i16/u32/u16`, `SAU.DIV.f32`
  (`shave_sau_div_i32_rr` …). **[S]** ⇒ an i32 `sdiv/udiv` should become an SAU
  divide when SAU exists, otherwise a libcall — never an inline loop.
* Floating point is **native only for f16/f32** (VAU/SAU/CMU `*_f32_*`, `*_f16_*`),
  f64 is soft-float. **[S/V]**
* Vector ops: 78 builtins whose name contains `64` (e.g. v64i8 DP4A / byte ops),
  WVRF classes up to 512 bits, `CMU.CP.<width>`, `PEU.PVV*` permutes. **[S]**

### D2.2 Explicit i64 handling
`SHAVEISD::I64_CONCAT`, `I64_EXTRACT_HIGH`, `I64_EXTRACT_LOW` **[S]** ⇒ i64 values
are handled as **register pairs with explicit concat/extract**, and there is no
native 64-bit ALU: every i64 op is either a pair-expansion or a libcall
(`__adddi3`-class names are in the runtime table; i64 mul/div/rem are the ones
arbitrary C hits hardest). Wave-1 already splits i64 add/sub/shift; the nodes show
the vendor's shape (concat/extract rather than LLVM's default `BUILD_PAIR`).

### D2.3 Custom nodes and what they replace
| node | meaning / why it exists | ours must |
|---|---|---|
| `HORIZONTAL_ADD/AND/OR/XOR/FADD` | vector *reduction* fused (no upstream equivalent anywhere in LLVM 14) | use CMU/VAU reduction ops or a shuffle+binary tree; a naive ladder is a big code-size delta |
| `CLAMP`, `MIN`, `MAX` | saturating CMU/VAU ops | map `smin/smax/umin/umax/sadd_sat…` to them |
| `CMU_CM`, `CMU_CPII` | coprocessor compare-immediate / predicate + immediate copy | predicate/flag materialisation (see D5.3) |
| `LANESPLAT`, `SPLAT` | one-lane replication (broadcast) | `BUILD_VECTOR` with all-equal operands |
| `UNPACK` | deinterleave | vector shuffle lowering |
| `SHD`, `SHLV` | dual shift / variable shift-left | shift lowering with VRF operands |
| `LDISym` | **symbol materialisation** (constant/address load) — pairs with `shave-enable-ldi` | global-address lowering without a constant pool |
| `CALL`, `RET_FLAG` | call/return with the SHAVE flag protocol | call lowering |
| `VECTOR_EXTEND`, `EXTRACT_SUBVECTOR`, `INSERT_SUBVECTOR` | sub-vector handling | vector type legalisation |
| `DYNALLOC` | `alloca` with a dynamic size | dynamic stack allocation |
| `ACCN_SEQ/ACCP_SEQ/MACN_SEQ/MACP_SEQ` | MAC/accumulator chains (`V_ACC0..3` accumulation, negative/positive) | recognise dot-product/accumulate loops (`shave-generate-{fp,int}-acc-mac` is the opt switch) |
| `TRUNCATE_INREG`, `FPTOUI_INREG`, `UITOFP_INREG` | conversions folded into the producing instruction (in-register truncate) | fold trunc/fptoui/uitofp into the CMU.CP producer |
| `SELECT`/`VSELECT`/`SETCC` (+60 `SHAVE_SETCC_*`) | comparison result materialisation, type- and CPU-qualified (`_Myr4`) | see D5.3 — this is the biggest correctness trap after latency |

### D2.4 Lowering switches that change *correctness-visible* output
* `shave-disable-constant-pools` (86647): *"Do not emit constant pools during
  BUILD_VECTOR lowering"* → by **default the vendor emits constant pools** for
  vector constants. For arbitrary C with vector constants we must decide: pool
  (needs `.rodata` + relocs in the kernel ELF) or materialise. **[S]**
* `shave-enable-ldi` / `shave-enable-vilv` / `shave-widen-three-element-vectors`
  (86726/86727/86631) → optional ISA-extension instructions. **[S]**
* `shave-disable-NaN-checking` (86702) → the default **adds NaN-checking code**
  around FP compares! Arbitrary C with `fcmp` will get extra instructions unless
  disabled. **[S]**

> **D2 — we must implement:** i32 native ALU set incl. `SAU.DIV`; f16/f32 native FP
> with soft-float f64; i64 as explicit pairs with the vendor's concat/extract
> shape; `BUILD_VECTOR`/shuffle/`SPLAT` lowering to CMU/PEU/VAU; reductions through
> `HORIZONTAL_*`; `SELECT/VSETCC` through the **flag protocol** (D5.3); calls +
> varargs; `LDISym`-style symbol materialisation instead of constant pools.

---

## D3 — Scheduling, latency, packets (general model)

(Excerpts and file:line in Part I; this is the rule restatement for implementers.)

| rule | value | evidence | conf |
|---|---|---|---|
| memory-load latency | **6** cycles; hardware may force **8** (feature bit 16) | `FUN_180145fd0` (`MFI+0x190 = min(max(opt=6, hw), per_fn)`), `FUN_180138bd0` (hard default 6), `FUN_180123080` (bits 15/16 → 6/8) | [V]/[H] |
| branch delay slots | **6** | `FUN_1801387b0` (`MFI+0x18c`), `shave-branch-delays` ctor writes 6 | [V]/[H] |
| default latency for any opcode with no itinerary entry | **6** | `FUN_180138bd0`: `if (table < 0) return 6` | [V] |
| latency is a **scheduling distance**, not only a stall count | yes | `FUN_1800e8e80` uses `max(alias_dist, vpu4_dist, MFI+0x190)` as the trace bound | [V] |
| short-latency family | `{0x8e0,0x8f9,0xa4e,0xa63,0xa64}` → latency 1 | `FUN_1801387b0` | [V] |
| pipe composition | ≤1 op per unit over **`BRU CMU IAU LSU0 LSU1 PEU SAU VAU`** | moviAsm64 2557-2641 | [V]/[H] |
| pipe size | **≤128 bits** (`a \|\| b \|\| c`, 1–3 ops in gold) | moviAsm64 `.64`/`.128` blocks + `-noFinalSlotCompression`; peer gold roundtrip | [V]/[H] |
| LSU conflict | a load and a store must not share an LSU in a pipe window; gold = **loads LSU0, stores LSU1** | moviAsm64 `LSU0/LSU1 unit is used more than once…`; peer HW | [V]/[H] |
| aliased store→load | ≥1 extra cycle (`shave-aliased-store-distance`, 0..6, default 1); VPU4 variant `shave-vpu4-load-store-distance` | `FUN_1800e8e80` reads both | [V] |
| LSU policy defaults | loads `prefer-lsu0`, stores `prefer-lsu1`, volatile `use-only-lsu1` | option ctors/help | [S] |
| NOP fill | delay slots / latency holes are filled with NOPs; the **NOP is a real MI** (compressed = no text) | `FUN_180098ca0`; moviAsm `--noCompressedNOPs` | [V]/[H] |
| VLIW slot optimiser | the *compiler* option `shave-disable-slot-optimiser` is **dead**; the live mechanism is the assembler's `SLOT_ALL` promotion for label alignment (`-L`, `--noSlotAllPromo`) | `raw/opt_xrefs.json`; moviAsm64 2747/2757 | [V] |
| pre-RA scheduler | exists, off by default (`shave-prera-scheduler`), factory `FUN_1800eee10` | `decompiled_all.c:162370` + option ctor | [V] |
| post-RA scheduler | **trace scheduler**, on; max-trace-blocks / direction / block weights; can mutate FUs in `LSU0,LSU1,IAU,SAU,VAU,CMU` order | Part I §4; option strings 86752-86768 | [V] |

> **D3 — we must implement:** a post-RA list scheduler that (a) enforces 6-cycle
> load and branch distances, (b) assigns each MI to exactly one of the 8 units,
> (c) packs ≤128 bits per pipe, (d) never puts a load and a store on the same LSU
> in a pipe window, (e) fills the remaining slots with NOP MIs. The vendor's own
> `SHAVEFillDelaySlots.cpp` analogue in our tree is 216 lines and one-instruction-
> per-line — it satisfies none of (b)-(e).

---

## D4 — Pass pipeline (names, order, correctness vs optimisation)

Registration table: DELTA.md §A2. Insertion order recovered from the four
`TargetPassConfig`-style hooks (all in `decompiled_all.c:80395-80490`):

| hook (addr) | passes added, in order | gate | class |
|---|---|---|---|
| `FUN_180084b10 @ 0x180084b10` | **Vector Type Widening** (`FUN_1800b7ae0`) | unconditional | opt (but see D2.4: it changes vector *types* before ISel) |
| `FUN_180084ac0 @ 0x180084ac0` | a pass built by `FUN_1800c5570` — [I] **Anti-Dependency Breaker** (it is gated on `shave-enable-anti-dependency-breaker`, but the *registered* factory for `shaveantidepbreakerpass` is `FUN_1800c1d30`), then `FUN_1800d4610()` | `shave-enable-anti-dependency-breaker` && >1 BB | correctness-adjacent (register lifetimes vs interrupts) |
| `FUN_180084b40 @ 0x180084b40` | **Address Simplifier**, **Pre-RA Scheduler Peephole**, **Pre-RA Scheduler** | `shave-enable-address-simplifier`, `!shave-no-peephole && shave-prera-scheduler-peepholes`, `shave-prera-scheduler` | opt / opt / opt |
| `FUN_180084c00 @ 0x180084c00` | **Pre-Sched2 Peephole**, **Predicator**, **Preemption Handler** | `shave-pre-sched2-peepholes`, (predicator always), `shave-enable-preemption-checks` ≠ 0 | opt / opt / **correctness** (interrupt safety) |
| (registration only) | `shave-loop-load-hoister` (`FUN_1800b41e0`), `shavepostraschedulerpass` (`FUN_1800cd9b0`) | `shave-enable-loop-load-hoisting`; post-RA always | opt / **correctness** (packets+latency) |

Order at the *machine* level is therefore roughly:
`anti-dep breaker → pre-RA scheduler (+peephole) → [ISel → RA]? no: these hooks
are the machine-pipeline hooks, so: SSA-opt(addr simplifier, pre-RA sched) →
RA → post-RA (pre-sched2 peephole, predicator, post-RA trace scheduler) →
preemption handler`. The IR-level widening pass runs before all of them. **[V]**

**Names for our own pipeline** (for `-debug-pass` parity): `shavepreemptionhandlerpass`,
`Loop Load Hoister`, `shave-predicator`, `shavewidenvectortypespass`,
`shaveaddresssimplifierpass`, `shavepreraschedulerpeepholepass`,
`shavepresched2peepholepass`, `shaveantidepbreakerpass`, `shavepostraschedulerpass`,
`shavepreraschedulerpass`, `SHAVE DAG->DAG Pattern Instruction Selection`.

> **D4 — we must implement:** at minimum (1) `shavewidenvectortypespass` before
> ISel, (2) `shavepreraschedulerpeepholepass` + `shavepreraschedulerpass` before
> RA, (3) `shavepresched2peepholepass` + `shavepostraschedulerpass` after RA.
> The rest are optional and must be off unless asked (matching vendor defaults).

---

## D5 — Asm printer, bundling, predicates, sections

### D5.1 Packet text form **[V]**
* A bundle is printed as **one line**: `<instr0>` then, for each further member,
  the literal `'            || '` (12 spaces + `||` + 1 space; string at
  `moviCompile64.ascii.txt:88065`) + `<instrN>`.
* Only bundles with **≥2** members take the packet path
  (`FUN_180098ca0`); 1-member bundles print normally. **[V]**
* The vendor's own reference packet (abort stub, strings 78385) is
  `BRU.SWIH 1 || LSU0.LDIL i18, 0xFFFF || LSU1.LDIH i18, 0xFFFF  // 'abort'`
  — note `//` comments and two spaces before them. **[V]**

### D5.2 Constant materialisation
* `LDIL` → LSU0 (low 16), `LDIH` → LSU1 (high 16), **both in the same pipe** when
  the part has LSU1; split into two pipes otherwise (`FUN_180098410` gated on
  `shave-has-LSU1-fu`). Gold materialises *small* constants with `LDIL` alone
  (peer). **[V]/[S]/[H]**
* `SHAVEISD::LDISym` + `shave-enable-ldi` provide the symbol/constant path; the
  default is **constant pools are used** for BUILD_VECTOR unless
  `shave-disable-constant-pools`. **[S]**

### D5.3 Predicates and flags (the correctness trap)
Peer-verified: **`CMU.CMx.I32 <a>,<b>` does not write a 0/1 value; it sets a FLAG
named by its operand register, and `BRU.BRA <that reg>, target` branches on the
flag** (gold: `CMU.CMZ.I32 i5` @0x50 → `BRU.BRA i5,0xb7` @0x53). **[H]**
The vendor models this with `SHAVEISD::SETCC` + the type/CPU-qualified
`SHAVE_SETCC_*` node family (60 names, e.g.
`SHAVE_SETCC_v4i32_v4i32_Myr4`) and the `CMU_CM` node — i.e. **the compare
producer and the branch consumer are a matched pair**: never materialise 0/1 and
never XOR the condition. This is also why `shave-enable-low-impact-preemption`
and the predicator passes exist (they manage the flag/state registers). **[S]/[H]**
Also: `shave-disable-NaN-checking` shows FP compares carry NaN-check codegen by
default. **[S]**

### D5.4 Section/alignment conventions
* `.code` (compiler, strings 79297) + instructions; the assembler wraps them into
  `.64`/`.128` blocks (`.endb`). **[V]**
* Basic blocks: alignment **1 or 16 bytes only**; aligned targets use `.lalign`
  (`.ealign`, `.salign` also exist, `.code`-only). **[V]**
* Function targets default **16 bytes** (`shave-align-function-targets`,
  `{"defaults to 16-bytes"}`), stack **8 bytes** (`shave-align-stack`). **[S]**
* Section naming is parameterised by `shave-standard-section-prefix` (consumers
  `FUN_180095b60`, `180129010`…`18012ac90`, `180157550`) — the same code that
  builds `.text.<fn>`-style names for `-ffunction-sections`. **[V]**
* NOP mnemonic = `BRU.NOP` (moviAsm64 2968); the compiler-side NOP MI has opcode
  `0xaa9`. **[V]/[H]**

> **D5 — we must implement:** one-line `||` bundles with the exact separator
> literal; `LDIL`/`LDIH` LSU0/LSU1 pairing with the LSU1-less two-pipe fallback;
> the **flag protocol** for `setcc`+`br` (no 0/1 materialisation, no XOR);
> `.lalign` for aligned targets and never a non-1/16 BB alignment; `BRU.NOP` for
> empty slots; `.code` sections.

---

## D6 — Consolidated "we must implement X" list (ranked by impact on general C)

| # | rule we must implement | why it decides correctness for arbitrary C | evidence |
|---|---|---|---|
| 1 | **Flag protocol for compares/branches**: `setcc` → CMU flag in a named register; `br/cselect` consumes that flag; never materialise 0/1 and never XOR the predicate | every `if`, `while`, `for`, `&&`, `?:` in C goes through this; the current materialise+XOR sequence branches on a register the compare never wrote → all control flow is garbage | [H] (gold `CMU.CMZ.I32 i5` → `BRU.BRA i5`), `SHAVEISD::SETCC`/`CMU_CM` [S] |
| 2 | **6-cycle load latency as a scheduling distance** | any C that reads a value it just loaded (every `a[i]`) needs ≥6 slots; our zero-write failure was exactly this | [V] `FUN_180145fd0`, `FUN_180138bd0`; [H] K=6 |
| 3 | **Pipe construction ≤128 bits, ≤1 op per unit over 8 units** | determines whether the stream executes at all; a wrong slot assignment silently drops a store | [V] moviAsm64 2557-2641; [H] |
| 4 | **LSU unit discipline** (loads LSU0 / stores LSU1; never mix within a pipe window; aliased distance ≥1) | C with both loads and stores (everything) corrupts memory otherwise | [V] `FUN_1800e8e80`, option ctors; [H] |
| 5 | **branch delay = 6 slots, filled with useful or NOP instructions** | every loop/early-exit has a branch; unfilled delay slots execute the next sequential instruction ⇒ wrong semantics | [V] `FUN_1801387b0`; [H] |
| 6 | **LDIL/LDIH on LSU0/LSU1 in one pipe** (two pipes without LSU1) | every 32-bit constant in C code (addresses, floats, masks) | [V] `FUN_180098410`; [H] |
| 7 | **call + frame lowering** (i18 arg, i30 ret, `ADJCALLSTACKDOWN/UP`, `SHAVEISD::CALL/RET_FLAG/DYNALLOC`); varargs **unproven** | any non-trivial C calls functions or takes the address of a local | [V] opcode-name array 82700-83000 + DAG-node strings; [V] abort stub |
| 8 | **i64 and div/rem lowering**: i64 as explicit pairs (concat/extract), i32 div → `SAU.DIV`, otherwise libcalls; f64 → soft-float | `long long`, `%`, `/`, `long double`-free FP in ordinary C | [S] `SHAVEISD::I64_*`, `shave_sau_div_i32_rr`, `-D__SOFTFP__` |
| 9 | **Vector lowering**: BUILD_VECTOR/shuffle → `SPLAT`/`LANESPLAT`/`UNPACK`, reductions → `HORIZONTAL_*`, sub-vector → `INSERT/EXTRACT_SUBVECTOR`; decide constant pools vs `LDISym` | vector C (the kernels we care about) | [S] node list; `shave-disable-constant-pools` |
| 10 | **Alignment & section conventions**: BB alignment ∈ {1,16}, `.lalign` for targets, 16-byte function alignment, `.code` sections, `BRU.NOP` fills | byte-exactness and label placement; unaligned targets stall (>128 bits) | [V] `FUN_180097e00`; [S] option help; moviAsm64 2476-2491 |
| 11 | **Thread-safety / interrupt-safe scheduling** (`shave-enable-interrupt-friendly-scheduling`, preemption handler, anti-dep breaker) | preemption can observe half-updated state; matters once kernels are interruptible | [S] option ctors; `FUN_1800b10a0` |
| 12 | **Stack instrumentation** (`shave-track-stack-usage`, `shave-stack-overflow-checking` → `__stackHighWater`, `__stackMaximumExtent`) | only if the runtime provides the symbols; off by default | [V] decompiled_all.c:170412/172315 |

Confidence legend: [V] verified in the vendor binary, [S] vendor string/table
inference, [H] hardware-verified by a peer, [I] stated inference.

---

## D7 — Ancestry: which SHAVE rules are Hexagon-derived, and what the fork moved

Cheap check on the pristine upstream tree (`git -C /home/asdf/projects/llvm-14 …`),
no build needed:

| upstream LLVM 14 artefact | SHAVE counterpart in the vendor fork |
|---|---|
| `llvm/include/llvm/CodeGen/DFAPacketizer.h` → `class VLIWPacketizerList`, `class DFAPacketizer`; TableGen backend `llvm/utils/TableGen/DFAPacketizerEmitter.cpp` (`-gen-dfa-packetizer`) | **no packetizer pass exists in the vendor compiler** (the 11 registered SHAVE passes contain no packetizer; `shavepostraschedulerpass` does FU *mutation*, not DFA validation) → SHAVE moved packet legality **into the assembler** (moviAsm64: one op per unit over `BRU CMU IAU LSU0 LSU1 PEU SAU VAU`, `.64`/`.128` blocks, `SLOT_ALL`, NOP compression) |
| `llvm/lib/Target/Hexagon/HexagonVLIWPacketizer.cpp`: `isSoloInstruction()`, `isPureSlot0()`, `static cl::opt<bool> Slot1Store("slot1-store-slot0-load", cl::Hidden, …)` | the same *idea* appears as SHAVE's LSU rules: "load and store must not share an LSU", `LSU0.LDIL`+`LSU1.LDIH` pairing, `shave-lsu-{load,store,volatile}-policy` — but expressed as option-driven FU assignment plus the assembler's pipe-conflict check rather than Hexagon's SLOT0/SLOT1 packetizer predicates |
| `llvm/lib/Target/Hexagon/HexagonVLIWPacketizer.h` | SHAVE has no equivalent header in the binary's symbol/string surface |
| generic `-sparc-delay-slot-filler` (Sparc) | SHAVE's delay-slot filling is **not** the generic filler: it comes from the post-RA scheduler + printer NOP fill, and `-forceDelaySlots` lives in moviAsm64 |

**Consequence for us (actionable):** we do **not** need to port Hexagon's DFA
packetizer. To match the vendor we need (a) an MI-bundle producer in the backend
(slot assignment via FU choice, ≤1 op/unit, ≤128 bits) and (b) an assembler that
*validates* the pipe and packs it into `.64`/`.128` blocks — i.e. the division of
labour the vendor uses. Implementing DFA validation in the compiler would be
extra machinery the vendor does not have, and would not change the emitted text.

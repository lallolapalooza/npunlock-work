# DELTA.md — vendor `moviCompile64.dll` vs upstream LLVM 14: the SHAVE-specific delta

Scope: isolate *only* the SHAVE-specific surface of the vendor compiler, then point
each item at the decompiled function(s) that implement it. This is the "what is
different" index; `ANNOTATED_DELTA.md` is the "what it does / what rule it enforces".

Vendor binary: `extracted/.../MVC_DEPEND/bin/moviCompile64.dll` (65,488,136 B,
PE32+, imagebase `0x180000000`, `.text` 50,686,464 B, 82,549 functions decompiled).
Upstream reference: `/home/asdf/projects/llvm-14` @ `f28c006a5` ("Bump version to
14.0.6", `release/14.x`) — read **through git** (`git show HEAD:<p>`,
`git grep … HEAD`), never from the dirty worktree.

---

## 0. Method (reproducible)

| tool | purpose |
|---|---|
| `tools/strva.py <dll> <needle>` / `--addr <va>` | map a string literal to its VA (PE section walk) |
| `tools/xref.py <dll> <needle>` | string VA → the 8‑byte pointer slot Ghidra names `s_<text>_<addr>` |
| `tools/optxref2.py` | one pass over `decompiled_all.c`; for every SHAVE `cl::opt` *object* reports the functions that touch it (window `[base, base+0x180)`, minus overlaps from adjacent option objects) |
| `tools/lines2fn.py line|range|top` | line number ↔ `FUN_<addr>` ↔ size, from `03_function_index.tsv` |
| `raw/*.txt`, `raw/*.c`, `raw/opt_xrefs.json` | frozen evidence |

Key layout facts established while doing this (needed to read any excerpt):

* **`cl::opt` object layout (MSVC, this build).** Registration functions are
  functions 3…56 of `decompiled_all.c` (addresses `0x180003600`…`0x180007a40`,
  in *alphabetical* option order). Each does
  `FUN_180159aa0(&OBJ,0,0)` (ctor) → field init → `FUN_180e325a0(&OBJ,&{name,len})`
  → `FUN_180e2ab90(&OBJ)` (register) → `atexit(...)`.
  The **value field is at `OBJ+0x80`** (e.g. `shave-branch-delays` object
  `DAT_183c6e0b8` → value `DAT_183c6e138`, set to 6 in the ctor; read at
  `decompiled_all.c:223131`; `shave-load-latency` object `DAT_183c6eb18` → value
  `DAT_183c6eb98`, ctor writes 6, read in `FUN_180145fd0`). Verified for bool
  options too (`shave-disable-nop-compression` object `0x183c6d4a0`, ctor writes
  `DAT_183c6d520 = 0`, read as `DAT_183c6d520 == '\0'` in `FUN_180098ca0`).
  *Caveat:* a raw `--addr` dump of a value field shows the **pre-static-init**
  bytes of the `.data` image (the registration routines run before any use), so
  the ctor writes and — above all — the *reader* sites are what pin the address.
  Option objects are packed alphabetically, so an object's *neighbour's* base
  sits as close as `+0xE0` — naive ±window scans produce false xrefs;
  `optxref2.py` excludes the `[other_base, other_base+0x100)` range of every
  other option.
* String literals that appear verbatim in `decompiled_all.c` (RIP-relative `lea`
  at the use site, e.g. every `cl::opt` name) are quoted directly; strings that
  live only in a generated table are indexed at runtime and do **not** appear.

---

## 1. Headline numbers

| surface | count | all upstream? |
|---|---|---|
| SHAVE `cl::opt` options (`-shave-*`) | **54** | no |
| SHAVE machine passes (registered pass names) | **11** | no |
| SHAVE-specific analyser/CLI names (`-shave-*`, `-shave-predicator`, `shave-loop-load-hoister`) | 3 | no |
| `SHAVEISD::` DAG nodes | **36** including `SHAVEISD::HORIZONTAL_ADD` / `SHAVEISD::CLAMP` / `SHAVEISD::LDISym` | no — LLVM 14 has **no** `HORIZONTAL_*` node anywhere |
| subtarget `-mcpu` names | 41 (see §A4) | no |
| driver-level `-mshave-*` options | 3 | no |
| assembler directives that are SHAVE-only | `.lalign` | no |
| C++ pass-description strings `SHAVE … Pass` | 11 | no |

Classification procedure: every candidate identifier was grepped against a
pristine copy of upstream LLVM 14 — the committed tree, **not** the worktree
(which our own agent has modified with a `SHAVE` target), using
`grep -rIl --exclude-dir=SHAVE <term> llvm/lib/Target clang/lib` plus
`git show HEAD:<file>` spot checks for the SHAVE/Myriad references.
Result table: **`raw/upstream_classification.tsv`** (`pattern ⇥ #upstream files ⇥ class`);
116 of 130 candidates are VENDOR-ONLY. Control group (`Bundle`,
`VLIWPacketizerList`, `HazardRecognizer`, `ScoreboardHazardRecognizer`,
`MachineScheduler`, `MachineTraceMetrics`, `SUnit`, `ScheduleDAGInstrs`,
`TargetInstrInfo`, `TargetSubtargetInfo`, `RegisterPass`, `Packetizer`)
all hit upstream → the method does not simply return "delta" for everything.
The one non-obvious `upstream` hit is the literal string `SHAVE`: upstream clang
already contains the **Myriad driver toolchain** that *drives* `moviCompile`
(§3.5) — but upstream has no LLVM backend for it.

---

## 2. CLASS A — vendor-specific (the delta we care about)

### A1. `-shave-*` `cl::opt` table (54 entries)

`src` = strings line in `re_strings/moviCompile64.ascii.txt`;
`obj` = `.data` address of the option object (value at `obj+0x80`);
`consumer` = functions that touch the option object, **registration function
excluded** (addresses `0x180003600`…`0x180007a40` are the ctors themselves).

| option | src | obj | ctor/reg | default (from ctor + `-help` text) | consumers |
|---|---|---|---|---|---|
| `shave-aliased-store-distance` | 86659 | `183c703c8` | `180003600` | `1`, range 0..6 — *"Minimum number of additional cycles between a store on one LSU followed by an aliased load on the other LSU"* | `1800d6ee0`, `1800e8e80` |
| `shave-align-all-branch-targets` | 86712 | `183c704a8` | `180003740` | false — *"Align all target labels to a 16-byte boundary"* | `180097e00` |
| `shave-align-function-targets` | 86710 | `183c702e8` | `180003850` | *"Align function labels to the specified boundary {defaults to 16-bytes}"* | `180098410`, `18011eca0` |
| `shave-align-jump-targets` | 86714 | `183c6de18` | `180003970` | false | `180097e00` |
| `shave-align-loop-targets` | 86716 | `183c6f860` | `180003a80` | false | `180097e00`, `18011eca0` |
| `shave-align-stack` | 86708 | `183c6d660` | `180003b90` | 8 bytes | `180123080` |
| `shave-average-instruction-size` | 86657 | `183c6e5f8` | `180003cb0` | 5.0 (*"…at '-O3' in Bytes"*) | `1800e8800` |
| **`shave-branch-delays`** | 86667 | `183c6e0b8` | `180003df0` | **6**, 0..INT_MAX | `180145fd0` |
| `shave-disable-constant-pools` | 86647 | `183c6e958` | `180003f30` | false | `18011af00` |
| `shave-no-replace-jmp-with-bra` | 86695 | `183c6e278` | `180004040` | false | `1800e8e80` |
| **`shave-disable-nop-compression`** | 86700 | `183c6d4a0` | `180004150` | false — *"Disable the use of NOP compression"* | **`180098ca0`** |
| `shave-disable-NaN-checking` | 86702 | `183c70800` | `180004260` | false | `180107720`, `18010f840` |
| `shave-no-peephole` | 86703 | `183c6d900` | `180004370` | false | `180084b40`, `1800e8e80` |
| **`shave-disable-slot-optimiser`** | 86699 | `183c6dd38` | `180004480` | false — *"Disable the VLIW slot optimiser"* | **none read it** (see §4) |
| `shave-enable-address-simplifier` | 86730 | `183c6f5b8` | `180004590` | false | `180084b40` |
| `shave-enable-anti-dependency-breaker` | 86731 | `183c70048` | `1800046a0` | false | `180084ac0` |
| `shave-generate-fp-acc-mac` | 86651 | `183c6fcc8` | `1800047b0` | false | `180100250`, `18011eca0` |
| `shave-generate-int-acc-mac` | 86653 | `183c6e198` | `1800048c0` | false | `1800ff260`, `18011eca0` |
| `shave-inplace-mutation` | 86767 | `183c6d580` | `1800049d0` | false — *"Enable the in-place mutation of already scheduled instructions"* | **`1800e0a70`** |
| `shave-enable-interrupt-friendly-scheduling` | 86769 | `183c6d9e0` | `180004ae0` | false — *"Prevent the scheduler from overlapping register lifetimes for anti-dependent instructions"* | `1800ee100` |
| `shave-enable-ldx-and-stx-instructions` | 86705 | `183c6d3c0` | `180004bf0` | false | `18009e2e0`, `1800a7d50` |
| `shave-enable-loop-load-hoisting` | 149634 | `183c6fda8` | `180004d00` | false (*"UNSAFE"*) | `1800eb310` |
| `shave-enable-low-impact-preemption` | 86744 | `183c6ea38` | `180004e10` | false | `1800b36c0` |
| `shave-prera-scheduler` | 86749 | `183c70b58` | `180004f20` | false — *"Use the SHAVE scheduler pre-register-allocation"* | `180084b40` |
| `shave-pre-sched2-peepholes` | 86725 | `183c6fe88` | `180005030` | false | `180084c00` |
| `shave-enable-preemption-checks` | 86742 | `183c70588` | `180005140` | 3-valued (`restore`/`norestore`/off) | `180084c00`, `18009c220`, `1800b3e50` |
| `shave-stash-retrieve` | 86718 | `183c6e518` | `180005480` | false — *"…VRF stash-retrieve logic to avoid spilling IRF registers to stack"* | `1800f9fe0`, `18014acd0` |
| `shave-has-floating-point-unit` | 86655 | `183c6ff68` | `180005590` | false | `18011eca0` |
| `shave-has-LSU1-fu` | 86659 | `183c6d820` | `1800056a0` | — *"Enable the second Load-Store functional-unit LSU1"* | `180098410`, `1800e0a70`, `1800e13e0`, `1800e76f0`, `1800ed130`, `1800fbca0` |
| `shave-has-SAU-fu` | 86660 | `183c6ebf8` | `1800057b0` | — | `1800ff260`, `180100250`, `180107720`, `1801081c0`, `18011eca0`, `180145f90` |
| `shave-has-VAU-fu` | 86661 | `183c6f940` | `1800058c0` | — | `1800ff260`, `180100250`, `1801081c0`, `18011eca0`, `180145fa0`, `180146b30` |
| `shave-inline-assembly-warning-threshold` | 86721 | `183c6dfd8` | `1800059d0` | int | (none outside ctor) |
| `shave-postra-scheduler-inner-loop-block-weight-modifier` | 86756 | `183c6fbe0` | `180005af0` | int | **`1800da4a0`** |
| `shave-lsu-flush-on-enter` | 86693 | `183c6fa20` | `180005c10` | false — *"Insert code to flush both LSUs on entry to a function"* | **`1800fbca0`** |
| `shave-lsu-load-policy` | 86779 | `183c6efb0` | `180005d20` | `prefer-lsu0` (`use-only-lsu1`/`use-only-lsu0`/`prefer-lsu1`/`prefer-lsu0`) | `1800e1b80`, `1800ed130` |
| `shave-lsu-store-policy` | 86784 | `183c708e0` | `180006100` | `prefer-lsu1` | `1800d6ee0`, `1800e1b80`, `1800ed130` |
| `shave-lsu-volatile-load-store-policy` | 86788 | `183c6e6e0` | `1800064e0` | `use-only-lsu1` | `1800d6ee0`, `1800ed130`, `1800fd610`, `180103270` |
| **`shave-load-latency`** | 86671 | `183c6eb18` | `1800067d0` | **6**, 6..UINT_MAX | `180145fd0` |
| `shave-max-inline-assembly-lines` | 86719 | `183c6d740` | `180006910` | int | `1814618a0` |
| `shave-postra-scheduler-max-trace-blocks` | 86752 | `183c6def8` | `180006a30` | int | **`1800daaf0`** |
| `shave-scheduler-max-tracked-mem-ops` | 86754 | `183c6e438` | `180006b50` | int | `1800c5740`, `18014dc60` |
| `shave-postra-scheduler-functional-unit-mutation-order` | 86768 | `183c6edb8` | `180006c70` | enum, desc *"default:LSU0,LSU1,IAU,SAU,VAU,CMU"* | `1800e0a70`, `1800e76f0` |
| `shave-postra-scheduler-outer-loop-block-weight-modifier` | 86758 | `183c6f778` | `180006ce0` | int | `1800da4a0` |
| `shave-prera-scheduler-peepholes` | 86724 | `183c70c38` | `180006e00` | false | `180084b40` |
| `shave-preemption-max-loop-depth` | 86746 | `183c70208` | `180006f10` | int (*"1 = innermost loops only"*) | `1800b3e50` |
| `shave-standard-section-prefix` | 86706 | `183c6f228` | `180007030` | string | `180095b60`, `180129010`, `180129400`, `180129e70`, `18012a3d0`, `18012a4a0`, `18012a8e0`, `18012ac90`, `180157550` |
| `shave-stack-overflow-checking` | 86776 | `183c6e358` | `1800070a0` | false | `1800facf0`, `180103270`, `18011eca0` |
| `shave-postra-scheduler-trace-generation-direction` | 86765 | `183c6f340` | `1800071b0` | `both`(`backwards`/`forwards`) | **`1800daaf0`** |
| `shave-track-stack-usage` | 86777 | `183c6eed0` | `1800074f0` | false | `1800fd610`, `180103270`, `18011eca0` |
| `shave-enable-ldi` | 86726 | `183c70128` | `180007600` | int | `180107720`, `1801081c0` |
| `shave-nop-sync-after-inline-asm` | 86722 | `183c6ecd8` | `180007710` | false — *"Emit a 'NOP SYNC' instruction after inline-assembly"* | **`180149a20`**, **`18015f870`** |
| `shave-enable-vilv` | 86727 | `183c6fb00` | `180007820` | false | `18011c2c0`, `180155220` |
| `shave-vpu4-load-store-distance` | 86772 | `183c6f698` | `180007930` | int | `1800d6ee0`, `1800e3050`, `1800e8e80`, `1800f4df0` |
| `shave-widen-three-element-vectors` | 86631 | `183c6dac0` | `180007a40` | `load-only` (`load-and-store`/`store-only`/`load-only`/`disable`) | `1800b94c0` |

Additional `-shave-*` names that exist only as *help text/description* in the
binary (strings 86630-86900) and therefore are not in the table above:
`shave-disable-constant-pools`, `shave-has-LSU1-fu`, `shave-has-SAU-fu`,
`shave-has-VAU-fu`, `shave-loop-load-hoister`, `shave-predicator`,
`shave-preemption-checks`, `shave-low-impact-preemption`, `shave-inline-assembly-warning-threshold`,
`shave-max-inline-assembly-lines`, `shave-standard-section-prefix`, `shave-align-stack`,
`shave-enable-vilv`, `shave-enable-ldi` — all are covered above via their objects.

Notable *non-`shave-`* options the SHAVE target introduces (same ctor block):
`-shave-*` siblings `target-cpu`, `target-features`, plus driver-only
`-mshave-preemption-checks`, `-mshave-low-impact-preemption`,
`-mshave-preemption-max-loop-depth` (strings 149xxx)
and the driver's predefined macros `-D__SHAVE_BRANCH_DELAY_SLOTS__=` and
`-D__SHAVE_MEMORY_LOAD_LATENCY__=` (strings 151905-151906) — the two numbers
from §A1 are **exported to the preprocessor**.

### A2. SHAVE machine passes (11)

Pass CLI names are the `cl::opt`-style strings registered through `RegisterPass`;
each is set up by a function that allocates a `PassInfo` and stores the
pass-factory pointer in `PassInfo+0x90` (`*(code **)(puVar2 + 0x12)`).

| pass CLI name | pass title (string) | strings line | registered by (`RegisterPass_xxx`) | factory (creates the pass object) |
|---|---|---|---|---|
| `shavepreemptionhandlerpass` | `SHAVE Interrupt Handler Pass` / `SHAVE Preemption Handler Pass` | 78419-78421 | `FUN_1800b1190` (line 112305) | `FUN_1800b10a0` |
| `Loop Load Hoister` | `SHAVE loop load hoister pass` | 78439-78441, 115240 | `FUN_1800b5c30` | `FUN_1800b41e0` |
| `shave-predicator` | `SHAVE Predicator` | 78438 / 115285 | `FUN_1800b5d20` (line 115286) | `FUN_1800b4220` |
| `shavewidenvectortypespass` | `SHAVE Vector Type Widening Pass` | 78445 | `FUN_1800b7b10` (line 117181) **and** `FUN_1800b8b30` | `FUN_1800b7ae0` |
| `shaveaddresssimplifierpass` | `SHAVE Address Simplifier Pass` | 78447 | `FUN_1800ba410` (line 119285) | `FUN_1800ba3e0` |
| `shavepreraschedulerpeepholepass` | `SHAVE Pre-RA Scheduler Peephole Pass` | 78451 | `FUN_1800bbbc0` (line 120513) **and** `FUN_1800bc3b0` | `FUN_1800bbb90` |
| `shavepresched2peepholepass` | `SHAVE Pre-Sched2 Peephole Pass` | 78453 | `FUN_1800bd900` (line 121979) | `FUN_1800bd8d0` |
| `shaveantidepbreakerpass` | `SHAVE Anti-Dependency Breaker Pass` | 78459 | `FUN_1800c9730` (line 132047) | `FUN_1800c1d30` |
| `shavepostraschedulerpass` | `SHAVE Post-RA Scheduler Pass` | 78462 | `FUN_1800e3ea0` (line 153712) | **`FUN_1800cd9b0` → ctor `FUN_1800cded0`** (object 0x2b8 B, vtable `0x1830719d8`) |
| `shavepreraschedulerpass` | `SHAVE Pre-RA Scheduler Pass` | 78464 | `FUN_1800f5c40` (line 166545) | `FUN_1800eee10` |
| `SHAVE DAG->DAG Pattern Instruction Selection` | (SelectionDAG ISel) | 78407 | — | — |

(All ten SHAVE `-xxxpass` names are also listed in `clang`'s `-Xclang -help-hidden`
style pass tables, hence two registration sites for two of them.)

`shavepostraschedulerpass` object size = `0x2b8` (`FUN_1800cd9b0` =
`new T()`), ctor `FUN_1800cded0` sets `vtable = 0x1830719d8`. Dumping that vtable
(`tools/strva.py` + section walk) gives
`+0x00 1800cef40`, `+0x10 1800aff20`, `+0x20 1808bcc80`, `+0x30 1808bf590`,
`+0x48 1800dba60`, `+0x78 1808bc500`, `+0x80 18062da50`, **`+0x88 1800e8e80`** —
the last is the pass body (3010 B; it is also the only function that reads both
`shave-no-peephole` and `shave-vpu4-load-store-distance`).

### A3. `SHAVEISD::` DAG nodes (36, strings 78383-78420 + 78470-78510)

`ACCN_SEQ ACCP_SEQ CALL CLAMP CMU_CM CMU_CPII DYNALLOC EXTRACT_SUBVECTOR
FPTOUI_INREG HORIZONTAL_ADD HORIZONTAL_AND HORIZONTAL_FADD HORIZONTAL_OR
HORIZONTAL_XOR I64_CONCAT I64_EXTRACT_HIGH I64_EXTRACT_LOW INSERT_SUBVECTOR
LANESPLAT LDISym MACN_SEQ MACP_SEQ MAX MIN RET_FLAG SELECT SETCC SHD SHLV
SPLAT TRUNCATE_INREG UITOFP_INREG UNPACK VECTOR_EXTEND VSELECT`
plus the `SHAVE_SETCC_*` node family (60 names, strings 78700-79000) which is
*type- and CPU-qualified*: `SHAVE_SETCC_{v2,v4,v8,v16,v32,v64}{i8,i16,i32,f16,f32}_…[_Myr4]`,
i.e. the vendor keeps a distinct `SETCC` DAG node per vector width **and per
Myriad revision** (`_Myr4` suffix).

`HORIZONTAL_ADD`/`HORIZONTAL_AND`/`HORIZONTAL_OR`/`HORIZONTAL_XOR`/`HORIZONTAL_FADD`
do not exist anywhere upstream (grep, §1) → purely SHAVE (horizontal reduction ops).

### A4. Subtarget `-mcpu` names (strings 86572-86631, 78506)

`3000xx 3010A0 3010A0-nn 3010B0 3010B0-nn 3010xx 3010xx-nn 3100xx 3100xx-nn
3600xx 3600xx-nn 3700xx 3700xx-nn 3710xx 3710xx-nn 3720xx 3720xx-nn 3730xx
3730xx-nn 3740xx 3740xx-nn 4000xx 4010xx 5000xx 5010xx ma2080 ma2085 ma2090
ma2090-nn ma2095 ma2095-nn ma2100 ma2150 ma2155 ma2450 ma2455 ma2480 ma2485
ma2490 ma2490-nn ma2495 ma2495-nn ma2x5x ma2x8x ma2x9x ma2x9x-nn myriad2.1
myriad2.2 myriad2.3 myriad2.4 myriad2.4-nn vpu20-nn`
(`-nn` = "no NN"/no-neural-engine variants). Predefined macros: `__myriad2`,
`__myriad2__` (strings 209928-209929); bug-report URL `http://www.movidius.org`.

### A5. Scheduling model surface

* Functional units (the scheduler's mutation order default is the canonical list):
  **`LSU0, LSU1, IAU, SAU, VAU, CMU`** → up to 6 issue slots per packet.
* Generated *processor resources* (strings 86541-86571): `PR_VRF_R0…R5`,
  `PR_VRF_W0…W5`, `PR_V_ACC0…3`, `PR_V_STATE`, `SHARED_RES_S0…S5,S8,S9`
  → the model tracks **vector register-file read/write ports**, the **four vector
  accumulators** and the **vector state register**.
* Special registers (strings 88020-88058): `B_STATE`, `I_STATE`, `S_STATE`,
  `V_STATE`, `B_ISR_SA`, `B_LEND`, `B_LBEG`, `G_GALOIS`, `B_MREPS`, `B_SREPS`,
  `P_SVID`, `TRF_17`, `C_CMU0…C_CMU15`, `C_CMU0123`, `C_CMU01234567`,
  `C_CMU0123456789ABCDEF`, `C_IAU0`, `C_SAU0..3`, `C_CMU01`, `V_ACC0..3`,
  `V_ACC00/01/10/11/12/02/13/03`, `S_ACC0/S_ACC1`, `B_IP_0`.
* Register classes (strings 79290-79300, 86500-86540): `VRF32_q0_1`, `VRF64_h`,
  `VRF32_qN`, `WVRF64_0`, `WVRF128_0`, `WVRF256_0`, `WVRF512` (the `W…`
  write-only classes), `CSR_EmptySHAVE`, `CSR_OrigSHAVE`, `CSR_StdSHAVE`
  (call-preserved-register masks), `W…` + `SELECT_WVRF*` pseudo ops.
* Pseudo-instruction names in the SHAVE assembly/string tables
  (79290-79410, 87800; *not* the MI opcode-name array): `FLUSH_LSU0`, `FLUSH_LSU1`,
  `SELECT_WVRF64_0`, `SELECT_WVRF128_0`, `SELECT_WVRF256_0`, `LSU_CP16_vrf_irf0`,
  `LSU_CPFL1`, `CMU_LUT_Pseudo`, `CMU_LUTW_Pseudo`, `ADJCALLSTACKDOWN`,
  `ADJCALLSTACKUP`, `DYNALLOC`, `IAU_LMULH/MULSI/LMULL/MULSU_Myr2v1`,
  `VAU_CMBBYTE_imm_vrf_Myr2v1`, `PEU_PVEN8C_Myr2v1`, `CMU_VSZMBYTE_imm_vrf64_Myr2v1`
  (the `_Myr2v1` suffix = "Myriad 2 revision 1 workaround" set).

### A6. Vendor error/assert strings that encode rules (see `ANNOTATED_DELTA.md`)

| string | strings line | meaning |
|---|---|---|
| `Not expecting to see the Basic-Block alignment not equal to zero or 2^^4 for SHAVE; got 2^^` | 78396 | BB alignment may only be **1 or 16 bytes**; anything else is a fatal (`swi 3`) error |
| `    .lalign` | 78397 | SHAVE-only assembler directive emitted for aligned targets |
| `BRU.SWIH 1 \| LSU0.LDIL i18, 0xFFFF \| LSU1.LDIH i18, 0xFFFF  // 'abort'` | 78385 | the canonical 3-slot packet: branch **and** both halves of a constant load in one bundle |
| `LSU0.LDIL i18, 0xFFFF` + `BRU.SWIH 1 \| LSU0.LDIH i18, 0xFFFF` | 78386-78387 | the same abort **without LSU1**: the constant is split over two packets |
| `'\n' contains no instructions, so force an immediate 'abort'` | 78401-78403 | empty-function fallback |
| `Have you ensured that there are enough trailing NOPs to satisfy all outstanding instruction latencies` / `See 'moviCompile.pdf' section 11.2.1` | 88085-88087 | inline-asm contract: the *user* must pad latency |
| `NOP SYNC  // Wait for outstanding schedules to complete` | 79130 | the emitted stall idiom |
| `Number of cycles after issuing a memory load before the register is updated` | 86672 | `shave-load-latency` = 6 |
| `Number of delay slots following a branch` | 86667 | `shave-branch-delays` = 6 |

### A7. Where the SHAVE passes are inserted (the pipeline hooks)

The pass-manager hooks are ordinary `TargetPassConfig` overrides and are
identifiable because they call the *pass factories* of §A2 through
`PassManager::addPass`:

| hook (function) | condition | passes added |
|---|---|---|
| `FUN_180084b40 @ 0x180084b40` (192 B, `decompiled_all.c:80435`) | `shave-enable-address-simplifier` VALUE `0x183c6f638` | `FUN_1800ba3e0()` = **Address Simplifier** |
| ″ | `!shave-no-peephole` (`0x183c6d980 == 0`) && `shave-prera-scheduler-peepholes` (`0x183c70cb8`) | `FUN_1800bbb90()` = **Pre-RA Scheduler Peephole** |
| ″ | `shave-prera-scheduler` (`0x183c70bd8`) | a third pass via `FUN_18060ba00(pm, …)` (pre-RA scheduler path) |
| `FUN_180084c00 @ 0x180084c00` (98 B, `decompiled_all.c:80469`) | `shave-pre-sched2-peepholes` (`0x183c6ff08`) | `FUN_1800bd8d0()` = **Pre-Sched2 Peephole** |
| ″ | `shave-enable-preemption-checks` (`0x183c70608` ≠ 0, tri-state) | `FUN_1800b10a0()` = **Preemption Handler** |

(The value-field addresses quoted are `obj+0x80` of the matching entries in
§A1; this is the check that the ±window xrefs are the real option reads.)

See §A8 for how the driver drives the assembler, and §A10 for the assembler-side
delta that completes these rules.

### A8. Driver-level statements about the assembler contract

`clang/lib/Driver/ToolChains/Myriad.cpp` **is upstream** and reveals the exact
external-tool contract, including one fact that matters for the packet format
and is otherwise invisible:

```cpp
void tools::SHAVE::Assembler::ConstructJob(…) {
  CmdArgs.push_back("-no6thSlotCompression");      // ← upstream clang, HEAD
  const Arg *CPUArg = Args.getLastArg(options::OPT_mcpu_EQ);
  if (CPUArg) CmdArgs.push_back("-cv:" + CPUArg->getValue());
  CmdArgs.push_back("-noSPrefixing");
  CmdArgs.push_back("-a");                         // "Mystery option."
  … "-i:<include>" … "<in>.s" "-o:<out>.o"
}
```

* **`-no6thSlotCompression`** — ⚠ **stale switch**: it appears in *upstream*
  clang's Myriad driver but **nowhere in either vendor binary**
  (`strva.py` on `moviCompile64.dll`/`moviAsm64.dll` → "not found"), so the
  fork dropped it. The vendor driver instead emits `--noSPrefixing --cv <cpu>`
  (two-token form) plus any `-Wa,…` pass-through
  (`decompiled_all.c:3243041-3243049`). The live compression knobs live in
  moviAsm64: `--noCompressedNOPs`, `--noFinalSlotCompression`, `--noSlotAllPromo`.
  Independently, the *6 functional units* (`LSU0,LSU1,IAU,SAU,VAU,CMU`) plus the
  assembler's 8-unit conflict rule fix the pipe shape as **up to 6 data-path
  slots / ≤128 bits**, which is the same 6 as `shave-load-latency` /
  `shave-branch-delays`.
* `-cv:<cpu>` carries the `-mcpu` into the assembler (moviAsm64 accepts both
  `-cv:` and `--cv`); `-noSPrefixing` disables `S.`-style section prefixes.
* The compiler side passes its own flags to moviCompile unchanged ("These are
  spelled the same way in clang and moviCompile" — `-I -D -U -f… -g -M -O -W
  -mcpu= -mllvm -Xclang`), which is why every `-shave-*` option is a *cc1*
  option and reaches the codegen through `-Xclang`/`-mllvm`.

### A9. Also SHAVE-only, found in the driver/CLI area

* `-mno-packets` / `-mpackets`, *"Disable/Enable generation of instruction packets"*
  (strings 147823/147924/149688/149719) — packetization on/off switch.
* `SHAVE::Assemble` → external assembler **`moviAsm`** and `SHAVE::Link` →
  **`sparc-myriad-rtems-ld`** (decompiled lines 3245558, 3245576): the compiler
  emits *text*, so byte-level packet descriptors are the assembler's job.
* Target registration: `FUN_180160d90` →
  `FUN_180c3ba60(&DAT_183c72780,"shave","Myriad SHAVE Processor","SHAVE", &LAB_180160d80, 0)`
  (decompiled line 242755).

### A10. The assembler-side delta (`moviAsm64`) that completes the rule set

`moviAsm64.dll` is hand-written C++ (it has RTTI: `.?AVERR_INSTRUCTION_IS_CONFLICTING@Log@@`,
`.?AVERR_NOP_IN_PIPE@Log@@`, `.?AVERR_ALIGNMENT@Log@@`, …). Its strings/help
(`re_strings/moviAsm64.ascii.txt`) contribute rules that the compiler side cannot
express — **all cross-checked in `ANNOTATED_DELTA.md` §11**:

| item | strings | rule |
|---|---|---|
| pipe unit conflict | 2557/2559/2570/2616/2617/2630/2632/2641 | `BRU`, `CMU`, `IAU`, `LSU0`, `LSU1`, `PEU`, `SAU`, `VAU` — **each at most once per pipe** (8 units; the compiler's *mutable-FU* list names only 6 of them) |
| pipe syntax | 2615, 2701-2703 | `Line ended with <||> sequence`, `Double pipe found`, `Invalid continued pipe after %s`, `Invalid NOP in pipe` |
| code blocks | 2553-2555, 2560, 2568, 2628, 2642 | `.code` section holds `.64`/`.128` blocks ending in `.endb` → **a pipe is ≤128 bits** |
| alignment | 2476-2477, 2491, 2592, 2747 | `Unaligned target instruction might generate a stall (> 128 bits)`, `-L : Optimize labels' address (align to 128 bits)`, `.lalign is valid for .code sections only` |
| delay/latency padding | 2421-2431, 2492, 2717-2718 | `-forceDelaySlots:<n>`, `-forceLoadDelay:<n>`, `There are side effects after delay slots, remaining latency cycles: %d` |
| NOP/slot compression | 2425-2428, 2755-2757 | `--noCompressedNOPs` (*"No NOP compression in delay slots (ma2xxx only)"*), `--noFinalSlotCompression` (*"…final delay slot"*), `--noSlotAllPromo` (*"Disable the SLOT_ALL promotion for labels alignment"*). NB the upstream-clang switch `-no6thSlotCompression` is **not implemented** in moviAsm64 (§A8) |
| analyzer | 2737-2741, 2478, 2681 | `-a : Disable the code analyzer`; `Instruction %s is conflicting: %s` / `… may be conflicting: %s`; `Analyzer fail: %s` |
| NOP mnemonic | 2968 | `BRU.NOP` |
| ISA extent | 2470-2472, 2762, 3180-3182, 3378, 3469, 4709-4717 | `-arch <targetMachine> : Create ELF object file for a specific target machine (sparc, arm)`; CPUs `3600xx 3700xx 3710xx 3720xx 3730xx 3740xx 4000xx 4010xx 5000xx 5010xx 5020xx`; `VAU.ALIGNBYTE`, `CMU.ALIGNVEC`, `VAU.ALIGNWORD`, `SAU.ALIGNBYTE`, `IAU.ALIGN`, `CMU.ALIGNDVEC`, `CMU.COMPRESS.X8/X16/X32`, `PEU.PVV8/16` |

**Key consequence:** the *compiler* never emits `.128`/`.64`/`.endb`/`SLOT_ALL`
(only `.code` and `.lalign`), so packet *blocking* and slot/slot-compression are
the assembler's job. Our backend therefore only has to produce correct
`a || b || c` pipe lines + NOP fills; the byte-level packing belongs to
`shave_asm.py`.

---

## 3. CLASS B — present upstream (control group, *not* delta)

`Bundle`, `VILWPacketizerList`-family `VLIWPacketizerList`, `HazardRecognizer`,
`ScoreboardHazardRecognizer`, `MachineScheduler`, `MachineTraceMetrics`, `SUnit`,
`ScheduleDAGInstrs`, `TargetInstrInfo`, `TargetSubtargetInfo`, `RegisterPass`,
`Generic::ScalarRC`, `Generic::VectorRC`, `Generic::Unknown Register Class`,
the `mips/hexagon/…` feature groups, `PostRASchedulerList` (LLVM `CodeGen/`).
The vendor DLL also contains **whole upstream targets** (strings for ARM, AArch64,
Hexagon, MIPS, RISCV, PPC, AMDGPU, Sparc `-sparc-delay-slot-filler`, x86, wasm,
m68k, ARC, AVR, BPF, CSKY, Lanai, MSP430, NVPTX, VE, WebAssembly, XCore) — i.e.
the 82,549 functions are mostly *upstream code*, which is exactly why reading
them blind is wasteful.

**Fork provenance hint:** SHAVE's VLIW concepts (packets, slot optimiser, NOP
compression, FU mutation order, post-RA trace scheduler, anti-dependency
breaking) mirror the **Hexagon** backend, and the vendor ships Hexagon strings
too (`-hexagon-small-data-threshold`, `-enable-hexagon-ieee-rnd-near`,
`Hexagon` feature group). Treat Hexagon as the upstream *ancestor* when a
mechanism looks Hexagon-shaped; but no `shave-*` name exists upstream.

### 3.5 …except the *driver*: upstream clang already knows `shave`

`clang/lib/Driver/ToolChains/Myriad.{h,cpp}` is **upstream LLVM 14** (verified
with `git show HEAD:…`), and it already:

* owns the triple `shave` (`llvm::Triple::shave`, `isShaveCompilation`),
* invokes exactly the three vendor binaries the fork ships —
  `moviCompile` (`tools::SHAVE::Compiler`), `moviAsm`
  (`tools::SHAVE::Assembler`), `sparc-myriad-rtems-ld` (`Myriad::Linker`),
* adds `-DMYRIAD2` and forwards `-mllvm`/`-Xclang`/`-mcpu=` unchanged,
* passes the assembler **`-no6thSlotCompression`, `-cv:<cpu>`, `-noSPrefixing`,
  `-a`** (see §A2c).

So the delta on the driver side is small and additive: the fork adds the
`-shave-*` cc1 options, the `-mshave-*` driver options, the
`__SHAVE_BRANCH_DELAY_SLOTS__` / `__SHAVE_MEMORY_LOAD_LATENCY__` macros, and
(per `extracted/…/moviCompile64.dll` argv handling) the `-D__SOFTFP__`,
`-mno-zero-initialized-in-bss`, `-misched=…` defaults (strings 151905-151960).
Upstream has **none** of the `shave-*` strings — checked
(`git show HEAD:clang/include/clang/Driver/Options.td | grep -i 'shave\|myriad\|6thSlot'`
returns nothing).

The forward-looking consequence: **our tree's clang driver work for SHAVE should
mirror upstream `Myriad.cpp`** (it is the same design), while the LLVM backend is
100 % fork-specific.

---

## 4. Negative results (verified, worth knowing)

* **`shave-disable-slot-optimiser` has no reader.** Its value field
  `0x183c6ddb8` appears only twice in 277 MB of decompiled C — both inside its own
  ctor (`decompiled_all.c:594` block, function `FUN_180004480`). Either the slot
  optimiser is unconditional in this build, or the flag is consumed through a
  copy the decompiler inlined. Do **not** model `-shave-disable-slot-optimiser`
  as a working knob.
* Same "no reader" result for `shave-inline-assembly-warning-threshold` and
  `shave-max-inline-assembly-lines` outside the frontend (`1814618a0`).
* `shave-align-stack`, `shave-average-instruction-size`, `shave-standard-section-prefix`
  have single consumers each; they are formatting/section-naming knobs, not
  scheduling knobs.
* No `__FILE__`/`.cpp` assertion strings exist (release build, `-DNDEBUG`), so
  decompiled functions cannot be named from assert text; naming must come from
  behaviour + the 54 `cl::opt` anchors used here.

---

## 5. Evidence index

* option table + globals: `decompiled_all.c:1-3000` (functions 3…56) — frozen copy
  of the raw hits in `raw/opt_xrefs.json`; C++ pass registration:
  `decompiled_all.c:112305,115286,117181,119285,120513,121979,132047,153712,166545`.
* pass-manager hooks: `decompiled_all.c:80435` (`FUN_180084b40` = pre-RA),
  `80469` (`FUN_180084c00` = post-RA / preemption).
* latency/branch-delay implementation: `decompiled_all.c:213788-214185`
  (`FUN_180138bd0`, `FUN_1801387b0`, `FUN_1801388e0`).
* latency/delay clamp into the machine-function info: `decompiled_all.c:223120-223150`
  (`FUN_180145fd0`), caller `FUN_180123080` at line 199354 (feature-bit → 6/8).
* packet/NOP-compression printer: `decompiled_all.c:96154-96375` (`FUN_180098ca0`).
* alignment/`.lalign`: `decompiled_all.c:95520-95690` (`FUN_180097e00`).
* abort stub / inline-asm banner: `decompiled_all.c:96040-96130` (`FUN_180098410`),
  95669, 241672 (`FUN_18015f870`), 225793 (`FUN_180149a20`).
* instruction-flag builder (43 flag bits): `raw/flagbuilder_1800dbac0.c`
  (function `FUN_1800dbac0`, `decompiled_all.c:148358`).
* post-RA scheduler vtable/ctor: `decompiled_all.c:135743` (`FUN_1800cd9b0`),
  135818 (`FUN_1800cded0`), body `FUN_1800e8e80` at `decompiled_all.c:157698`
  (frozen: `raw/postsched_1800e8e80.c`).
* upstream classification: `raw/upstream_classification.tsv`,
  `raw/upstream_patterns.txt`; upstream driver scaffolding
  `git show HEAD:clang/lib/Driver/ToolChains/Myriad.cpp`.
* assembler side: `re_strings/moviAsm64.ascii.txt` (strings 2419-2431, 2476-2492,
  2553-2642, 2701-2741, 2747-2762, 2846-2900, 2968, 3180-3182, 4709-4717).
* team ledger used for the cross-check: `npunlock-work/FINDINGS.md`
  ("LATEST hardware-verified rules"), `npunlock-work/packets/PACKET_SPEC.md`.

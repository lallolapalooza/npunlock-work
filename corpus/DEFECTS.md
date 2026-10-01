## Top defect classes (ranked by how much *arbitrary user C* they block)

Rank = (breadth of ordinary C that hits it) x (how early it is hit) x (no workaround
that keeps the user inside the npunlock contract: `-O2`, one translation unit,
`void controlled_act(unsigned)`).

| # | class | mechanism / exact message | smallest repro | user-visible blast radius |
|---|---|---|---|---|
| 1 | **any function call** | `fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee GlobalAddress)` / `(callee ExternalSymbol)` / `(callee select)` — `SHAVETargetLowering::LowerCall` | `repros/r16_call_internal.c` (a `noinline static` helper) | every multi-function kernel, all of `lib/mlibm.a` (`tanhf`, `expf`, …), varargs, recursion, indirect calls, and every i64/double operation that LLVM lowers to a libcall (`__udivdi3`, `__divdi3`, `__multi3`, `__floatundisf`, `__adddf3`). All four shipped `examples/*.py` kernels (`a03`, `a04`) and `b08/b09/b11/b13/b15/b22/b23`, `r17/r18/r20/r33/r34/r39/r41/r48/r57` fail here. |
| 2 | **signed relational compares** | `fatal error: error in backend: SHAVE: unsupported condition code 20 in wave 1 (floating-point and signed compares are unsupported)` (18/20/21 seen) | `repros/r62_signed_relational.c` | `if (a < b)` / `for (int i = 0; i < n; ++i)`. This is a **regression**: the 12:53 build compiled `a02`/`b06`/`r23` fine, the 13:00 build rejects them. It also breaks npunlock's own `include/npunlock/npu3720_kernel.h` (`__fixsfdi` contains `exponent < 0`), i.e. the documented include path (`a02`) no longer compiles. Only signed `==`/`!=` survive. |
| 3 | **int ↔ float conversions** | `Cannot select: t50: f32 = uint_to_fp t49` / `fp_to_uint` / `fp_to_sint` / `sint_to_fp` | `repros/r06_uint_to_float.c` … `r09_int_to_float.c` | `(float)u`, `(unsigned)f`, `(int)f`, `(float)i` — i.e. essentially every kernel that mixes an integer accumulator/index with fp data, and every `__fp16` ↔ int kernel. `b03`, `b10`, `b12`, `r46`, `r55` (the call behind them is a second failure). |
| 4 | **division / modulo lowering** | `Cannot select: … = mulhu t…, Constant:i32<-1431655765>` (unsigned `/`,`%` by a constant) and `Cannot select: … = sdivrem t…, t…` (signed `/`,`%`) | `repros/r01_udiv_by_const.c`, `r04_sdiv_i32.c` | `x / 3u` is the canonical constant-division optimization, `a % b` is everywhere. Note the asymmetry: `in[0] / in[1]` (runtime divisor) **works** via `SAU.DIV.U32` (`r02`), only the constant-folded form and the signed forms fail. |
| 5 | **global addresses / data sections** | `Cannot select: t50: i32 = GlobalAddress<i32* @g_counter> 0` (also `[8 x i32]* @table`, `@switch.table…`), `Cannot select: t4: ch = br_jt …` | `repros/r12_global_var.c`, `r13_rodata_table.c`, `r10_jumptable_switch.c` | any `static`/global data, any `static const` lookup table indexed at runtime, any string/table the optimiser keeps in `.rodata`, and any switch big enough to become a jump table (`b07`, `b18`, `r36`). `r54` (string literal) passes only because its indexing was fully unrolled. |
| 6 | **stack frames** | `error: SHAVE: unsupported IR node in wave 1: t39: i32 = FrameIndex<0>` → `fatal error: error in backend: SHAVE backend wave 1: unsupported IR` | `repros/r14_alloca_array.c`, `r30_struct_copy.c` | local arrays (`unsigned scratch[8]`) with a runtime index, 8-word struct copies that lower through memory, byte loops that LLVM turns into stack traffic (`r47`), any alloca (`b17`). No stack pointer is modelled, so the backend cannot spill either (see #7). |
| 7 | **register spills crash the compiler** | `SHAVE: stack spills are not supported yet …` then `UNREACHABLE executed at …/SHAVEInstrInfo.cpp:41!` → **SIGABRT / core dump** | `repros/r61_regspill_crash.c` (34 live values OK, 35 → abort) | hard crash instead of a diagnostic; any function with more live values than the 31-register file. This is the only process-level crash class found; `r60` (200 stores) hits it too. |
| 8 | **floating-point compares and float selects** | `error: SHAVE: unsupported fcmp in wave 1: t84: i32 = setcc t49, t52, setolt:ch` (all predicates, including `==`) and `fatal error: … SHAVE: only i32 value-selects are supported in wave 1` | `repros/r64_fcmp_only.c`, `r26_f32_minmax.c` | fp `min`/`max`/`relu`/`clamp`, `if (x > 0)`, sort/select code — the core of most fp kernels. `b16`, `r15`, `r26`. |
| 9 | **integer intrinsics have no ISel pattern** | `Cannot select: t65: i32 = ctlz t50` (`cttz`, `ctpop`, `bswap`, and `fabs` for `fabsf`) | `repros/r49_ctlz.c` … `r53_fabs.c`, `r45_f32_ops.c` | `__builtin_clz/ctz/popcount/bswap`, `fabsf` (clang lowers `fabsf` to `llvm.fabs`, so even a plain `fabsf` call-out is dead unless it links to libm — which is blocked by #1). |
| 10 | **bogus linker message for a missing entry symbol** | `ld.lld: error: output file too large: 18446744073692774968 bytes` (size is `2^64 - 152`) followed by an empty section list | `repros/r42_undefined_entry.c`, `r43_empty_tu.c` | a user who renames the entry or compiles a TU with no code gets a nonsensical size error instead of `undefined symbol: controlled_act`. Not a compile blocker, but it misleads exactly when the user is confused. |

### What already works (so fixes can be scoped precisely)

Full compile→assemble→link→validate pass for: unsigned compares of every kind,
`for`/`while`/`do-while`/nested loops with `break`/`continue`, 12-arm if/else chains,
`goto`/labels, ternaries, `&&`/`||`/`!`, 3-case switches, variable shift amounts,
all shift flavours, 8/16/32-bit signed and unsigned loads/stores, volatile accesses,
`__fp16` arithmetic and fp16↔fp32 round trips, fp32 add/sub/mul/div/neg and fp32
literals (including a `static const float k[3]` that is unrolled), bitfields, unions,
structs by value and struct returns, `_Bool`/`enum`, pointer arithmetic with negative
offsets, string literals with unrolled indexing, 64-bit add/sub/shift/compare,
int32↔int64 extension, a 200-statement straight-line function and a 16-live-value
function (both below the spill threshold).

Encoder/linker status: across **89 compiled kernels** the assembler emitted/encoded
22 distinct mnemonics (`BRU.NOP`, `BRU.BRA`, `BRU.JMP`, `CMU.CMZ.I32`, `CMU.CP.32`,
`IAU.ADD/XOR/AND/OR/SUB/SHL/SHR.U32/SHR.I32/MUL`, `LSU0.LDIL/LDIH`, `LSU1.LDIH`,
`LSU0.LDO.32/LD.8/LD.16/LD.32.F16.F32`, `LSU0.ST.32/ST.16/ST.8`, `LSU1.ST.16.F32.F16`,
`SAU.DIV.U32/DIV.F32/ADD.F32`, `VAU.MUL.F32`) with **zero encoder rejections**, and
`ld.lld` produced a structurally valid ELF for every object it was given. The
toolchain's bottleneck is entirely the *backend*, not the assembler or linker.

### Notes for "arbitrary user C"

* The most damaging pattern is that failures are *hard stops at `-O2`*: the compiled
  surface is not "arbitrary C minus a few APIs", it is "C that happens to avoid
  calls, signed `<`, int↔float casts, `/`/`%` by constants, globals, locals with
  runtime indices, fp compares, and >34 live values". Ordinary kernels (gelu,
  softmax, conv, layernorm) hit at least 3–4 of these in the first screen of code.
* Zero assembler/linker/structural failures means the remaining work is *frontend of
  the backend* (ISel patterns + call lowering + frame/global addressing), not the
  encoding work that the ISA stream owns.
* Structural pass ≠ correct execution: FINDINGS.md R2–R5 (load latency 6, branch
  delay 6, `CMU` flag semantics, LSU0/LSU1 conflict) mean a structurally valid ELF
  can still write nothing to the output window. This corpus measures *buildability*;
  the two axes must both be green before "arbitrary C" is real.
* The gated acceptance fixture `a01` still builds (384 B `.text` on revision A,
  432 B on revision C) — the corpus does not regress the gate, it quantifies the
  distance beyond it.

### The three highest-impact fixes

1. **Implement `LowerCall` (and the callee-save/return-address convention).** One
   function unblocks the largest single group: internal helpers, all of `mlibm.a`,
   varargs, recursion, indirect calls, and every i64/double libcall. Without it the
   repo's own `examples/*.py` cannot work at all.
2. **Fill the arithmetic/compare ISel surface: int↔float conversions, signed
   relational compares, `mulhu`/`sdivrem`, `fcmp` + FP value-selects.** This is the
   set that ordinary C reaches within its first few lines, and it is cheap
   relative to #1 (single DAG patterns/legalizations, no ABI design). Class 2 is
   additionally a *regression* relative to the 12:53 build.
3. **Give the target a memory model: global/data address materialization (also fixes
   jump tables) and stack frames (spill slots) — and make the no-spill case
   impossible to reach.** Fixing only the graceful-abort path stops the compiler
   from core-dumping (class 7), but local arrays/globals/switches (classes 5, 6) need
   real addressing; a jump-table `br_jt` also needs a PC-relative or absolute data
   base that the vendor emitting stream actually supports.

## Execution check on the gate fixture (the orthogonal axis)

The task's optional NPU step was run for the one kernel whose carrier shape matches
(the 1x16 fp16 add fixture). Raw evidence: `logs/smoke_a01_revC.txt`,
`logs/smoke_gold.txt`, `logs/dump_a01_revC.txt`, `logs/dump_gold.txt`.

| image | smoke exit | stages | oracle | dump_probe |
|---|---|---|---|---|
| gold `tests/fixtures/npu3720/add1-fp16.elf` (control) | 0 | all passed | exact | `ACTUAL == EXPECT` (`00 c3 bc bd e0 b5 68 32 …`) |
| our `a01` built by revision A/C | 1 | loader…execution passed, **oracle failed** | mismatch | `ACTUAL 00 01 00 00 ff ff ff 00 00 00 80 3f 00 00 00 bf 20 01 00 00 …` |

The control passing in the same session proves the harness and dump path are live.
Our kernel *does run* (every stage up to `execution` passes, `level_zero_result = 0`)
but writes guard/parameter constants (`0x3f800000` = 1.0f, `0xbf000000` = -0.5f) into
the output window instead of the sums, which is exactly the FINDINGS.md R4/R5
signature (branch on a `CMU` flag the compare never wrote; LSU0 load/store conflict).

**So the two axes are independent and currently green/red in opposite places:**
`a01` is structurally perfect ("arbitrary C" buildability would be satisfied) and
still semantically wrong, while `b01`-`b26` are structurally innocent failures of
code generation. Fixing the buildability defects listed above is necessary but *not
sufficient* for the project's promise; the emitted-stream semantics (R2-R5) remain a
separate, still-open blocker on the same fixture.

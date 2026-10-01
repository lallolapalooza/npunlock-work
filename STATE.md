# STATE SNAPSHOT — npunlock SHAVE toolchain reimplementation (pre-compaction)
Written by the main agent at the point of user-requested compaction. Read this FIRST after any restart, then `FINDINGS.md` (the running ledger) and the per-workstream dirs.

## 1. Task
Create a new folder, clone https://github.com/hsfzxjy/npunlock, and replace the README instruction to download proprietary Intel/Movidius **MoviTools** with the reverse-engineered, native toolchain that lives in `/home/asdf/projects/r2gvp04w-re`; reimplement that toolchain so npunlock compiles custom C for the SHAVE cores without MoviTools and without wine. Acceptance for the toolchain: the repo's `movitools_add1`-style gate — compile `tests/fixtures/npu3720/add1-fp16.c` and get an ELF that runs on the real NPU with `ACTUAL == EXPECT`.

## 2. Workspace map
| Path | What |
|---|---|
| `/home/asdf/projects/npunlock-work/npunlock` | the CLONE/DELIVERABLE (README + wiki + ABI + C/Python sources; offline suite 15/15 green in `build/fresh`) |
| `/home/asdf/projects/npunlock-work/FINDINGS.md` | running ledger: rules R1–R16, round-2 rules, corrections/retractions, status |
| `/home/asdf/projects/npunlock-work/STATE.md` | this snapshot |
| `/home/asdf/projects/npunlock-work/shave-isa/` | native assembler + encoder: `shave_asm.py` (sha256 91283e10…, gates 185/185 via `gates.py`), `encodings.json` (sha256 5660b393…), `gates.py` (sha256 c1101f02…), `cal_fix.py`, `VALIDATION.md`, `ENCODING_NOTES.md`, `ANNOTATED_RULES.md`, `PACKETS.md` |
| `/home/asdf/projects/npunlock-work/shave-toolchain-root/` | packaged toolchain: `bin/{moviCompile,moviAsm,moviLLD}` (POSIX wrappers) + `lib/mlibm.a` |
| `/home/asdf/projects/llvm-14/` | LLVM 14.0.6 fork with our SHAVE target (`llvm/lib/Target/SHAVE`, incl. new `SHAVEFillDelaySlots.cpp`); `BYTE_EXACT_LOG.md`; `tmpdump/` (probe harness, ~300 kernels, `dump_probe`, `mkapp.py`, `mkpatch.py`, `runp3.sh`, `runp4.sh`) |
| `/home/asdf/projects/npunlock-work/lld-sparc/` | lld elf32/EM_SPARC patch + `RELOC_NOTES.md` + `LAYOUT_SPEC.md` |
| `/home/asdf/projects/npunlock-work/{packets,delta,rptrace,corpus,mathlink,loopdiff}/` | vendor RE: packet/filler model; vendor-vs-upstream LLVM delta + rules R1–R21; vendor semantics Q1–Q6; arbitrary-C corpus report; mlibm link verification; op-by-op diff + root causes |
| `/home/asdf/projects/npunlock-work/{E2E_REPORT.md,NPU_REPORT.md}` / `npunlock/BUILD_REPORT.md` | E2E, NPU harness, build/test reports |

## 3. What is DELIVERED (verified)
1. Clone + README/wiki/ABI replacement text (README L113-125 → native toolchain + r2gvp04w-re provenance). Offline suite **15/15** (`ctest --test-dir build/fresh`).
2. Native toolchain E2E: `add1-fp16.c → clang(SHAVE) → .s → shave_asm obj → ld.lld` → **structure-valid** ELF32/EM_SPARC, entry 0x1d000000, empty `.arg.data`, passing `npunlock_parse_shave_elf` (`E2E_REPORT.md`).
3. Assembler: gold `.text` round-trip **192/192 byte-identical**; `gates.py` **176/176**; obj==asm gate; compressed-NOP fillers; branch descriptors; LSU0-loads/LSU1-stores; latency rules; zero-offset rewrite.
4. NPU harness: `npunlock-patchblob` + `tools/npu_add1_smoke.sh` + `NPU_REPORT.md`; **gold's ELF executes 7/7 stages with exact oracle**.
5. mlibm.a linking: all reloc types applied exactly (200/202/209/210/208), 169/204 symbols resolve, valid ELFs (`mathlink/MATHLINK_REPORT.md`).
6. Vendor RE: `delta/` (54 `-shave-*` options, 11 passes, 36 SHAVEISD nodes, 41 CPUs, rules R1–R21 with annotated functions), `packets/` (filler table, branch marker+fill, gold byte map, pipe recipe), `rptrace/` (Q1–Q6), `loopdiff/DIFF.md` (root causes + arithmetic proofs).

## 4. Hardware model (all measured on the real Meteor Lake NPU; evidence in FINDINGS.md)
- **Load latency 6 slots**; SAU.DIV.U32 needs 13; LDIL/LDIH pair needs ≥6 non-LSU slots before its next consumer *and* before the next LSU op.
- **A same-license LSU op inside a load's visibility window discards the load's write-back** (LDIL/LDIH included) — the cause of the "count=0" era; LD.32.F16.F32 needs a slightly larger window.
- **Branch descriptor selects the SENSE, not the direction**: `1b 40` = branch iff flag == 0; `2b 40` = iff flag != 0; with the flips, fill count nibble = 6−k real delay slots (`1b 50` = immediate). A bare 4-byte conditional branch does not transfer.
- **Flag protocol**: `CMU.CMZ(X)` / `CMU.CMII(A,B)` set a FLAG named by their operand register; `BRU.BRA <that reg>` consumes it; the flag is cleared if that GPR is rewritten in between. No ordered compares exist (division/sign-flip formulations are used). Never materialise 0/1 (XOR).
- **Compressed NOP**: one byte = `(n−1)<<4`, n=1..16 slots; `BRU.NOP` (0x07) = 1 slot; vendor default is compressed.
- **LDI forms (4-byte)**: `LSU0.LDIL = 82|(r<<3|2)|lo16`, `LSU0.LDIH = 82|(r<<3|3)|hi16`, `LSU1.LDIH = a9|(r<<3|3)|hi16`, `LSU1.LDIL = 0x89-class`. Three-byte forms are invalid.
- **LSU traps are (register × immediate × form)-dependent** (r19/r21/r24/r30 common); keep LSU destinations in r0..r15.
- Runtime values: params+0x0 = 0x2E014340 (in ptr), +0x4 = 0, +0x8 = 1, +0xc = 4 (rank), +0x10 = 0x80000050 (dims ptr; dims = [8,1,1,1] at params+0x50), +0x28 = out ptr (0x2E014000 inv1 / 0x2E014010 inv2). carrier-init = `00 01 00 00 ff ff ff 00 00 00 80 3f 00 00 00 bf 20 01 00 00 …`.
- Evidence discipline: shell-invoked runs 2-3×, only `dump_probe` ACTUAL/EXPECT + smoke exit code, subtract carrier-init, distinctive witness values, prefer layout-preserving swaps, use `packets/tools/runp3raw.sh` (not the auto-legalizing `runp3.sh`) for filler work.

## 5. Backend state (llvm-14)
`SHAVEFillDelaySlots.cpp` (addPreEmitPass2): LDI-pair spacing to its consumer and to the next LSU op, `1b 40` descriptor on every conditional branch, 6 branch fillers, load write-back window (LSU ops excluded; +1 slot for LD.32.F16.F32), SAU.DIV 13-slot padding — all cl::opt-tunable. `SHAVEISelLowering`: SETCC32 flag protocol (unsigned + signed sign-flip + division formulations), `getSetCCInverse` restored in LowerBR_CC (flag = NOT(cond)). `SHAVEInstrInfo.td`: NOP, BR_DELAY/BR_DELAY_BWD, 4-byte LSU0.LDIL, f16 store on LSU1. `SHAVEAsmPrinter`: bundle-aware. Signed-compare regression FIXED.

## 6. FINAL STATUS — ACCEPTANCE GATE PASSED (supersedes the pre-pass text below)
`npunlock-work/npunlock && ./tools/npu_add1_smoke.sh --elf ../our-add1-fp16.elf` → **exit 0**, 7/7 stages, oracle exact 32/32; ELF `npunlock-work/our-add1-fp16.elf` (1124 B, sha256 bb25f4b5…, reproducible). Offline ctest 15/15; assembler gates 181/181; gold round-trip 192/192; gated byte-compare RED (accepted). Closed in the final phase: branch descriptor sense, load write-back window, `SAU.ADD.F32` constant-selector operand (`i1`=+1.0 … `i8`=+10.0), `SAU.ADD.F32` ≥2-slot result latency. See FINDINGS.md "FINAL" section. Remaining work = the arbitrary-C corpus list (calls first).

### (historical) the last mile
Compiled `add1-fp16.c` now: entry guards take the correct sense, dims loop runs, **count = 8 in both invocations**, and the **first fp16 store lands a real value** (`80 c4` / `11 40`) — previously zero writes. **Remaining: the full kernel faults (0x70000001) inside the fp-loop body.** Two isolated suspects (`loopdiff/DIFF.md` §6):
1. `SAU.ADD.F32` mis-reads its **b operand** (2.0+3.0 → 0x402D413D = 1/√2); gold's (i7,i7,i1) encodes byte-identically as `3CE09C30`, so the b-field placement for other registers needs auditing.
2. The fp16-converting load's write-back window interacts with the LDI/LDIH pair at slots 1–6 (k5 stored 0x0000).
Also open: AsmSchedImpl's new load-window legalizer has an `UnboundLocalError: load_ctx` crash (repro `loopdiff/ours2_4.s`).

## 7. Next actions (in order)
1. Fix the SAU.ADD.F32 b-operand field/`; fmt=` choice (cross-check gold anchor `3ce09c30` and corpus anchors; agent AsmEncodingRE is the encoder authority).
2. Fix the LDI-pair/window interaction for `LSU0.LD.32.F16.F32` (hoist the pair above the load; ≥7-8 non-LSU slots).
3. Re-run: `cd npunlock-work/npunlock && ./tools/npu_add1_smoke.sh --elf <elf>` → must exit 0 with ACTUAL == EXPECT (`00 c3 bc bd e0 b5 68 32 22 3a 55 3c 9a 3d de 3f 11 42 1a 44 2b 45 9e 48 53 4c 2c 50 17 54 0c 58`).
4. DONE (2026-09-28): gates recorded (structure PASSED, offline 15/15, gated byte-compare RED accepted), `BYTE_EXACT_LOG.md` §25 written, reports consolidated, NPU gate exit 0 with oracle exact 32/32.
5. IN PROGRESS — arbitrary C (the remaining part of the goal "compile arbitrary custom C kernels"). Baseline `corpus/results_kernels-c.tsv`: **13/33 kernels OK, 20 FAIL**. Harness `corpus/run_corpus.sh` (`--only NAME[,NAME...]`); analysis `corpus/REPORT.md` + `corpus/DEFECTS.md`. Owner: agent **CorpusWave1** (sole writer of `llvm-14`; nobody else may edit the backend while it runs). Priority: (1) calls/`LowerCall` (fatal today; blocks libm + multi-function kernels), (2) int↔float (`uint_to_fp`/`fp_to_uint`/`fp_to_sint`/`sint_to_fp`; b03/b10/b12, r06-r09, r46), (3) int div/mod (`mulhu` div-by-constant r01/r03/r56, `sdivrem` r04/r05, i64 mul r19), (4) `GlobalAddress` + `br_jt` (b07/b18, r10/r12/r13/r36), (5) `ctlz`/`cttz`/`ctpop`/`bswap`/`fabs` (r44, r49-r53), (6) fp compares ("unsupported condition code 20 in wave 1", a02), (7) frame indices/spills (r60 SIGABRT, `UNREACHABLE` at `SHAVEInstrInfo.cpp:41`). Invariant: the add1 NPU gate (exit 0, oracle exact) and offline ctest 15/15 stay green after every item.

## 8. Verification commands
```
cd /home/asdf/projects/npunlock-work/npunlock
ctest --test-dir build/fresh                     # 15/15 offline
./tools/npu_add1_smoke.sh --elf <elf>            # NPU gate: exit 0 + oracle exact
/home/asdf/projects/llvm-14/tmpdump/dump_probe --bundle build/fresh/npu-smoke/bundle
python3 /home/asdf/projects/npunlock-work/shave-isa/gates.py     # assembler gates (183/183)
# compile path:
/home/asdf/projects/llvm-14/build/bin/clang -cc1 -triple shave -target-cpu 3720xx -S -O2 -x c <src> -ffunction-sections -fdata-sections -o - \
 | python3 /home/asdf/projects/npunlock-work/shave-isa/shave_asm.py obj - \
 | /home/asdf/projects/llvm-14/build/bin/ld.lld -flavor gnu -EL -e controlled_act -z max-page-size=0x10 --gc-sections -T npunlock/src/shavecc/shave_kernel.ld - -o out.elf
```

## 9. Agent roster (all task-scoped; dirs above)
AsmEncodingRE (encoder authority, answer-only), AsmSchedImpl (assembler owner), MedicalCrayfish (backend), StrangeSole (loopdiff diagnosis), VendorRE2 + BiologicalMockingbird (vendor RE), InternationalUrial (delta), CorpusProbe (arbitrary-C corpus), MathLink (archive linking), RoughFinch (measurement harness), ResultingBear (in flight on the fp-loop fault).
Cross-cutting: no wine anywhere (deliverable must not require it); no network for the toolchain; NPU measurements via the harness only.

## 10. Phase 6 — gemma-4-E4B via llama.cpp + a ggml NPU backend (user-directed)
**User goal (2026-09-28):** "the arbitrary C can be a llama.cpp backend example" + run `https://huggingface.co/ggml-org/gemma-4-E4B-it-GGUF`; also "base it off the cpu and cuda backends" — i.e. add a ggml backend (like ggml-cpu/ggml-cuda) that offloads ops to the Meteor Lake NPU through npunlock + our reimplemented SHAVE toolchain. Model arch `gemma4`, ctx 131072; quants BF16 15.05 GB / Q8_0 8.03 GB / **Q4_0 4.59 GB** (chosen).

**Resource note:** the host hit 678 MB free with swap fully used (user's 7.91 GB qemu VM); the user freed ~8 GB → `available` ~10.7 GB. Builds are allowed again; llama.cpp build runs at `-j6` (22 cores). Rule learned: check `free -m` before any ninja/clang build; never exceed `-j2` when `available` < 2 GB.

**Artifacts**
- `npunlock-work/llama.cpp` — llama.cpp clone (ggml-org), built with `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` + `ninja -C build -j6 llama-cli llama-bench` (log `/tmp/llama_build.log`).
- `npunlock-work/models/gemma-4-E4B-it-Q4_0.gguf` — model download (curl from HF resolve URL, single writer; 4.59 GB).
- `npunlock-work/corpus/llama/{ops.cpp,vec.cpp,vec.h,LICENSE}` — canonical ggml sources (MIT © ggml authors) used to author the real-kernel corpus targets.
- New backend target: `llama.cpp/ggml/src/ggml-shave/` (owner agent **GGMLBackend**) — ggml device + host-visible buffer type + `supports_op` + `graph_compute`; per supported op: compile with `shave-toolchain-root`, `npunlock-patchblob` → `porting_probe.py create` → `npunlock-linux-probe`, then verify. Milestone 1 = ADD/MUL/SCALE bit-exact vs the CPU backend, then run gemma-4-E4B and report what offloads (unsupported ops stay on CPU via `ggml_backend_sched`).
- Corpus targets from ggml: `corpus/kernels/l01_silu_f32.c` (x/(1+expf(-x))), `l02_rms_norm_f32` (1/sqrtf(mean+eps)), `l03_softmax_f32` (expf), `l04_vec_dot_f32`, `l05_rope_f32` (sinf/cosf), `l06_scale_f32` (owner agent **LlamaKernels**) — these are the acceptance evidence for the CALLS gap (`mlibm`).

**Owners / no-conflict split:** `llvm-14` = **CorpusWave1** (sole writer; arbitrary-C gaps) · `llama.cpp/` = **GGMLBackend** · `corpus/kernels/l*` = **LlamaKernels** · read-only plans = GapCalls / GapCasts / GapDivBuiltins / GapGlobalsFrames.

**Corpus state:** re-measured 15/33 OK on the current tree (was 13/33); remaining families unchanged (calls → int↔float → div/mod → globals/br_jt → builtins → fp compares → frames/spills). Vendor call-form evidence found by CorpusWave1 in `mlibm.a`'s `math_lite.o` (`tanhf` calls `expf`): address materialised into i30 as LDIH(hi16, reloc 202) + LDIL(lo16, reloc 200); the transfer-and-link opcode is still being pinned from the 375 prebuilt vendor ELFs in `/tmp/prebuilt3720`.

### 10.1 Phase-6 progress at checkpoint (2026-09-28 ~14:10)
- **llama.cpp build**: DONE — `llama.cpp/build/bin/llama-cli` (1.47 MB) and `llama-bench` linked (RC=0). Note: the harness kills background bash at 300 s, so the build needed one `ninja` resume; log `/tmp/llama_build.log`.
- **Model download**: DONE — `models/gemma-4-E4B-it-Q4_0.gguf` = 4.59 GB (4590807392 bytes). Run: `cd llama.cpp && ./build/bin/llama-cli -m ../models/gemma-4-E4B-it-Q4_0.gguf -p "..." -n 48 -c 2048 -t 8 --no-warmup`.
- **ggml kernel corpus** (`corpus/results_llamakernels.tsv`, tag `llamakernels`): l04_vec_dot_f32 OK (496 B), l06_scale_f32 OK (480 B); l01_silu_f32 FAIL = calls gap (`llvm.exp.f32` -> mlibm `expf`); l02_rms_norm_f32 FAIL = `uint_to_fp` then `fsqrt`; l03_softmax_f32 FAIL = "only i32 value-selects are supported" (f32 select); l05_rope_f32 FAIL = `fpow`. These four are the demand-driven acceptance targets for the arbitrary-C work (real model code, not synthetic).
- **In-flight owners at checkpoint**: CorpusWave1 (sole `llvm-14` writer; 15/33 corpus OK, pinning the vendor call/return form from `mlibm.a` `math_lite.o` + 375 prebuilt ELFs in `/tmp/prebuilt3720`) · GGMLBackend (new `llama.cpp/ggml/src/ggml-shave/`) · GapCalls / GapCasts / GapDivBuiltins / GapGlobalsFrames (read-only plans).
- **Resume checklist**: 1) finish llama.cpp build + model download; 2) run `llama-cli -m models/gemma-4-E4B-it-Q4_0.gguf -p "..." -n 32` (CPU) to confirm the model loads and generates; 3) check GGMLBackend's milestone-1 result (ADD/MUL bit-exact vs CPU, then offload report); 4) apply the gap plans as CorpusWave1 lands them, keeping add1 gate exit 0 / oracle exact + ctest 15/15.

### 10.2 Model run — WORKS (2026-09-28)
`cd llama.cpp && ./build/bin/llama-cli -m ../models/gemma-4-E4B-it-Q4_0.gguf -p "<q>" -n N -c 2048 -t 8 --no-warmup` loads gemma-4-E4B-it-Q4_0 (ftype Q4_0, modalities text) and generates coherent text: measured **Prompt 21.6 t/s, Generation 8.3 t/s** (CPU, 8 threads). It is a chat/`any-to-any` build, so after the completion it enters the interactive `>` loop — bound it with `-n` and/or a `timeout` (the harness kills background bash at 300 s; use `timeout 300 … < /dev/null`).
**Billing incident:** all six working agents were killed mid-flight by a 402 "Insufficient account funds"; the user restored funding and all six were revived as `CorpusWave1-2`, `GGMLBackend-2`, `GapCalls-2`, `GapCasts-2`, `GapDivBuiltins-2`, `GapGlobalsFrames-2`, re-primed with the recovered evidence (below) so nothing was re-derived.
**Recovered from the pre-kill transcript (now also in FINDINGS.md):** the CALL mechanism is `BRU.SWP i30` (`47 f2 01`) with the callee address materialised hi-first by `LSU1.LDIH`+`LSU0.LDIL` into i30, return via `BRU.JMP i30` (`47 f1 01`), return value in **i18** — hardware-verified by CorpusWave1 (incl. mlibm `floorf`/`expf`).

### 10.3 SCOPE DECISION (user, 2026-09-28): CUDA-style partial backend
Arbitrary-C/NPU support is **limited like a CUDA ggml backend** — a subset of ops that offloads, everything else transparently falls back to CPU via `ggml_backend_sched`. We do not know whether SHAVE is Turing-complete, so completeness is explicitly NOT a goal. Consequence: the corpus gap list is a *menu ranked by offload value*, not a checklist.
Priority: calls (`expf`) -> `GGML_OP_SILU` > f32 select (`SELECT_IRF32`) -> `GGML_OP_SOFT_MAX` > int<->float (`fsqrt` is a libm call, free with calls) -> `GGML_OP_RMS_NORM` > cheap bit builtins + `GlobalAddress` (no libm dep) . **Deprioritised/skippable:** jump tables, varargs (b13), function pointers (b20), recursion (b15), i64 div/mod, frame/spill completeness beyond supported kernels, `fpow` beyond rope.
Backend contract: capability-gated `supports_op` (false on anything uncertain — a false positive yields wrong numbers, a false negative only costs a CPU fallback); Tier-1 ADD/MUL/SCALE must be bit-exact vs the CPU backend on a standalone ggml graph before the model path is touched; CPU-only output must not regress with the backend registered; coverage reported honestly (which ops offload, which fall back).

### 10.4 MILESTONE — calls land; libm executes on the NPU (2026-09-28)
`LowerCall`/`LowerReturn`/`LowerFormalArguments` implemented with the measured ABI (args i18..i11 descending, ret i18, SP i19, link i30). Emitted sequence: `IAU.SUB i19,i19,8 ; LSU1.STO.32 i30,i19,4 ; LDIL i30,sym ; LDIH i30,sym ; BRU.SWP i30 ; 6 fillers ; LDO.32 i30,i19,4 ; IAU.ADD i19,i19,8`.
- **Corpus 15/33 -> 20/33, zero regressions.** New PASS: `l01_silu_f32` (real ggml silu, `expf` on the NPU), `a03_gelu_fp16`, `a04_gelu_fp32` (`tanhf`), `l04`, `l06`. Verified by me: `l01_silu_f32` PASS end-to-end; add1 gate exit 0 + oracle exact (ELF now 1172 B).
- Still failing: `l02_rms_norm_f32` (int↔float; `fsqrt` is a libm call and is therefore already available), `l03_softmax_f32` (f32 value-select), `l05_rope_f32` (fpow).
- New hardware rules measured: the callee-address **LDIL/LDIH pair must be ≥6 issue slots before `BRU.SWP`** (adjacent = stale i30 = the call silently does not happen); **LDIL zero-extends, LDIH inserts** (emit LDIL first); a single LDIL into **i21 faults** (i20/i22/i23/i25-i29 fine).
- New owner `CorpusWave2` (sole `llvm-14` writer) has the remaining items, with two correctness defects queued first: `getCallPreservedMask` must be the measured **`0xBFF7F800`** (i11..i18, i20..i29, i31; only i0..i10 caller-saved) and `SHAVERegisterInfo.td` lacks **i31** with an unsafe 2-byte `GPRF16` spill size.
- `GGMLBackend-2` was told **SILU and GELU can now be claimed**; RMS_NORM/SOFT_MAX/ROPE stay unclaimed until their toolchain items land.

### 10.5 Parallel wave (6 agents, disjoint ownership)
| Agent | Owns | Task |
|---|---|---|
| `CorpusWave2` | `llvm-14/**` (sole writer) | fix `getCallPreservedMask` -> `0xBFF7F800` + missing i31/GPRF16 spill size; then f32-select -> int↔float -> div/builtins -> GlobalAddress/BRIND |
| `GGMLBackend-2` | `llama.cpp/**` | `ggml/src/ggml-shave/` device+buffer+`graph_compute`; Tier-1 ADD/MUL/SCALE bit-exact; SILU/GELU claimable now; RMS_NORM/SOFT_MAX/ROPE gated |
| `MultiOpProbe` | `npunlock-work/multiop/` | can one invocation run MULTIPLE patched kernels? per-invocation host cost? carrier families + op contracts |
| `NPURuntime` | `npunlock-work/npu-runtime/` | reusable execution layer (`run()`/`warm()`) with measured per-call latency vs the naive CLI flow |
| `KernelLib` | `npunlock-work/kernel-lib/` | precompiled op kernels + `manifest.json` (op/dtype/shape/ELF/sha256/status/latency) + `build.sh` |
| `EvalHarness` | `npunlock-work/eval/` | deterministic greedy prompt set, CPU-only vs NPU-backed comparison, tokens/s + exact-match + offload stats |

NPU device is shared: every agent must use a private work dir (concurrency in a shared scratch path already caused one spurious failure). Architecture constraint reaffirmed for all: npunlock exposes only the ACT-SHAVE path (static shapes, invocation-local memory, no strides/broadcast/aliasing); DPU/tensor-engine ops (matmul/conv) are not programmable from npunlock, so a full CPU/CUDA-backend port is impossible — the deliverable is a capability-gated partial backend.

### 10.6 ggml-shave backend DELIVERED (capability-gated, bit-exact)
`llama.cpp/ggml/src/ggml-shave/` (owner GGMLBackend-2): CMake option `GGML_SHAVE` (OFF by default; with it OFF nothing is configured/built/registered — verified by a fresh configure + `nm -D libggml.so | grep -c shave` = 0), `ggml-shave.cpp` (device, host buffer type, `supports_op` capability table, `graph_compute`, stats), `shave-engine.{h,cpp}` (persistent runner + length-prefixed frame transport, serialized calls), `shave-runner.c` (NPU child, compiles npunlock's `src/linux_probe/infer_engine.c`), `kernels/{shave_act.h,shave_add.c,shave_copy.c}` + `kernels/prebuilt/*.elf`, `tools/build_kernels.py`, `tests/shave-selftest.cpp`, README.
- **Standalone selftest PASS**: f16 ADD x16 offloaded and **bit-exact vs the CPU backend**; f16 ADD x17/x4096 and f32 ADD x16 correctly declined; `ggml_backend_sched` places the supported node on the NPU and falls back bit-exact for 4096; stats line `ggml-shave-stats {"engine":"ready","invocations":2,"offloaded":2,"declined":4,"unsupported":0}`.
- `llama-cli` registers the device (`SHAVE0`) and runs normally, but **0 model ops offload** — measured **~46 ms per NPU invocation** (~0.7k elements/s), dominated by npunlock's per-call graph create/initialize, against a carrier that accepts only a 32-element f16 tile. Every op falls back to the CPU (tokens/s unchanged: prompt 21.6, gen 8.3).
- Diagnostics for consumers: `GGML_SHAVE_STATS=1` prints one parseable line at exit; `GGML_SHAVE_DEBUG=1` logs each offloaded node; `ggml_backend_shave_offload_count()` / `ggml_backend_shave_status()` in-process.
- **Consequence:** session/graph reuse and large-tile carriers are now the critical path (assigned to `NPURuntime` and `MultiOpProbe`), and realistic-tile kernels to `KernelLib`.

### 10.7 Hard architectural limits found (2026-09-28, verified)
1. **Carriers cannot be generated on Linux.** `npunlock/src/ir2blob/` ships only `ir_worker_win32.c`; `ir2blob.c:34` fails on POSIX ("Level Zero worker failed … not_found"). Only three prebuilt carriers exist: `npunlock/tests/fixtures/npu3720/{abs-abs-1x32.blob, abs-add-1x16-tile1.blob, add1-shared-1x16.blob}` — 16/32-element f16 tiles. Backend-measured contract: `abs-abs-1x32` = 1 f16 input 1x32 + 1 output, two chained ACT groups with 16-element tiles, lane i pairs with lane i^16 (patch group0 with the op kernel, group1 with a copy kernel); `abs-add-1x16-tile1` pre-adds a fixed ramp and is unusable for general ops. A POSIX graph compiler (candidate: the `libopenvino_intel_npu_compiler.so` present on this machine) is the **prerequisite for anything model-scale**.
2. **Per-invocation cost ~19-48 ms** (in-engine mean 19.4 ms over two calls; 46 ms/call sustained over 20 calls; 48 ms first call), dominated by npunlock's per-call graph create/initialize. With a 32-element tile that is ~0.7k elements/s.
3. **f32 arithmetic defect (open, routed to CorpusWave2):** `a*b` (VAU.MUL.F32 and SAU.MUL.F32, all fmts/orders/destinations) either leaves the destination unchanged or faults; `SAU.SUB.F32` never produces a-b; f32 ADD is correct only when `; fmt=2` forces the register form — the compiler default (fmt=3) reads the third operand as the **hardware constant selector**. Reported by the backend author with hardware measurements.
4. **Refuted claim:** the backend author reported a "team-wide toolchain regression" (freshly compiled `add1-fp16.c` = 1012 B ELF executing as a copy). **I could not reproduce it**: the standard path compiles the fixture to a **1172 B** ELF and `npu_add1_smoke.sh` passes with the **oracle exact 32/32**. CorpusWave2 has been asked to confirm/refute and to check their `fmt fixup` pipeline and the stale `corpus/toolchain-snapshot*` assemblers.

### 10.8 ggml-shave status (verified)
`ggml-shave-selftest` PASS: NPU f16 ADD bit-exact vs CPU (16/16), capability gate declines f16 x17/x4096 and f32, scheduler fallback bit-exact; `llama-cli` registers `SHAVE0 : Intel Meteor Lake NPU (NPU3720 SHAVE cores via npunlock)`. Model run: 0 ops offloaded (tile + per-call cost), tokens/s unchanged. Stats: `GGML_SHAVE_STATS=1` -> `ggml-shave-stats {"engine":"ready","invocations":N,"offloaded":N,"declined":N,"unsupported":N,"lanes":16,"tile_elements":32,"avg_invocation_us":…}`. Open: f16 MUL/SCALE blocked by (3); SILU/GELU blocked by last-bit differences between vendor mlibm and host libm (so not bit-exact vs the CPU backend) plus the tile limit; NPURuntime/KernelLib integration seams are one-file each.

### 10.9 NPURuntime DELIVERED (measurement + reusable layer)
`npunlock-work/npu-runtime/`: `libnpu_runtime.{a,so}` + `include/npu_runtime.h` + README + copyable `examples/backend_example.c` + 6 diagnostic tools + `make check` (15-check acceptance suite, all PASS). Nothing outside its directory was modified.
**API**: `npurt_warm/is_warm/shutdown`, `npurt_compile_source` (moviCompile→moviAsm→moviLLD via fork/exec, no shell), `npurt_kernel_create` (patch+load+initialize, cached by blob/ELF sha256), `npurt_kernel_create_from_blob`, **`npurt_kernel_create_layered(entries[], n)`** (several ACT groups of ONE carrier patched with different ELFs — the multi-kernel path), `npurt_run(k,&req,&timing,&diag)`, `npurt_kernel_clone` + `npurt_run_batch` (N invocations, one submission, one wait). Errors + 512-byte diagnostics.
**Measured (vs the graph-recreation paths the ggml backend used)**:
| path | per invocation |
|---|---|
| naive CLI flow (patchblob+bundle+probe) | 1045–1353 ms |
| recreate graph per call | 22.7 ms idle / 31–47 ms loaded |
| **reused session, 16 elements** | **0.95–1.00 ms** (999 invocations/s) |
| reused session, 32 elements | 1.21 ms (829/s) |
| reused session, 2048 elements | 1.145–1.165 ms (858–873/s, 1.76–1.79 M elements/s) |
| **batched marginal per execution** | **0.38–0.48 ms**, flat from 16 to 2048 elements (5.1 M elements/s @2048) |
Speedup ≈23–46x vs recreation, ≈1000x vs the CLI flow; batching adds 2.1–2.5x. Per-kernel setup 6.2–25.9 ms (amortised after ~25 invocations). Breakdown of the per-invocation cost: bind <0.01 ms, command-list re-submit 0.01–0.08 ms, fence create/destroy ~0.08 ms, **fence wait (device execution) 0.80 ms @16 el / 1.12 ms @2048 el**.
**Device finding**: destroying the Level Zero context and re-initialising in one process SIGSEGVs on the next submit inside `libze_loader.so.1` (dlclose doesn't help) → loader/device/context are process-lifetime; `shutdown()` releases kernels+queue and `warm()` re-arms.
**Verified**: `make check` PASS (build warning-free; add-one oracle 3/3; compile-from-source example exact; cross-validation vs `dump_probe` ACTUAL on `a05_scale_f32`; multi-kernel handles; batch bit-identical; 2048-element carrier; lifecycle; fence-timeout recovery; 4-thread + 2-process concurrency; reuse regression guard).
**Honest consequence**: the true floor is the **~0.8–1.1 ms device-execution wait per invocation**, so throughput is latency-bound (~1.8 M elements/s single, 5.1 M batched at 2048) — far below a CPU elementwise kernel. NPU offload therefore only pays for compute-heavy-per-element kernels or when overlapped/batched, NOT for cheap elementwise ops. This must be stated plainly in the backend's coverage report.

### 10.10 CARRIER GENERATION ON LINUX — LANDED AND VERIFIED (PosixWorker + MultiOpProbe)
`npunlock/tools/npu_carrier_smoke.sh --elements 4096` (with `NPUNLOCK_SHAVE_TOOLCHAIN_DIR=shave-toolchain-root`) → **exit 0, all stages passed, oracle exact (`fp16_add1_exact`), element_count 4096**; it emits an IR (`abs-1x4096.xml`), the **new carrier blob** (`carrier-1x4096.blob`), a contract/inspect JSON, a bundle and an execution report under `npunlock/build/fresh/npu-carrier/`. So the shipped path can now generate carriers of arbitrary 1xN f16 shape on Linux — the 16/32-element fixture ceiling is gone.
- Measured tile sizes: 8 f16/invocation (abs-add-1x16), 16 (abs-abs-1x32), and **512/1024/2048/4096/8192 f16/invocation** for IR Abs carriers at N=1024/2048/4096/8192/16384 — all patched outputs verified element-exact.
- Carrier menu: ACT-patchable `Abs, Elu, Erf, Sigmoid, Exp, Floor, Log, Sign, Sqrt, Tanh, Maximum, Minimum, Power, Divide`; DPU-only (not patchable) `Add, Clamp, Multiply, Relu, Subtract`; `Ceil`/`Negate` fail to compile.
- Multi-kernel per invocation: 2 load-bearing ranges (abs-add-1x16, IR Abs carriers), **4** on abs-abs-1x32; per-range RELA addend + extent rewrite (12 bytes); **entries must be 0x400 apart** (range address rounds down to 0x400).
- Warm re-execution 0.14–0.5 ms → 77.4M elements/s at N=16384 (149k at N=32); fresh cycle 22.9–55.4 ms.
- Cross-check: `npu_carrier_smoke.sh --elements 4096` oracle exact, carrier contract identical to the out-of-tree one, warm exec 0.232 ms vs 0.241 ms.
- **Ceiling to state honestly**: the per-invocation floor is the ~0.8–1.1 ms device-execution wait (NPURuntime), so single-invocation throughput is ~1.8 M elements/s at 2048 elements (5.1 M batched) — far below a CPU elementwise kernel. Offload pays for compute-heavy-per-element kernels or overlapped/batched work, not for cheap elementwise throughput.

### 10.11 TOOLCHAIN WAVE 2 — corpus 27/33, f32 arithmetic FIXED (CorpusWave2, verified by me)
- **Corpus 20/33 -> 27/33.** Newly PASS: `b01_int_arith`, `b03_casts_fp`, `b10_structs`, `b12_bitfields`, `b16_fcmp`, **`l02_rms_norm_f32`**, **`l03_softmax_f32`** (+ `l01_silu_f32`, `l06_scale_f32` already). I re-verified `l01/l02/l03/l06/b01/b03` all OK, gate exit 0 + oracle exact, ctest 100% (suite now 16 tests).
- **f32 arithmetic defect CONFIRMED, root-caused, FIXED and bit-exact verified on hardware** (f32 `MUL`/`SUB` and the `SAU.ADD.F32` operand form) — this was the blocker for `GGML_OP_MUL`/`SCALE` and any f32 kernel.
- **Call-preserved mask corrected with a measurement that overturns the DWARF reading**: a RegMask bit index is the *Register enum* (TableGen numbers SHAVE::R0==1), so the hardware value 0xBFF7F800 is not directly writable; and the vendor CIE's `DW_CFA_same_value` list is not the callee-saved set (it includes r18, the arg/return register). Measured on silicon by renaming one register in the bit-exact `l01_silu_f32` assembly: **i11 does NOT survive an `expf` call**; i12..i17, i23, i25..i29, i31 do. Applied mask = i12..i16, i19, i20..i29, i31 (word0 0x7FF3E000, word1 0x1 for i31 as bit 32).
- **`def R31` added** (was missing; HWEncoding 31) and **`GPRF16` spill size corrected to 32** (TableGen had inferred 16 from the f16 type, giving 2-byte slots against 32-bit registers).
- **NEW bug fixed**: `LowerFormalArguments` returned `DAG.getRegister($r18)` instead of a `CopyFromReg`, so every use of `layerParams` was a bare physical-register read; after a call it dereferenced `expf()`'s result as the params pointer (kernel wrote nothing). Fixed -> `expf(-6.96148681640625) = 0x3a786e2a` bit-identical to the host oracle.
- The "1012 B copy / team-wide regression" claim was **refuted with evidence** — it was the frozen 13:01 snapshot clang (`SAU.ADD.F32 i2,i2,i4` = +1.41494) versus live (`i2,i2,i1` = +1.0).
- Still failing: `b02_div_mod` (sdivrem), `b07_switch` (br_jt), `b08_calls_internal`, `b09_libm`, `b11_int64`, `b13_varargs`/`b15_recursion` (skip), `b17_alloca`, `b18_globals`, `b22_double`, `b23_i64_float`, `l05_rope_f32` (**NEW: LLVM crash**).

### 10.12 PosixWorker DELIVERED — shipped-path carrier generation on Linux, VERIFIED
- **Diagnosis corrected**: `src/ir2blob/ir_worker_win32.c` contained **no** Windows API (it is transport-neutral framing) — renamed `ir_worker_process.c` and used by both platforms. The real Linux gaps were (a) no POSIX worker executable (CMake built `npunlock_worker` under `if(WIN32)` only → `posix_spawn` ENOENT → "Level Zero worker failed"), and (b) `src/workers/ir_worker.c` was win32-only (`windows.h`, `LoadLibraryExW`, `GetStdHandle`, `--response-handle`).
- **Port**: shared portable core + thin platform layer — new `src/workers/worker_platform.h`, `worker_platform_posix.c` (`dlopen("libze_loader.so.1")` + `dlsym`, fd-based response sink via `--response-fd`, ignoring SIGPIPE) and `worker_platform_win32.c` (Windows behaviour byte-identical). `npunlock_worker` now builds on POSIX and is installed as Runtime.
- Also: `src/linux_probe/main.c` lost its hardcoded 32-byte tensor requirement (now equal/nonzero/even-sized, with INPUT/EXPECT/ACTUAL + first/last mismatch index); `tools/porting_probe.py` gained additive `--element-count N` and `--oracle fp16_add1_exact` (defaults unchanged → the existing gate is byte-identical); `tests/integration/test_ir2blob.c` ported to POSIX (unblocking the gated `npu_ir2blob_abs` test on Linux); docs updated (README, wiki/PORTING milestone 5, DEVELOPMENT, CUSTOM_KERNELS IR layout + kernel-shape traps, LIMITATIONS, PORTING_PROBE_BUNDLE, docs/C_API, fixtures README); new fixtures `add1-wide.c`, `maximum-2input-1x1024.xml`; new tests `porting_ir_worker_posix`, `npu_ir2blob_abs`.
- **Verified by me**: `ctest --test-dir build/fresh` = **16/16** (was 15); `npu_carrier_smoke.sh --elements 16384` → exit 0, **oracle exact**, element_count 16384 (512x the original 32-element fixture); 512/2048/8192 f16 per ACT invocation available at 1x1024/1x4096/1x16384.

### 10.13 Operational: scratch MUST be disk-backed, NOT tmpfs (2026-09-28)
**Verified with `df -T`/`findmnt`:** `/tmp` is `tmpfs` (RAM) 15.2 GB total / 3.9 GB free — it filled to ~80% and caused confusing toolchain/device failures. `/var/tmp/npu-scratch` is `ext4` on `/dev/nvme0n1p5`, **212.5 GB with 49.2 GB free**. Broadcast to all agents (5/5 delivered). Use:
```
export TMPDIR=/var/tmp/npu-scratch            # 50 GB free on the 213 GB volume, mode 1777 (created)
export NPUNLOCK_SMOKE_WORK_DIR=/var/tmp/npu-scratch/<agent-or-task>
```
Fallback: `/home/asdf/projects/npunlock-work/scratch` (same volume). `TMPDIR` is honored by Python's `tempfile`; tools taking an explicit work dir must be pointed at a subdirectory. Clean per-case scratch; never delete `/tmp/opencode` or another agent's directory. Anything to keep must live under `npunlock-work/**`.

### 10.14 MODEL RUNS ON THE NPU — correct, but slow (verified by me)
`./build/bin/llama-cli -m ../models/gemma-4-E4B-it-Q4_0.gguf -dev SHAVE0 -p "The capital of France is" -n 16 --temp 0 -st` →
```
Paris
ggml-shave-stats {"ops":["add","mul","copy"],"invocations":1053,"offloaded":1053,"declined":146485,
 "unsupported":0,"elements_offloaded":8571904,"tile_bytes":131072,
 "lanes":{"add":32768,"mul":32768,"copy_bytes":131072},"per_op_invocations":{"add":632,"mul":421,"copy":0},
 "setup_ms":9.93,"npu_ms":5161.38,"avg_invocation_us":4901.6}
[ Prompt: 6.0 t/s | Generation: 0.4 t/s ]     # CPU-only: prompt 21.6 / gen 8.3 t/s
```
- **Functionally correct**: 1053 NPU invocations (632 add, 421 mul), 8.57 M elements offloaded, output "Paris" identical to CPU-only; `ggml-shave-selftest` PASS with 15 offloads bit-exact vs the CPU-only scheduler; `GGML_SHAVE=OFF` unchanged; `llama-cli -dev SHAVE0` no longer SIGFPEs (the earlier 254 s was the interactive REPL waiting on stdin).
- **~20x too slow**: 4.9 ms/invocation because `shave-engine.cpp` still uses the child-process transport that recreates/initialises per call. `npu-runtime` achieves 0.95-1.2 ms reused-session (0.38-0.48 ms batched). 146,485 nodes declined (only add/mul claimed).
- Owner `GGMLBackend-4` dispatched to (1) adopt `npurt_*` through the `shave-engine.cpp` seam, (2) widen the op set from `kernel-lib/manifest.json` (silu/gelu/rms_norm/soft_max/scale + binary add/mul on two-input carriers), (3) report before/after t/s honestly.
- Limits to respect in claims: SHAVE fp32 **flushes denormals** (operand and result); **fp16 fused store loses ~3% of lanes** in one shape (aliased-store pad bug — top compiler priority); mlibm last-ULP deltas; f32 carriers rejected by npunlock; >1 tile per node declined.

### 10.15 EVALUATION RESULTS — offload is correct but does not pay (EvalHarness-2)
Artifacts: `eval/baseline/RESULTS.md` (470 lines) + `eval/baseline/2026-09-28-gemma4-e4b-q4_0/` (per-mode records, verbatim CLI streams, bench JSONs, engine-cost JSONs); harness `eval/run_compare.sh` (bugs fixed: relative `--out` exported as `TMPDIR` so offload silently disabled; `llama-bench` run without `cwd`; `usage()`; `--score-only` added).
**Correctness**: NPU vs CPU — **6/8 prompts byte-exact** (arith, short, repeat, list3, count10, code); 2/8 diverged (explain, longctx), both single-flipped-token shaped inside the `[Start thinking]` preamble, *consistent with* the fp32 **denormal flush** but not isolated. CPU vs CPU2: **8/8 byte-identical** (determinism confirmed).
**Offload**: NPU run = **71,383 invocations, 100% offloaded**, 195,395,072 elements, 364.9 s device time = **5.11 ms/invocation (0.54 M elements/s)**; 0 unsupported; ~65k declined per prompt (~89k longctx).
**Speed (llama-bench, -p 128 -n 32 -r 2 -t 8)**: CPU pp 28.4 / tg 8.23 t/s · NPU pp 18.9 / **tg 0.458** t/s. CLI means: CPU 18.2/6.6 t/s (23.9 s) vs NPU 8.1/0.46 t/s (81.4 s).
**Cost regimes (all measured)**: fresh graph per call 22.9-55.4 ms · live graph in-process warm 0.143-0.5 ms (149k -> 77.4 M elem/s at N=32..16384) · shipped warm engine 4.85-5.11 ms/invocation (0.54 M elem/s) · **CPU P-core f16 AVX2 vector 2.4-5.4 ×10⁹ elem/s** (scalar 1.6e8; f32 mul 2.6 µs at N=2560).
**VERDICT**: elementwise NPU offload on this machine is **capability/correctness only, NOT a throughput win** — the shipped path is ~5,000-10,000x slower than the CPU elementwise rate, and even the best in-process warm figure is 30-70x slower. Recommendation: keep the backend as an opt-in, honest-coverage offload path; do not claim speedups; the only defensible future case is compute-heavy-per-element kernels with batching and overlap.

### 10.16 FRAMING (user, 2026-09-28) — the NPU is for compute-heavy workloads
"The NPU is for compute heavy workloads then, it makes sense since it was demoed that way with stable diffusion. llama.cpp for us is just a demo to prove the NPU works."
Consequences for how we read the results:
- The elementwise-throughput verdict in §10.15 is **expected and not a defect**: a ~1-5 ms per-invocation NPU path cannot beat a CPU vector op on cheap elementwise work. Do not present it as a failure, and do not chase elementwise speedups.
- The deliverable's purpose is **proof that the NPU works under our own toolchain** — llama.cpp/gemma-4-E4B is the end-to-end demonstration (6/8 prompts byte-exact, 100% of claimed ops offloaded, output identical where verifiable), not a benchmark.
- The meaningful performance story is **per-op, on compute-heavy math** (the `expf`/`sqrtf`/`tanhf` chains in silu/gelu/rms_norm/softmax, and large-tensor work), where the NPU can plausibly beat the CPU — that is the comparison to produce next, plus batching/overlap to amortise the fence wait.

### 10.17 WRAP-UP DIRECTIVE (user, 2026-09-28)
"just stop after this … we already know what the npu is for."
Reading: **no new workstreams**; the NPU is understood to be for compute-heavy workloads, so no further performance demonstration is required. Actions taken:
- **Cancelled** the just-spawned compute-heavy benchmark agent (`HeavyDemo`) — that question is answered by design intent, not by more measurement.
- **Let finish**: `IntMulFix` (silent wrong integer multiply — a real correctness bug) and `GGMLBackend-4` (adopt `npu-runtime`, claim the compute-heavy ops, re-measure) — both are integration/correctness, not perf chasing.
- **Then stop.** No new agents after those two. Final state is recorded in this file (§10.1-§10.16) and `FINDINGS.md`.

### 10.18 DECISION: no SD demo — there is no plausible speedup through our path (2026-09-28)
Asked to decide on the stable-diffusion.cpp demo by whether SD could be faster on the NPU than on the CPU. Analysis from measured numbers + known architecture:
1. **SD's compute is ~99% conv + attention matmul.** Norms/activations (GroupNorm, SiLU, softmax) are ~1%.
2. **Our path can only run ACT-SHAVE kernels** patched into a carrier. Conv/matmul execute on the **DPU/tensor engines**, which npunlock cannot program (its model is ACT-range patching; MultiOpProbe's carrier census also found Add/Multiply/Relu/Clamp are DPU-only and not patchable).
3. **Measured rates**: our NPU elementwise = 0.54 M el/s (shipped warm) to 77.4 M el/s (best in-process) vs **CPU P-core f16 vector 2.4-5.4 G el/s** — so even the ~1% we could offload runs 30-10,000x slower on the NPU.
4. Net: nothing that matters can be offloaded by us; the parts we *can* offload are slower than the CPU. **No speedup, plausibly a small slowdown.** Port agent cancelled before investing.
The NPU *can* accelerate SD (Intel demoed it) — but through the **DPU/tensor-engine graph path** via the driver's own graph compiler (the `ZeGraphCreate2`/NGRAPH_LITE path we now know works on Linux, as PosixWorker used), i.e. stock OpenVINO/Level Zero, which needs none of our MoviTools replacement. Our toolchain's contribution is the **programmable ACT side** (custom kernels), which llama.cpp already demonstrated end-to-end.

### 10.19 INTEGER MULTIPLY "DEFECT" — RESOLVED: the multiply was never wrong; three mis-encodings (IntMulFix, verified by me)
**D1 — `IAU.SHR.U32 d,a,imm` was assembled to a word that is not a shift.** `encodings.json` carries **two encodings per schema-16 record**: the `gen_encodings` "table-derived" twin (wrong whenever the record list has more than one leading literal) and the `patch_encodings` "first-at-top" twin (the model validated against real vendor binaries). For `IAU.SHR.U32`/`IAU.SHL` they disagree and neither had corpus hits, so `resolve_form` preferred the broken twin. Probe: `IAU.SHR.U32 i5,i3,16` -> `c6 6e 14 18` (0x18146EC6) returned `a>>2`; with `; fmt=3` -> `c6 6e 14 88` (0x88146EC6) returned `a>>16` **exact**. Since `LowerMULHU`'s 16×16 decomposition is built from `IAU.SHR.U32 …,16`, the high half was garbage (`(a*b)>>32` gave `0xd77dfa80`, oracle `0x8c6f4246`).
**D2 — `IAU.SHR.I32 d,a,rb` shifted LOGICALLY**: the assembler's preferred form (fmt 0, `0x20000EC6`) is the wrong twin; vendor-true is **fmt 2** (`0x40000EC6`). fmt0 -> `0x0000c0de` (logical) vs fmt2 -> `0xffffc0de` (arithmetic).
**D3 — stores were emitted on `LSU0`, where they do not store at all.** Two otherwise identical kernels: `LSU0.ST.32 i3,i0` = `82 69 06 08` -> the whole output tensor comes back as uninitialised memory; `LSU1.ST.32` = `89 69 06 01` -> `0xc0dec480` exact. The backend printed `LSU0.ST.*` and relied on the assembler legality pass to rewrite the unit (the vendor only ever stores on LSU1), so **in the `--no-legalize` probe path every compiler store silently vanished** — the source of the previously "unreproducible" values.
**Fixes**: `.td` pins `SHR_U_K`/`SHL_K` to `; fmt=3` and `SHR_S` to `; fmt=2` (with the measured comment in place); all five store defs now emit `LSU1.ST.*` directly (assembled `.text` and the add1-fp16 ELF are byte-identical, `cmp`-verified; the compiler's assembly now works with *and* without the legalizer); `encodings.json` gets `encode_priority: 0` on the vendor-true shift entries (mirrored in `patch_encodings.py` so regeneration keeps it); new corpus kernel `b27_int_mul.c` (64-bit high/low half, 32-bit product, mulhu-based constant division); FINDINGS marks the old defect RESOLVED.
**Verified on hardware**: MULHU sweep 20/20 high-half exact (incl. carry cases `0xf000f000²`, `0x7bff7bff²`); `mlo`/`mhi`/`mulh`/`mdiv` all oracle-exact; `sra.c` returns `0xffffffc0` (arithmetic). Invariants: **ctest 16/16**, add1 gate exit 0 + oracle exact.
**Caveat lifted**: the "all integer kernels are suspect" note applies no longer for the multiply/shift/store paths, which are now measured; re-check any result that was measured through a `--no-legalize` probe before this fix, since stores could have been lost.

### 10.20 BACKEND 2.1x FASTER — carrier ladder + SiLU (GGMLBackend-4; verified by me)
**Root cause of the per-invocation cost**: a patched carrier executes its **whole tensor** per invocation, so device time is set by the carrier's slot count, not the payload. The engine only carried the 131072-byte (65536-slot) rung, so the model's dominant 10240-byte tensor paid 12.8x wasted work. Measured serialized medians: 16384 B 1.635 ms · 32768 B 2.059 · 65536 B 2.957 · **131072 B 4.896**.
**Fix**: a carrier ladder (16384/32768/65536/131072 B per engine op, new IRs + vendor-compiled blobs); `ggml_shave_engine_tile_for(op,payload)` picks the smallest rung ≥ payload; stats report a `tile_histogram`.
**Measured (my own run, same command as their A/B)**:
```
> What is the capital of France? …  -> "Paris"
{"ops":["add","mul","copy","silu"],"invocations":927,"offloaded":927,"declined":65357,
 "elements_offloaded":5765632,"per_op_invocations":{"add":506,"mul":421,"copy":0,"silu":0},
 "setup_ms":26.89,"npu_ms":2536.24,"avg_invocation_us":2736.0,
 "tile_histogram":{"16384":505,"32768":168,"65536":127,"131072":127},
 "engine_id":"f3b0bb81…"}
[ Prompt: 6.7 t/s | Generation: 1.1 t/s ]
```
- Per-invocation **5.16 -> 2.74 ms** (my measurement; their A/B: 5.157 -> 2.484, second pair 5.291 -> 2.599); same-run `npu_ms` **4780 -> 2536** (halved); output **byte-identical**, "Paris" correct on both.
- Model t/s: parent baseline 6.0/0.4 -> now ~8-10/1.1-1.3 (their A/B pair 6.9/0.5 -> 9.8/1.3); **CPU-only 12.9/6.2** — so still slower than CPU, as expected for cheap elementwise work.
- **New claimed op**: `GGML_UNARY_OP_SILU` (f32) with a declared tolerance (**≤4 ULP, measured worst 3**), using `kernel-lib`'s `silu_f32.elf` on Abs carriers. Op set is now ADD, MUL, CONT (bit-exact) + SILU (tolerance).
- Selftest: **PASS, 107 checks, 23 offloads** (I re-ran it).
- **Bug found and fixed**: processes that had offloaded never exited — `atexit` handlers iterated function-local `std::map` statics whose own `__cxa_atexit` destructor ran first (gdb showed a spin in `std::_Rb_tree_increment`); both maps are now deliberately-leaked heap singletons and `llama-cli`/`llama-bench` exit 0 with the stats line.

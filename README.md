# npunlock-work — final state (2026-09-28)

Self-contained index. Written at shutdown; assumes no conversation context.

## 1. The task
Create a new folder, clone `github.com/hsfzxjy/npunlock`, replace its README instruction to download
proprietary Intel/Movidius **MoviTools** with a **native, wine-free, reverse-engineered toolchain**
(from `/home/asdf/projects/r2gvp04w-re`), reimplement that toolchain so npunlock can compile custom C
for the Meteor Lake NPU's **SHAVE** cores, and prove it with the repo's add-one acceptance gate.
Later scope added by the user: use **llama.cpp** (and later `stable-diffusion.cpp`) as a **demo that the
NPU works**; support is deliberately **partial, CUDA-style** ("we don't know if SHAVE is Turing complete");
the NPU is understood to be for **compute-heavy** workloads.

## 2. Delivered and verified
| Thing | Evidence |
|---|---|
| npunlock clone with MoviTools references replaced by the native-toolchain story (README + 17 wiki/ABI docs) | `npunlock/README.md`, `npunlock/wiki/GET_SHAVE_TOOLCHAIN.md` |
| **Acceptance gate**: `cd npunlock && ./tools/npu_add1_smoke.sh --elf ../our-add1-fp16.elf` | **exit 0**, all 7 stages, `oracle exact` (`fp16_carrier_add1_exact`, 32/32), `device_id 32029` |
| Offline suite `ctest --test-dir npunlock/build/fresh` | **16/16** |
| Native toolchain: LLVM-14 SHAVE backend + from-scratch assembler + patched lld, packaged as `shave-toolchain-root/{bin/moviCompile,bin/moviAsm,bin/moviLLD,lib/mlibm.a}` | assembler gates **~188/188**, gold `.text` round-trip **192/192 byte-identical**, `obj == asm` |
| NPU carriers generated **on Linux** (was Windows-only) | `npunlock/tools/npu_carrier_smoke.sh --elements 16384` -> exit 0, oracle exact; carriers + contracts in `carriers/` |
| ggml backend for the NPU | `llama.cpp/ggml/src/ggml-shave/`; registers as `SHAVE0`; selftest **PASS** |
| llama.cpp demo on the NPU | `"The capital of France is" -> "Paris"`, output identical to CPU-only; **927 invocations, 100% of claimed ops offloaded** |
| Kernel library | `kernel-lib/manifest.json` — 23 kernels, **8 hardware-verified** (`silu_f16` exact at 8/512/8192 el/invocation) |
| Reusable runtime | `npu-runtime/` (`npurt_*`): **0.95–1.20 ms** per warm invocation, **0.38–0.48 ms** marginal batched (vs 22.9–55.4 ms recreate-per-call, 1045–1353 ms CLI flow) |
| Evaluation harness + results | `eval/run_compare.sh`, `eval/baseline/RESULTS.md` (470 lines) |

## 3. Honest limits (do not overstate)
- **Corpus 27/33 is STRUCTURAL only** (`validate_elf.py` = compile/asm/link/ELF shape). Numerically
  verified on hardware this round: `a05_scale_f32`, `l01_silu_f32`, plus the integer-multiply probes.
- **Offload does not pay for elementwise work** on this NPU through npunlock: shipped warm engine
  ~0.54 M el/s (later 2.7 ms/invocation), best in-process 77.4 M el/s, vs **CPU P-core 2.4–5.4 G el/s**.
  End-to-end NPU-enabled generation was slower than CPU-only (0.46–1.3 vs 6.2 t/s). Capability, not speed.
- Backend op set: `ADD`, `MUL`, `CONT` (bit-exact) + `SILU` (declared ≤4 ULP). Everything else falls back to CPU.
- **fp32 flushes denormals** (operand and result) — the one legitimate NPU/CPU numeric divergence.
- One blob is irreducible: the **VPU firmware** the kernel driver loads (`/lib/firmware/intel/vpu/mtl_vpu_v0.0.bin`,
  on this box via the `intel-npu-driver` snap). Everything else is open (kernel GPL, UMD MIT, compiler + OpenVINO Apache-2.0).
- Intel's own SD-on-NPU demo uses a **prebuilt INT8 blob**, splits **UNet+ on NPU / UNet− on iGPU**, and their
  iGPU beat the NPU by 62% (261% on matched INT8) — NPU value there is power, not throughput.

## 4. Still open (documented, not blocking)
Not implemented, in the order a future session should take them:
1. **Frame/spill lowering on `SP=i19`** (return address at `[i19+N-4]`, link in `i30`) — blocks `l05_rope`
   and any register-pressure kernel; today it reports a clean `report_fatal_error` instead of crashing.
2. **`GlobalAddress` selection + drop the false `ISD::BRIND = Custom` claim** (that claim makes
   `areJTsAllowed()` true and creates unselectable `br_jt` nodes) — unblocks `b18`, `b07`, `r12/r13/r36/r10`.
3. **`SDIV` Legal + `SDIVREM`/`UDIVREM` Custom + `LowerDivRem`**, then the bit builtins
   (`ctlz`=31−BSR, `cttz`=BSF, `ctpop`=`IAU.ONES`, `fabs`=`SAU.ABS.F32`, `bswap`) — unblocks `b02` and several `r*`.
4. **`fpow`** (needed by `l05_rope`).
5. Not attempted by scope: jump tables, varargs (`b13`), function pointers (`b20`), recursion (`b15`), i64 div/mod.

Resolved rather than open (kept here so they are not re-investigated):
- The **fp16 fused-store lane loss could NOT be reproduced** — the "missing aliased-store pad slot" hypothesis
  was disproven by its own author (padding 0/1/2/4 slots changes nothing; the original repro was a probe
  artifact). Best explanation is the integer/shift/store mis-encodings below, now fixed.
- **`IAU.LMULH` is measured BAD** (not a plain widened multiply; it corrupted an unrelated register). It was
  removed from selection and `ISD::MULHU` now lowers via a 16×16 partial-product decomposition, verified
  20/20 on hardware including carry cases.
- The underlying "wrong integer multiply" turned out to be three mis-encodings: a `SHR.U32` immediate word
  that is not a shift (duplicate encodings, broken twin preferred), `SHR.I32` shifting *logically*, and
  **stores emitted on `LSU0`, which does not store at all**. All fixed and hardware-verified.

## 5. What was fixed in the reimplementation (the hard-won knowledge)
Calls (`BRU.SWP i30`, args i18..i11, ret i18, SP i19, link i30) · callee-saved set measured on silicon
(`i12..i16,i19,i20..i29,i31`; **i11 is NOT preserved** by mlibm `expf`) · branch descriptor selects the
**sense** (`1b 40` = taken iff flag == 0) · **no LSU op inside a load's write-back window** (silently drops
the load) · `SAU.ADD.F32`'s third operand is a **hardware constant selector** (i1 = 1.0, i4 = 1.41494) ·
f32 `MUL`/`SUB` need `; fmt=2` and a 3-slot result latency · `IAU.MUL` needs ≥1 filler slot ·
**stores must be `LSU1.ST.*`** (`LSU0.ST.*` does not store) · `SHR.U32`/`SHR.I32` encodings were duplicate
twins with a broken one preferred → pinned to the vendor-true forms · `SAU.DIV` 13 slots ·
fp16 carriers only (npunlock rejects f32-typed carriers) · multi-kernel carriers need entries **0x400 apart** ·
carrier cost scales with **slot count, not payload** (hence the carrier ladder 16/32/64/128 KB).

## 6. Where everything lives
`npunlock/` deliverable repo · `shave-isa/` assembler+encoder · `shave-toolchain-root/` packaged toolchain ·
`llvm-14/` (at `/home/asdf/projects/llvm-14`) LLVM-14 fork + SHAVE backend · `lld-sparc/`, `packets/`,
`delta/`, `rptrace/`, `corpus/`, `mathlink/`, `loopdiff/`, `multiop/`, `npu-runtime/`, `kernel-lib/`,
`carriers/`, `eval/`, `demo-npu/` (empty; cancelled), `sd.cpp/` (cloned, not ported), `llama.cpp/`,
`models/` (gemma-4-E4B Q4_0), `npunlock-work/FINDINGS.md` (measured-rule ledger),
`npunlock-work/STATE.md` (§10.x = running log), `BYTE_EXACT_LOG.md` (in `llvm-14/`).

## 7. Reproduce the headline
```bash
cd /home/asdf/projects/npunlock-work/npunlock
NPUNLOCK_SHAVE_TOOLCHAIN_DIR=../shave-toolchain-root ./tools/npu_add1_smoke.sh --elf ../our-add1-fp16.elf
NPUNLOCK_SHAVE_TOOLCHAIN_DIR=../shave-toolchain-root ./tools/npu_carrier_smoke.sh --elements 16384
ctest --test-dir build/fresh
cd ../llama.cpp && ./build/bin/ggml-shave-selftest
GGML_SHAVE_STATS=1 ./build/bin/llama-cli -m ../models/gemma-4-E4B-it-Q4_0.gguf -dev SHAVE0 -p "The capital of France is" -n 16 --temp 0 -st
```
Scratch must be disk-backed (`export TMPDIR=/var/tmp/npu-scratch/<name>`); `/tmp` is a small tmpfs.

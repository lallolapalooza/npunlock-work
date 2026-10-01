# npu-runtime — reusable NPU execution layer for compiled SHAVE kernels

A small C library (plus benchmark/diagnostic tools) that runs a compiled SHAVE
kernel on the Meteor Lake NPU with **one call per invocation** instead of the
four-stage CLI flow per call. It is what a ggml/llama.cpp backend links when it
wants NPU offload without paying npunlock's per-call graph bring-up.

Everything lives in this directory; nothing outside it is modified. The runtime
consumes — read-only — headers, the patchblob implementation and the carrier
fixture from the `npunlock` tree, and the SHAVE toolchain from
`shave-toolchain-root`.

## Headline result (measured on this machine, MTL NPU `0x7d1d`, 2026-09-28)

| path | per invocation | source |
| --- | --- | --- |
| full CLI flow (compile + patch + bundle + probe, fresh process) | ~1 045–1 353 ms | `tools/naive_run.sh` (5 tries) |
| persistent runner, graph re-created per call (`infer_engine`, what `ggml-shave` measured) | **46 ms** | `ggml/src/ggml-shave/README.md` |
| this runtime, graph session reused (1×16 fp16 carrier) | **1.00 ms** idle host / 1.10–1.22 ms loaded (min 0.91, best p90 1.10) | `build/npu-runtime-bench --runs 200` |
| this runtime, graph session reused (1×32 fp16 carrier, `ggml-shave`'s kernel) | **1.21 ms** | `--carrier abs-abs-1x32.blob` |
| 2048-element tile (`multiop/work/ir/g2048_add1.blob`) | **1.165 ms → 1.76 M elements/s** | `npu-runtime-bench --blob … --runs 100` |
| **wide tiles from the shipped carrier path** (`npunlock/build/fresh/npu-carrier/carrier-1xN.blob`) | 1024: 1.36 ms (0.75 M el/s) · 4096: 2.02 ms (2.03 M el/s) · 16384: 4.65 ms (3.53 M el/s) | `npu-runtime-bench --blob …` |
| real fused op `silu_f16` (mlibm `expf`) on `Abs(1,1024)`, kernel-lib ELF | **1.73 ms → 0.59 M elements/s** (exact f16 vs a double-precision reference) | `npu-runtime-dump --elf kernel-lib/elf/silu_f16.elf --carrier kernel-lib/carriers/abs_1in_1x1024.blob` |
| two-input graph (kernel-lib `add_f16` on `Maximum(1,1024)`) | 1024-element `a+b` bit-exact, `npurt_run_inputs()` | `tools/run_checks.sh` step 3e |
| batched 32 executions per submission (2048-element tile) | **0.401 ms/exec → 5.1 M elements/s** | `npu-runtime-ceiling --blob …` |

So session reuse is a **~40× reduction** against the current backend and ~23×
against re-creating the graph in-process (measured median 22.7–47 ms/call with
`--recreate`, host-load dependent). Steady state is 820–1000 invocations/s for
the 1×16 carrier and ~830 invocations/s for the 1×32 carrier, i.e. **13 k–16 k
and 26.5 k f16 elements/s** with the carriers that exist today.

Measurement conditions matter: the NPU and the host are shared with other
agents, so every figure below was taken in a private scratch directory
(`/tmp/NPURuntime/...`) and confirmed by repeated runs; "idle" numbers are from
a quiet host (load < 2), "loaded" from a host at load ≈ 12, where the median
roughly doubles but the minimum barely moves (0.91 ms → 0.93 ms).

## What is per-kernel and what is per-invocation

Measured stage costs (medians; reproduce with `tools/naive_run.sh` and
`bench/bench_runtime.c`):

| stage | cost | when | reusable? |
| --- | --- | --- | --- |
| `moviCompile` → `moviAsm` → `moviLLD` | 137 + 639 + 140 ms | per kernel (per source change) | yes — cache the ELF |
| `npunlock-patchblob` (carrier + ELF → graph blob) | 3.7–19 ms | per kernel | yes — cache the blob |
| `porting_probe.py create` (manifest + hashes) | 133–230 ms | per kernel | not needed at all for execution |
| `zeGraphCreate2` (driver graph create) | 0.06–0.30 ms | per kernel (per blob) | yes — keep the handle |
| argument discovery + host allocations | 0.2–1.3 ms | per kernel | yes |
| `zeGraphInitialize` | 1.8–8.9 ms | per kernel | yes — done once |
| **graph execute + fence wait** | **0.80 ms** | **per invocation** | no |
| command list/fence create + append + destroy | 0.04–0.16 ms | per invocation | needed (no `zeCommandListReset` in the L0 subset used) |
| input/output `memcpy` (32 bytes) | < 1 µs | per invocation | no |

The one-off per-kernel cost is 6–26 ms (`warm` 3–6 ms + kernel session), i.e.
amortised after ~25 invocations. Note that the *driver's* graph create is only
0.06–0.3 ms — the 22.7 ms of the "no reuse" path is `zeGraphInitialize` plus the
driver's first-execute setup, so re-using an initialized graph handle is where
the win is.

## Build

```sh
cd /home/asdf/projects/npunlock-work/npu-runtime
make                      # build/libnpu_runtime.{a,so} + build/npu-runtime-{bench,example,test,dump,ceiling,session}
make check                # runs tools/run_checks.sh (full scoped acceptance)
```

Overridable variables: `NPUNLOCK_ROOT` (default
`/home/asdf/projects/npunlock-work/npunlock`), `NPUNLOCK_BUILD` (default
`$(NPUNLOCK_ROOT)/build/fresh`, must contain `libnpunlock.so`), `BUILD`.

Requirements: `libze_loader.so.1` (Intel NPU driver) and the three toolchain
binaries under `$NPUNLOCK_ROOT/../shave-toolchain-root` when you compile kernels
from source (`NPURT_TOOLCHAIN_DIR` overrides).

Runtime configuration comes from `npurt_config_defaults()`, which reads:

| variable | meaning |
| --- | --- |
| `NPURT_NPUNLOCK_ROOT` | npunlock source tree (carrier + ld script defaults) |
| `NPURT_TOOLCHAIN_DIR` | SHAVE toolchain root |
| `NPURT_CARRIER_BLOB` | carrier graph to patch |
| `NPURT_INCLUDE_DIR` | include dir for `<npunlock/npu3720_kernel.h>` |
| `NPURT_WORK_DIR` | scratch dir for compile artifacts (default `/tmp/npu-runtime`) |
| `NPURT_ENTRY_SYMBOL` | kernel entry (default `controlled_act`) |
| `NPURT_EXTRA_CFLAGS` | extra `moviCompile` flags |
| `NPURT_VERBOSITY` | 0 silent, 1 warnings, 2 stage trace, 3 driver-call trace |

## API

`include/npu_runtime.h` is the contract; the short version:

```c
npurt_config cfg; npurt_config_defaults(&cfg);   /* paths, timeouts, cache size */
npurt_configure(&cfg, &diag);                    /* optional, before warm()      */
npurt_warm(&diag);                               /* ONCE per process (3-6 ms)    */

npurt_kernel *k;
npurt_kernel_create(elf, elf_size, &k, &diag);   /* ONCE per kernel; cached by
                                                  * SHA-256 of the patched blob  */

npurt_run_request req = { .input = in, .input_size = 32,
                          .output = out, .output_size = 32 };
npurt_timing t;
npurt_run(k, &req, &t, &diag);                   /* PER INVOCATION               */

npurt_shutdown();                                /* releases kernels/queue       */
```

Also available:

* `npurt_compile_source(src, len, &elf, &len, &diag)` — C → SHAVE ELF through
  `moviCompile`/`moviAsm`/`moviLLD` (fork/exec, per-stage timeout, no shell).
* `npurt_kernel_create_from_blob(blob, size, &k, &diag)` — for a build-time
  pipeline that already patched the graph (e.g. `ggml-shave`'s embedded
  `shave-kernels.inc`): no patching, just load.
* `npurt_kernel_create_layered(entries, n, &k, &diag)` — patch several ACT
  groups of one carrier with different ELFs (the `abs-abs-1x32` carrier has 8
  patchable groups).
* `npurt_run_inputs(k, &{inputs, input_sizes, input_count, output, …})` — the
  multi-input form used for two-operand carriers (`npurt_run` is its one-input
  wrapper, so single-input callers are unaffected).
* `npurt_kernel_lookup(elf, size, &k)`, `npurt_kernel_info_get(k, &info)`
  (shapes, hashes, stage timings), `npurt_kernel_free(k)`,
  `npurt_cache_stats_get(&stats)`.
* `run_checks.sh`, `npu-runtime-ceiling` (batching ceiling), `npu-runtime-dump`
  (hex output, diffable against `dump_probe`), `npu-runtime-session`
  (lifecycle probe).

## Example

`examples/backend_example.c` is the copyable one (`make build/npu-runtime-example`):

```
warm: 2.5 ms
compile (moviCompile -> moviAsm -> moviLLD): 499 ms for 1172 bytes
kernel session: 11.9 ms (patch 7.2 + graph create 0.2 + arg query 0.7 + init 3.8)
second create (cache hit): 0.015 ms
run 1: 1.054 ms (device 0.778 ms)
steady state: 1.120 ms/call, 893 calls/s
output vs host oracle: exact
```

For a ggml backend the mapping is:

| backend concept | runtime call |
| --- | --- |
| device init (`ggml_backend_shave_init`) | `npurt_config_defaults` + `npurt_configure` + `npurt_warm` |
| engine/op install (one entry today) | `npurt_kernel_create_from_blob` (or `npurt_kernel_create` with the ELF) once per op/shape |
| `graph_compute` (per node) | `npurt_run` with the node's tile |
| shutdown | `npurt_shutdown` |

Measured adoption note for `ggml-shave`: their `shave-runner` pays the graph
create/initialize per request because `npunlock_infer_execute()` is a
one-shot entry point; holding the `npurt_kernel *` in the runner (or dropping
the runner) is the whole change.

`shave-runner.c` (a child process plus a pipe round trip per call) is then no
longer needed for latency; a single process can own the session. If the
existing runner is kept, it should hold the `npurt_kernel *` for the whole
process lifetime instead of calling the bring-up engine per request.

## Measured numbers

All runs used a private scratch dir (`/tmp/NPURuntime/...`); the NPU is shared
with other agents, so numbers are confirmed by repeated runs.

### Session reuse vs no reuse (same kernel, same carrier, same process)

```
$ build/npu-runtime-bench --runs 200          # 3 runs: idle host, then loaded host
first (cold) call :   15.097 / 13.214 ms     <- driver's first-execute setup
min               :    0.909 / 0.921 ms
median            :    1.001 / 1.166 ms
mean              :    1.108 / 1.32  ms
p90               :    1.102 / 1.715 ms
breakdown (median): fence wait 0.802 | session 0.039 | submit 0.079 | teardown 0.080 | prepare 0.001
throughput        :  999.3 / 858 invocations/s, 15 988 / 13 725 elements/s (16 elements/tile)

$ build/npu-runtime-bench --runs 15 --recreate     # shutdown + create + run per call
median            :   31.326 ms
breakdown (median): fence wait 2.205 | session 0.215 | submit 10.409 | teardown 0.140 | host prepare 9.679
throughput        :   31.9 invocations/s
```

### Batching / pipelining (`build/npu-runtime-ceiling`, K graph executions in one submit)

```
1x16 carrier (16 elements/graph)          1x32 carrier (32 elements/graph)
   K   total   per-exec                     K   total   per-exec
   1   912 us    912 us                     1  1100 us   1100 us
   2  1265 us    633 us                     2  1536 us    768 us
   4  2087 us    522 us                     4  2390 us    598 us
   8  3661 us    458 us                     8  3939 us    492 us
  16  6745 us    422 us                    16  7305 us    457 us
  32 14822 us    463 us
```

Marginal cost of one extra graph execution: **≈0.40 ms (16 elements), 0.41 ms
(32 elements) and 0.38 ms (2048 elements)** — i.e. flat in the element count, so
the cost is per-invocation dispatch/teardown, not per-element work. The
remaining ~0.5–0.7 ms of a single-invocation call is the submit→completion
round trip, which only shows up when the caller waits for each invocation
separately.

`npurt_run_batch()` turns that into an API: `npurt_kernel_clone()` produces
independent sessions of the same image (own graph handle and buffers), and one
submission then runs all of them with one wait. `tools/batch_probe.c` measures
it and checks every slot against the serialized result bit for bit:

```
$ build/npu-runtime-batch --width 16
  [ ok ] all 16 batch results are bit-identical to the serialized ones
  serialized : 1.226 ms/call (816 calls/s)
  batched    : 0.482 ms/call (2074 calls/s)  [2.54x]
```

(width 4 → 1.71x, width 16 → 2.54x, width 32 → 2.41x; total ≈ 0.55 ms +
N × 0.44 ms.)

### Concurrency (one `/dev/accel0`)

```
4 threads x 50 runs in one process : 0 failures, 1.137 ms/call  (vs 1.00 ms solo)
2 processes x 60 runs              : both exit 0
                                     A 456.1 calls/s, B 454.9 calls/s  (911/s aggregate)
```

Both processes complete with correct results and no device errors; the device
is time-shared, so each gets roughly half the solo throughput. Within a
process, calls serialize on one mutex, which is exactly what a single-threaded
ggml backend wants.

### Failure modes

* **A re-used fence returns early** — `zeFenceHostSynchronize` on a fence that
  already signalled from the previous submission returns before the new
  invocation has produced its output (measured directly: every second repeat
  then read the reset output window, all `0x7e00`). The runtime therefore
  creates a fresh fence per submission (the command *list* is cached, which is
  safe) and never reports a result it has not waited for.
* **Fence timeout** — a 100 µs timeout on a ~1 ms invocation returns
  `NPURT_ERR_TIMEOUT` (`0x00000001`), and the following 19 invocations on the
  same handle still pass: the session recovers, retry is safe.
* **Hard teardown** — destroying the Level Zero context and re-initialising the
  driver in the same process makes the next submit fault with SIGSEGV inside
  `libze_loader.so.1` (unloading the loader does not help). The runtime
  therefore keeps the loader/device/context for the process lifetime and
  `npurt_shutdown()` only releases kernels and the command queue; `npurt_warm()`
  re-arms the same session. Reproduced twice, and 5/5 clean with the shipped
  behaviour (`build/npu-runtime-session`).
* **Several kernels open at once** — supported: `npu-runtime-test --multi`
  holds two or more kernel sessions (distinct graph handles, distinct host
  allocations) and interleaves three invocations each; all handles stay valid
  and every kernel's output is bit-stable across passes. Handles are never
  destroyed behind the caller's back (`npurt_cache_stats::over_capacity` warns
  instead).
* **A kernel that does not return** — a miscompiled kernel (corpus `a01`/`a02`
  write guard constants; `a02` took ~4.5 s per invocation) can occupy the device
  past the fence timeout, after which the *next* submissions fail with
  `0x70000001` (`ZE_RESULT_ERROR_DEVICE_LOST`) for the rest of the process. A
  fresh process works again immediately (verified: bad-kernel run FAIL, then a
  new process passed the full acceptance test). Practical rule: keep the fence
  timeout short and treat a timeout as "restart the process", and never offload
  a kernel that has not been shown to terminate.
* **Device wedged by another process** — not observed; concurrent processes
  simply share bandwidth.

### Real kernels on wide tiles (all through this runtime)

| case | tensor | per call | rate | verification |
| --- | --- | --- | --- | --- |
| add-one, `carrier-1x1024.blob` | 1024 f16 | 1.36 ms | 0.75 M el/s | byte-exact vs npunlock's `expected-output-0.bin` (2 runs) |
| add-one, `carrier-1x4096.blob` | 4096 f16 | 2.02 ms | 2.03 M el/s | executes, repeated runs stable |
| add-one, `carrier-1x16384.blob` | 16384 f16 | 4.65 ms | 3.53 M el/s | executes, repeated runs stable |
| `silu_f16` (mlibm) on `Abs(1,1024)` | 1024 f16 | 1.73 ms | 0.59 M el/s | worst abs error 4e-6 vs double-precision silu, 6 runs bit-identical |
| `add_f16` on `Maximum(1,1024)` (two inputs) | 1024 f16 ×2 | ~1.7 ms | — | `a=-4, b=1 -> -3` exactly over all 1024 elements, twice |
| fixture add-one on `abs-add-1x16-tile1` | 16 f16 | 0.96 ms | 15.7 k el/s | byte-exact vs the project's fp16 oracle |

### Cross-validation

`tools/run_checks.sh` step 3 compiles `corpus/kernels/a05_scale_f32.c` through
the runtime and compares the output bytes with npunlock's independent ground
truth: the same kernel is built by the CLI flow and executed by
`llvm-14/tmpdump/dump_probe --bundle`, whose `ACTUAL` line is the reference.
Both sides are derived at check time, so the check survives toolchain
rebuilds (the emitted bytes for this kernel changed once already when the
clang backend was rebuilt):

```
runtime : 60 13 23 c1 e4 a5 7e 3e 00 00 80 3e 46 69 84 3e 0d f0 86 42 50 66 6d 48 1c 72 52 50 0a 7e 46 58
dump    : 60 13 23 c1 e4 a5 7e 3e 00 00 80 3e 46 69 84 3e 0d f0 86 42 50 66 6d 48 1c 72 52 50 0a 7e 46 58
```

and step 1 checks the semantic add-one oracle (`ACTUAL == EXPECT` for the
fixture kernel) bit-exactly, three times.

## What this means for offload economics

* Per invocation the host now costs ~0.05 ms (validation, copies, cached command
  list) — the rest is device/queue latency, so there is nothing left to win on
  the host side.
* Device time per invocation is **flat** up to at least 2048 elements
  (~0.38–0.44 ms), so throughput is set by the tile size alone.
* Measured elements/s (this runtime):

  | tile | serialized | batched |
  | --- | --- | --- |
  | 16 f16 | 15.7 k/s | ~40 k/s |
  | 32 f16 | 26.5 k/s | ~79 k/s |
  | 2048 f16 (`multiop/work/ir/g2048_add1.blob`) | **1.76 M/s** | **5.1 M/s** |
  | 8192 f16 | [INFERENCE] ~7 M/s | [INFERENCE] ~20 M/s |

  `multiop` independently reports 77 M elements/s at 16 384 elements, but their
  "warm execute" loop re-uses one fence across repeats; on this driver a
  re-used fence can return before the invocation completes (measured: every
  second repeat then returned the untouched output window), so that figure is a
  submission *rate*, not a completion latency. It matches the batched numbers
  here (which are completion-correct) rather than the serialized ones.
* Consequence for a ggml backend: per-op offload of a 16- or 32-element tile is
  pointless, but a carrier with a **2048+ element tile** executed from a held
  session is 1.8–5 M elements/s, i.e. within reach of a CPU core for f16
  elementwise work, and the runtime already supports it (`--blob` loads any
  patched carrier; tensor sizes come from the driver, nothing is hardcoded).
  The remaining prerequisites are outside this directory: wide carriers
  (`agent PosixWorker`, `multiop/scripts/ircompile.c`) and working arithmetic
  kernels (`agent KernelLib`).

## Limits

Respected and documented in code:

1. **One device, static shapes.** Tensor sizes and dims come from the carrier
   baked into the blob; `npurt_run` asserts both buffer sizes and the optional
   dims against the driver-reported graph tensors and returns
   `NPURT_ERR_SHAPE` on mismatch. Nothing reshapes at run time.
2. **Invocation-local memory.** The runtime only ever hands the graph its own
   driver host allocations; kernels see invocation-local chunks, as
   `npunlock/wiki/LIMITATIONS.md` describes. No strides, broadcast or aliasing.
3. **Exactly one output tensor**, and one or more input tensors. Single-input
   graphs use `npurt_run`; graphs that expose their operands as separate
   arguments (IR-built two-operand carriers) use `npurt_run_inputs` with one
   buffer per input, in graph argument order. More than one output is rejected
   at create time. Several kernels can be open at once (one graph handle each).
4. **Copies.** Input/output are copied through the driver host allocations each
   call; measured at <1 µs for 32 bytes and ~4 µs for a 2048-element tile
   (4 096 bytes each way). There is no zero-copy API yet.
   Batch/clone handles are *not* part of the kernel cache: release them with
   `npurt_kernel_free()` or `npurt_shutdown()`.
5. **Session is process-lifetime** (see failure modes). Do not expect
   `shutdown()` to return the NPU to a "fresh" state.
6. **Single-threaded queue.** All entry points take one global mutex; concurrent
   threads work (measured) but serialize.
7. **Toolchain path.** `npurt_compile_source` needs
   `moviCompile`/`moviAsm`/`moviLLD`; kernels that call libm additionally need
   `link_libraries = {"mlibm"}` in the config.
8. **Carrier `.blob` is required as input.** The runtime does not build graphs;
   it patches and executes them.
9. **A hung kernel poisons the process, not the device.** The runtime cannot
   cancel an invocation; see the failure modes above. Recovery means a fresh
   process.

## Files

| path | role |
| --- | --- |
| `include/npu_runtime.h` | public API and its contract |
| `src/npurt_api.c` | state, kernel cache, graph session, `npurt_run` |
| `src/npurt_device.c` | Level Zero bring-up, graph create/query/init, submission |
| `src/npurt_toolchain.c` | `moviCompile` → `moviAsm` → `moviLLD` driver |
| `src/npurt_util.c` | diagnostics, clock, SHA-256, configuration |
| `examples/backend_example.c` | copyable ggml-style usage |
| `bench/bench_runtime.c` | per-call benchmark (`--recreate`, `--threads`, `--timeout-us`, `--json`) |
| `tests/runtime_test.c` | semantic oracle + API/shape/state/cache checks |
| `tools/run_checks.sh` | scoped acceptance suite (see below) |
| `tools/device_ceiling.c` | batching ceiling probe (internal API) |
| `tools/batch_probe.c` | `npurt_run_batch` correctness + speedup |
| `tools/dump_output.c` | hex output, diffable against `dump_probe` |
| `tools/session_probe.c` | lifecycle probe used to find the teardown fault |
| `tools/naive_run.sh` | baseline: times each CLI stage of the old flow |

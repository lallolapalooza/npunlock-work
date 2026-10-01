# eval baseline — gemma-4-E4B-it-Q4_0, CPU vs NPU (ggml-shave)

Recorded baseline for the deterministic CPU-vs-NPU harness in `eval/`.
Written by `EvalHarness-2` on 2026-09-28.

Run directory: `eval/baseline/2026-09-28-gemma4-e4b-q4_0/`
(records + verbatim llama-cli streams per mode, `summary.md`, `summary.json`,
`npu_engine_cost.json`, `npu_cycle_cost.json`, `bench_{cpu,cpu2,npu}.json`).

## 0. Provenance

| item | value |
| --- | --- |
| harness | `eval/run_compare.sh` sha256 `ecf7199facca1be5b960fc344bcc559ada8f970e1db0c9443f0346b51650c21a`; scorer `tools/compare.py` sha256 `9c5dbef0bc560da0bfff742a89253502311042a81c39337dfcec34f93bad9e16`; runner `tools/cli_run.py` sha256 `99d0f2ac252786b40baa75de59d4fff13c2c46e51f15c5d3cb979adf266ecff2`; cost tools `tools/npu_engine_cost.py` sha256 `9071996e6f60e0fb55640edebaf5e7a5fcf38d2e07f54283aa19741b5c554e7b`, `tools/npu_cycle_time.py` sha256 `b63005328b453838ce55a19a2d1982829d7117fa68576425efb070a8b0382f8c`. The run used `--modes cpu,cpu2` (~9 min), then `--modes npu --timeout 900` (~25 min), then `--score-only --modes cpu,cpu2,npu` to regenerate the three-mode summary from the recorded evidence. |
| prompt set | `eval/prompts.json`, sha256 `15649f9730aa03b552a2e257cf0e1bb86f0a4c6b1ac5c163b847aa79624ea9a9` (8 prompts; `tools/gen_prompts.py --check` reports it up to date) |
| model | `../models/gemma-4-E4B-it-Q4_0.gguf`, 4,590,807,392 B, Q4_0, 7.46 B params |
| llama.cpp | `57b557c` + the uncommitted ggml-shave patch set (`ggml/src/ggml-shave/`, `ggml/include/ggml-shave.h`, `ggml-backend-reg.cpp`, `common/fit.cpp`) |
| backend library (frozen build under test) | `build/bin/libggml-shave.so.0.25.3`, sha256 `b0671c3c7c6c0205b59f47a2b8916a053e35d748a63e7db1545bd11dc9c640b1`, built 16:10:23, owner `GGMLBackend-3`, who froze `build/bin` for the duration of the NPU pass |
| backend library (previous build, "before") | sha256 `460682dbd38e08c6fca6ad40a86e318a5ebe98571a7aed0d0bdffb6d21cc2fdb` — 16 lanes / 32-element f16 tile, fresh graph per call, 0 nodes offloaded (§1, §4) |
| decoding contract | `-t 8 -c 4096 --temp 0 -s 1234 -co off -st --no-warmup -fit off -b 2048 -ub 512`, one process per prompt, stdin closed, bounded by `-n` and by the harness timeout (300 s CPU modes, 900 s for the NPU mode because the offloaded run is slow) |
| device (NPU mode) | `SHAVE0` → Intel Meteor Lake NPU via npunlock + npu-runtime; driver `intel-level-zero-npu 1.38.0.20260910-34487311128~ubuntu26.04`, node `/dev/accel/accel0` |
| device (CPU mode) | `none` (no device offered to the scheduler) |
| host | Intel Core Ultra 7 155H (Meteor Lake), 22 threads, Linux 7.0.0-34-generic |

Greedy decoding (`--temp 0`) with a fixed seed and a fixed thread count in every
mode; the only difference between the `cpu`/`cpu2` and `npu` invocations is
`-dev`. `cpu` and `cpu2` are the same configuration run twice, which is the
determinism anchor: any `cpu` vs `cpu2` difference would be run-to-run noise, and
there is none (§3).

**Load caveat.** The machine is shared with sibling agents working on the same
NPU and on the backend build, so the *absolute* tokens/s depend on machine load
and are recorded, not asserted. Every text comparison, every check verdict and
every offload counter in this file is load-independent.

## 1. What one NPU invocation costs, by regime

"Per invocation" means three different things in this project, and they differ
by up to ~10^4. All of them are measured; none of them is wrong. Every number in
this file is therefore labelled with its regime and its tile size.

| regime | what a call does | graph lifetime | build |
| --- | --- | --- | --- |
| **F — fresh graph per call** | Level Zero loader open → device select → context create → graph create → initialize → first execute → teardown | one call | the previous engine (`libggml-shave.so` sha256 `460682db…`): its runner called `npunlock_infer_execute()` (`npunlock/src/linux_probe/infer_engine.c:623`), which is stateless; also the npunlock CLI probe |
| **W — live graph, in-process** | execute only | the graph object survives across calls | `multiop`'s `mprobe` handle |
| **W′ — shipped engine, warm session** | stage the payload into a 131072 B tile, one npu-runtime execute, fence wait | one Level Zero session + one initialized graph per operation for the whole process | the frozen engine (`libggml-shave.so` sha256 `b0671c3c…`), `shave-engine.h`: "brings up the Level Zero session once, keeps one initialized graph handle per engine operation for the whole process and executes a patched carrier graph per call (the 'warm re-execution' path)" |

The old engine was **F by construction** — the backend's own README attributed its
cost to "the driver per-call graph create/initialize in npunlock's bring-up
engine, not by the kernel". The frozen engine implements **W′**, which is why the
same "one invocation" measurement moved from ~22 ms to ~4.9 ms without the
kernel changing.

### 1.1 Regimes F and W, measured in-process (`multiop`)

`multiop/REPORT.md` §3 + `multiop/work/timing.json` (MultiOpProbe, in-process
`mprobe`, IR-built `Abs` carriers, median of 3 runs × 50 executions, private work
dir, no concurrent device load):

| N (f16) | create ms | initialize ms | first execute ms | warm re-execute ms | one fresh cycle ms | regime-W elements/s | regime-F elements/s |
|---|---|---|---|---|---|---|---|
| 32 | 0.143 | 1.888 | 45.386 | 0.215 | 47.737 | 149 k | 670 |
| 1024 | 0.153 | 1.160 | 31.533 | 0.143 | 32.846 | 7.1 M | 31 k |
| 4096 | 0.168 | 1.124 | 48.095 | 0.241 | 50.932 | 17.0 M | 80 k |
| 16384 | 0.104 | 1.518 | 40.925 | 0.212 | 42.547 | **77.4 M** | 385 k |

The prebuilt fixture carriers (patched with the project's add-one kernel) sit in
the same band: `abs-add-1x16-tile1` 16 elements → 55.4 ms fresh cycle / 0.263 ms
warm, `abs-abs-1x32` 32 elements → 50.4 ms / 0.500 ms, `add1-shared-1x16` 16
elements → 22.9 ms / 0.418 ms.

### 1.2 Regime W′, measured through the frozen engine

The frozen engine instruments itself, so these are its own numbers, not a
reconstruction: `setup_ms` (one-off `npurt_warm()` + kernel load), `npu_ms`
(accumulated device time), `avg_invocation_us` (their mean).

| what | value | source |
| --- | --- | --- |
| one-off engine setup (session warm + 3 kernels) | 10.8–22.2 ms per process | `setup_ms` in the records |
| tile | 131072 B per input = 32768 f32 elements (`add`/`mul`), 131072 B for `copy` | `tile_bytes`/`lanes` |
| per invocation, this run's model prompts | 4.99 ms (`arith`, 2067 invocations), 5.14 ms (`short`, 927) | `avg_invocation_us` in `npu/*.json` |
| per invocation, backend's own model run | 4.85 ms | `ggml-shave` README (same stats line) |
| per invocation, idle host | ~1.0 ms device-side, 0.38–0.48 ms marginal when a batch can be formed | `npu-runtime` `make check`, backend README |
| per invocation, 15-invocation selftest on a loaded host | 7.8 ms | backend README |
| offloaded throughput, `arith` | 8.44 M elements in 10.32 s = **0.82 M elements/s** | `elements_offloaded` / `npu_ms` |
| offloaded throughput, backend's aggregate | 9.01 M elements / 8.79 s = 1.03 M elements/s ≈ 4968 elements per invocation (most model tensors fill a quarter of a tile) | backend README |

### 1.3 Reconciliation of the earlier `eval/` numbers

The earlier harness note ("one invocation ≈22 ms → ≈45 invocations/s, protocol
control ≈15 µs") is a **regime-F** number: 100 back-to-back calls through the
then-shipped `ggml-shave-runner --serve` on a fresh graph each time, tile = 32
f16 elements. It is *not* a warm number, despite the runner process being
persistent — the process was persistent, the graph was not. MultiOpProbe's fresh
cycle brackets it (22.9–55.4 ms; their cross-check against the backend README's
19.4/46/48 ms is in `REPORT.md` §3.5). MultiOpProbe's 0.14–0.5 ms is **regime
W**, an in-process live handle, and no shipped path has reached it. The frozen
engine's 4.85–5.14 ms is **regime W′**: a genuinely warm session, but each call
still pays a payload memcpy, an npu-runtime submission and the device fence wait
(~1 ms idle; 0.38–0.48 ms marginal when a batch can be formed). W′ is ~20x
MultiOpProbe's W and ~5–10x better than F.

### 1.4 Consequences of the split

* Tile size does **not** move regime F: create stays 0.10–0.17 ms and initialize
  1.1–1.9 ms across N = 32…16384, while the *first* execute is 31–48 ms at every
  N. Regime F is a per-call lifecycle constant, not a data cost — which is why
  the earlier "the same ~20 ms is spent per invocation regardless of tile size"
  observation was right.
* Tile size moves regime W's data volume directly: 149 k elements/s at N=32
  versus 77.4 M elements/s at N=16384 (512x more elements per invocation). In W′
  the same lever is weak in practice because model tensors are small: the model
  run averaged ~4968 elements per invocation, i.e. most tiles are a quarter
  full.
* So a bigger tile alone cannot rescue per-op offload, and neither does a warm
  session alone: what remains is amortising the per-invocation fence wait
  (batching *provably independent* nodes — nodes inside one backend split are
  dependent, so this needs an independence argument) and choosing kernels whose
  arithmetic per element is much higher (§5).


## 2. Baseline runs (prompt set)

Three modes, one build, identical arguments except `-dev`:

* `cpu` — `-dev none`, no device offered to the scheduler.
* `cpu2` — byte-for-byte the same configuration as `cpu`, run a second time.
  This is the determinism anchor: any `cpu` vs `cpu2` difference is run-to-run
  noise, so it is the reference against which an `npu` difference can be
  attributed.
* `npu` — `-dev SHAVE0`, the ggml-shave device registered and selectable.

Per prompt, the numbers below are llama-cli's own
`[ Prompt: P t/s | Generation: G t/s ]` line; the authoritative
prompt-processing (pp) / generation (tg) figures are llama-bench
(`-p 128 -n 32 -r 2`).

### 2.1 llama-cli, per prompt (recorded, one process per prompt)

| prompt | kind | n | cpu pp | cpu tg | cpu2 pp | cpu2 tg | npu pp | npu tg | npu vs cpu | cpu2 vs cpu |
|---|---|---|---|---|---|---|---|---|---|---|
| `arith` | arithmetic | 24 | 17.9 | 6.3 | 16.9 | 6.7 | 6.8 | 0.5 | exact | exact |
| `short` | short_answer | 16 | 16.2 | 8.1 | 15.3 | 6.2 | 7.2 | 0.5 | exact | exact |
| `repeat` | repeat_after_me | 32 | 19.5 | 7.3 | 12.0 | 5.5 | 9.0 | 0.5 | exact | exact |
| `list3` | instruction_following | 32 | 19.0 | 6.8 | 10.6 | 4.9 | 7.1 | 0.5 | exact | exact |
| `count10` | counting | 32 | 19.2 | 7.0 | 10.8 | 5.2 | 6.4 | 0.4 | exact | exact |
| `code` | code | 96 | 18.9 | 6.6 | 12.3 | 5.3 | 6.3 | 0.4 | exact | exact |
| `explain` | explanation | 96 | 13.3 | 5.4 | 12.9 | 5.0 | 6.9 | 0.5 | **DIVERGED** | exact |
| `longctx` | long_context_tail | 24 | 21.3 | 4.9 | 18.4 | 4.4 | 15.1 | 0.4 | **DIVERGED** | exact |

Mode means (`summary.md`): `cpu` 18.16 pp / 6.55 tg (23.9 s mean wall), `cpu2`
13.65 / 5.40 (26.7 s), `npu` **8.10 / 0.463** (81.4 s mean wall — generation is
~12x slower and prompt processing ~2.2x slower than the CPU run).

### 2.2 llama-bench (authoritative, `-p 128 -n 32 -r 2`, device per mode)

| mode | pp t/s | pp stddev | tg t/s | tg stddev |
| --- | --- | --- | --- | --- |
| `cpu` | 28.391 | 0.305 | 8.231 | 0.019 |
| `cpu2` | 21.725 | 1.609 | 6.546 | 0.194 |
| `npu` | 18.889 | 1.183 | 0.458 | 0.009 |

The two CPU rows differ by ~30% on pp (28.4 vs 21.7): the host is shared with
sibling agents, and the harness records timings rather than asserting them. All
three rows were produced by a by-hand invocation with the correct working
directory, because the harness's own bench step invoked `llama-bench` without
`cwd=$LLAMA_DIR` and so failed to load the relative `--model ../models/…` path
for every mode (fixed in `run_compare.sh` after this run, §6).

Note the two regimes in the `npu` column: llama-bench pre-fills 128 tokens in one
batch (18.9 t/s), while the frozen prompts are 9–2062 tokens and mostly pay
per-token overhead (6.3–15.1 t/s in §2.1). Generation is the clear loss either
way: 0.458 t/s against 8.23 t/s CPU-only, an ~18x slowdown.

### 2.3 What the prompts produced, and where `npu` differs

* 6 of 8 prompts are **byte-identical in all three modes**; all 8 pass their
  content checks in both CPU modes.
* **`npu` diverges on `explain` and `longctx`** — first differing character 47
  and 111 respectively, both inside gemma-4's `[Start thinking]` preamble (in
  `explain` the whole first reasoning line is reworded; in `longctx` the trailing
  `` ` `` of a truncated line is missing).
* `cpu` vs `cpu2` is byte-identical on **all 8** (§3), so the difference is
  attributable to the device rather than to sampling. The only documented
  deviation of this build is the fp32 **denormal flush-to-zero** in the offloaded
  `add`/`mul` kernels (`ggml-shave` README; `ggml-shave-selftest` pins it with
  six measured cases): an offloaded node whose operand or result is subnormal
  returns a different value than the CPU. One flipped greedy token rewrites the
  rest of the preamble, which is exactly the shape of both differences. I did
  **not** isolate the individual tensor and value, so the attribution is
  "consistent with the documented rule", not proven.
* `longctx` **fails its content check in every mode** (`0/1`): at `n_predict=24`
  the model spends the entire budget inside the thinking preamble and never
  reaches the passphrase (`MAGENTA-7741`) at the end of the 2062-token prompt.
  That is a property of the frozen prompt set plus this model — identical on the
  CPU baseline — not a backend effect. The prompt still does its job (2 k-token
  prompt processing) and its text remains a valid divergence oracle.
* `explain` passes its checks in all modes, including the diverged `npu` one,
  because its regex (`scatter`) matches the thinking preamble that quotes the
  instruction.
* Consequence for the harness verdict: `run_compare.sh` exits **1** on this run
  (two divergences), which is the designed behaviour — `0` when nothing diverges.


## 3. Determinism

The frozen set was run twice in identical CPU-only configuration (`cpu` and
`cpu2`, same build, same argv except nothing at all) precisely so that an
`npu`-vs-`cpu` text difference can be attributed rather than guessed.

* **8 of 8 prompts produced byte-identical generated text** on this build
  (greedy `--temp 0`, `-s 1234`, `-t 8`, one process per prompt, `-fit off`).
  There is no sampling noise to hide behind: the harness's "must match exactly"
  oracle is sound here, so any `npu` difference is a backend effect (the only
  argument that differs between the two modes is `-dev`).
* The verdict is recorded per prompt in `summary.md` (column `cpu2 vs cpu`) and
  in `summary.json` (`rows[].modes.cpu2.match_vs_baseline`).
* Kept for honesty, because it is evidence about the *oracle*, not about the
  backend: on the **previous** engine (16-lane tile, fresh graph per call,
  `libggml-shave.so` sha256 `460682db…`), two CPU-only passes over `explain`
  differed from each other at character ~208 of the generated thinking
  preamble, i.e. that build had at least one prompt where an `npu`-vs-`cpu`
  difference could not have been attributed to the device. It does not
  reproduce on the frozen build (8/8 identical above).
* Re-scoring is idempotent: `eval/run_compare.sh --score-only --out <dir>
  --modes cpu,cpu2,npu` recomputes `summary.md`/`summary.json` from the recorded
  evidence without re-running the model (no LLM call, no NPU call).

## 4. Offload statistics and node selection

Every mode records the backend's own counter line (`GGML_SHAVE_STATS=1`), once
per prompt process; `summary.md` tabulates the fields it finds, and the raw line
is inside each `<mode>/<id>.json` record and `<mode>/<id>.raw.err`.

Two schemas appear in this session, because the engine changed under the run:

| build | tile | line |
| --- | --- | --- |
| old engine, sha256 `460682db…` | 16 lanes / 32 f16 elements, fresh graph per call | `{"engine":…,"invocations":0,"offloaded":0,"declined":N,"unsupported":0,"lanes":16,"tile_elements":32,"avg_invocation_us":0.0}` — the device is consulted for every node and declines all of them |
| frozen engine, sha256 `b0671c3c…` | 32768 f32 lanes / 131072 B per input, warm session | `{"engine":…,"ops":["add","mul","copy"],"invocations":…,"offloaded":…,"declined":…,"unsupported":…,"elements_offloaded":…,"tile_bytes":131072,"lanes":{"add":32768,"mul":32768,"copy_bytes":131072},"per_op_invocations":{…},"setup_ms":…,"npu_ms":…,"avg_invocation_us":…}` |

`setup_ms` is the one-off `npurt_warm()` + kernel load, `npu_ms` the accumulated
device time of all invocations, `avg_invocation_us` its per-invocation mean.

### 4.1 Which nodes the frozen engine actually claims

From the backend's node census (`llama.cpp` run with `GGML_SHAVE_CENSUS=1`, raw
artifact `llama.cpp/../npunlock-work/ggmlback-3/work/census.txt` (sha256 `5a76ca2be91d2097b7cb0ad90f181126c18f29ce3d872c1574b1dcf241686d43`): 339 lines, **282** of them parseable
node signatures):

| node | verdict | reason | signatures |
| --- | --- | --- | --- |
| `ADD f32/f32` | **accept** | offloaded | 7 |
| `MUL f32/f32` | **accept** | offloaded | 6 |
| `MUL_MAT q4_0/f32`, `f16/f32` | decline | operation has no NPU kernel | 58 |
| `RMS_NORM f32` | decline | operation has no NPU kernel | 36 |
| `MUL f32/f32` (large/broadcast/non-dense) | decline | elementwise: not dense f32, broadcast, or larger than one tile | 36 |
| `ROPE f32/i32` | decline | operation has no NPU kernel | 24 |
| `CONT f32` | decline | copy: layout/dtype not a dense byte copy or larger than one tile | 18 |
| `SCALE f32` | decline | operation has no NPU kernel | 20 |
| `SOFT_MAX`, `GET_ROWS`, `UNARY`, `GLU`, `VIEW`, `PERMUTE`, `TRANSPOSE`, `NONE`, `SET_ROWS`, `FLASH_ATTN_EXT`, `ADD f32/f32` (oversized) | decline | see `census.txt` | 77 |

So of 282 observed node signatures, **13 are offloadable** — all of them small
dense f32 elementwise nodes (`ADD`/`MUL` with 1–2560 elements in the census,
i.e. the residual adds and the elementwise multiplies), while every `MUL_MAT`
(the actual arithmetic of the model, and the only op that would be worth
offloading) is declined for lack of a kernel.

### 4.2 This run's counters

Each prompt runs in its own llama-cli process, so the counters are per record
(the table `summary.md` prints is the last record of each mode; summed over the
eight `npu` records):

| mode | invocations | offloaded | elements offloaded | device time | per invocation | declared declines (per prompt) |
| --- | --- | --- | --- | --- | --- | --- |
| `cpu` | 0 | 0 | 0 | 0 | – | 2975 (all prompts) / 4271 (`longctx`) |
| `cpu2` | 0 | 0 | 0 | 0 | – | 2975 (all prompts) / 4271 (`longctx`) |
| `npu` | **71,383** | **71,383** (100%) | **195,395,072** | **364.9 s** | **5112 µs** | 65,357 (all prompts) / 89,567 (`longctx`) |

* every invocation served a real model tensor (`unsupported=0` in all 24
  records), split `add 3427 + mul 5987` for `longctx` and similar elsewhere —
  `copy` never fired;
* one-off engine setup (`setup_ms`) is 5.1–22.2 ms per process; the tile is
  131072 B = 32768 f32 elements per input, and the model run averaged ~2700
  elements per invocation (195.4 M / 71,383), i.e. most tiles are mostly padding;
* the per-prompt spread is large because the offloadable nodes scale with the
  work: `arith` 2067 invocations / 10.3 s of device time, `explain` (a 96-token
  thinking generation) 36,647 invocations / **184.5 s of device time**, which is
  82% of that prompt's 226 s wall clock;
* `declined` is constant per prompt within a mode (2975/4271) because it counts
  `supports_op` answers while a graph is built, not per generated token; the
  `npu` mode calls it ~22x more often (65,357) because selecting an extra device
  changes how often the scheduler asks.

## 5. Verdict: does offload pay at model scale?

**No — not for elementwise work, and not on this hardware with these kernels;
it is a capability milestone, not a throughput win.** All the numbers that
decide this are measured on this machine on 2026-09-28 and are collected here:

| what | measured | source |
| --- | --- | --- |
| CPU, vectorised f16 add, one P-core | **2.4–5.4e9 elements/s** (0.19 ns/element at N=16384) | throwaway `gcc -O3 -mavx2 -mf16c` F16C loop (source in the appendix), pinned to a P-core |
| CPU, scalar f16 add, one P-core | 1.6e8 elements/s (6.1 ns/element) | same loop, scalar body |
| CPU, f32 multiply, one P-core (numpy SIMD) | 2.60 µs at N=2560, 6.39 µs at N=10240, 12.47 µs at N=32768 → 1.0 → 0.38 ns/element | `taskset -c <P-core> python3` (numpy 2.5.3), measured while the run was in flight |
| NPU, live graph, in-process (regime W) | 0.21 ms per 8192-element invocation = **77.4 M elements/s** (38.6 ns/element) | `multiop/REPORT.md` §3.1/§3.3 |
| NPU, fresh graph per call (regime F) | 22.9–55.4 ms per invocation → 289–700 elements/s at 16–32-element tiles | `multiop/REPORT.md` §3.2, `ggml-shave` README |
| NPU, shipped engine, warm path (regime W′) | **5.11 ms per invocation** over this run's 8 prompts (71,383 invocations); 195.4 M elements in 364.9 s = **0.54 M elements/s** | this run's `GGML_SHAVE_STATS=1` lines (`npu_ms` / `invocations` / `elements_offloaded`) |
| model, CPU-only | 13.3–19.5 prompt t/s, 5.4–8.1 generation t/s | §2 |
| model, `-dev SHAVE0` with offload | **8.10 prompt t/s, 0.463 generation t/s** (81.4 s mean wall per prompt vs 23.9 s CPU-only) | §2 |

Read as economics per element, the comparison is not close in any regime:

* the NPU's **best** case (live graph, biggest tile, in-process probe) is
  ~30–50x *slower* per element than one vectorised CPU core, and the CPU has
  eight of those cores in this run;
* the shipped engine's warm path is ~2×10^4x slower per element than that CPU
  core because each call pays the npu-runtime submission plus a fence wait
  (~1 ms measured idle, ~0.4 ms marginal when a batch can be formed —
  `GGMLBackend-3`, who also notes that nodes inside one backend split are
  dependent, so batching them is not sound without an independence proof);
* a fresh graph per call is ~10^5x slower per element.

Three model-scale facts make this concrete rather than theoretical:

1. **The offloadable nodes are the cheapest ones.** The backend's node census
   (`ggmlback-3/work/census.txt`, sha256 `5a76ca2b…`, 282 distinct node signatures) accepts only 13
   dense f32 `MUL`/`ADD` signatures (`ADD` ne=[…,1,1,1] with 1–2560 elements)
   and declines every `MUL_MAT`/`ROPE`/`RMS_NORM`/`GET_ROWS`/`SOFT_MAX` with
   "operation has no NPU kernel", plus the larger dense f32 elementwise nodes
   with "larger than one tile". Every FLOP that matters stays on the CPU; what
   moves to the NPU is the share of the graph that costs the CPU least.
2. **End to end it is a loss.** Across the frozen set the offloaded run moves
   195.4 M elements in 364.9 s of NPU device time (71,383 invocations), while
   generation falls from 5.4–8.1 t/s (CPU-only) to 0.46 t/s and prompt
   processing from 13.3–21.3 t/s to 6.3–15.1 t/s (§2.1); the mean wall clock per
   prompt goes from 23.9 s to 81.4 s.
3. **The per-op arithmetic is decisive.** For `arith` the engine ran 2067
   invocations over 8.44 M elements (4083 elements each) and spent 10.32 s of
   device time. The same elementwise work on one CPU core is
   2067 × 2.6–6.4 µs ≈ 5–13 ms (measured, see the table above). The NPU spends
   ~1000x longer on those nodes than the CPU would, and the end-to-end effect is
   a ~2.7x wall-clock slowdown on that prompt (24.9 s with offload against 9.1 s
   CPU-only) because the `MUL_MAT`s — which stay on the CPU — still dominate the
   wall clock.

What could still pay, and what would have to be true:

* kernels with **far more arithmetic per element** (the kernel library now has 8
  hardware-verified kernels including `silu_f16`, exact at 8192
  elements/invocation) — the trade is still against a CPU core that does
  2.4e9 f16 adds/s, so the arithmetic-to-byte ratio has to be high;
* **overlap**: NPU work that runs while the CPU does the `MUL_MAT`s would hide
  the ~1 ms latency instead of serialising behind it;
* **batching** provably-independent nodes so the fence wait amortises.

Until one of those lands, the honest one-line answer is: per-op elementwise
offload on this NPU is a **correctness demonstration, not a speedup** — 6 of the
8 frozen prompts reproduce the CPU text byte-for-byte with 71,383 real NPU
invocations, and the two that do not (`explain`, `longctx`) differ in a
single-token-flip shape that is consistent with the documented fp32
denormal-flush rule of the offloaded kernels (and *not* with sampling, since
`cpu` vs `cpu2` is byte-identical on all 8).

## 6. How to reproduce

```sh
cd /home/asdf/projects/npunlock-work
RUN=/home/asdf/projects/npunlock-work/eval/baseline/<name>   # must be absolute

# 1. CPU baseline + determinism, same build, ~9 min
eval/run_compare.sh --modes cpu,cpu2 --out $RUN --label "<name>"

# 2. NPU pass on the frozen build, ~25 min when the backend really offloads
eval/run_compare.sh --modes npu --timeout 900 --out $RUN --label "<name>"

# 3. three-mode summary from the recorded evidence: no model run, no NPU call
eval/run_compare.sh --score-only --modes cpu,cpu2,npu --out $RUN --label "<name>"
```

Exit status is `0` when no mode diverged from the `cpu` baseline and `1` when one
did; the summary is written either way. Notes that matter:

* `--out` is made absolute by the harness before it is exported as `TMPDIR`
  (fixed during this work). With a relative `--out`, the ggml-shave engine's
  `mkdir("$TMPDIR/ggml-shave-<pid>")` runs with `cwd=llama.cpp` and fails, the
  backend reports `engine:"unavailable"` and — in `-dev SHAVE0` mode — silently
  offloads nothing.
* `--timeout 900` is needed on an offloaded build: `longctx` is a 2062-token
  prompt and prompt processing runs at ~7 t/s with offload (against ~19 t/s on
  the CPU).
* two harness bugs were found and fixed while producing this baseline (both in
  `run_compare.sh`, both invisible on a CPU-only run): the run directory was not
  made absolute before being exported as `TMPDIR` (which silently disabled the
  engine in `-dev SHAVE0` mode), and `llama-bench` was invoked without
  `cwd=$LLAMA_DIR`, so its relative `--model ../models/…` failed to load and
  every `bench_*.json` stayed empty. The `cpu`/`cpu2`/`npu` bench numbers in §2.2
  were re-produced by hand with the corrected working directory, and the fix is
  in the script (`sha256 ee203e77…`; the records themselves were produced by the
  earlier `sha256 ecf7199f…`).
* never edit `eval/run_compare.sh` while a run is in flight — bash reads the
  script incrementally and a rewrite corrupts the remainder of the run (this
  happened once here; the CPU records survived and the benches were re-run).

---

## Appendix: the CPU comparator benchmark (throwaway, not part of the harness)

`f16add_bench.c` below was compiled with `gcc -O3 -mavx2 -mf16c` and pinned to a
P-core (`taskset -c 0 ./f16add_bench`); it measures a ggml-style F16C
convert → `_mm256_add_ps` → convert loop and a scalar loop, and its numbers are
the two CPU rows in §5. It is deliberately *not* a harness tool — re-create it
from this listing if the number needs re-checking.

One trap worth recording: pinning to `-c 21` on this host lands on a low-power
E-core and reports ~1/16th of the throughput (`numpy` f32 add of 1 M elements:
10.5 ms on core 21 vs 0.64 ms on core 0). All CPU numbers quoted in this report
were taken on a P-core.

```c
#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double now(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + 1e-9 * ts.tv_nsec;
}
typedef _Float16 f16;

static void vec_add(const f16 *a, const f16 *b, f16 *c, int n) {
    int i = 0;
    for (; i + 7 < n; i += 8) {
        __m128i va = _mm_loadu_si128((const __m128i *)(a + i));
        __m128i vb = _mm_loadu_si128((const __m128i *)(b + i));
        __m256 fa = _mm256_cvtph_ps(va);
        __m256 fb = _mm256_cvtph_ps(vb);
        _mm_storeu_si128((__m128i *)(c + i), _mm256_cvtps_ph(_mm256_add_ps(fa, fb), 0));
    }
    for (; i < n; ++i) c[i] = a[i] + b[i];
}
static void scalar_add(const f16 *a, const f16 *b, f16 *c, int n) {
    for (int i = 0; i < n; ++i) c[i] = a[i] + b[i];
}

int main(void) {
    const int sizes[] = {32, 4096, 16384, 1048576};
    const int iters[] = {2000000, 20000, 5000, 100};
    for (int s = 0; s < 4; ++s) {
        int n = sizes[s], it = iters[s];
        f16 *a = aligned_alloc(64, (size_t)n * 2), *b = aligned_alloc(64, (size_t)n * 2);
        f16 *c = aligned_alloc(64, (size_t)n * 2);
        for (int i = 0; i < n; ++i) { a[i] = 1.0f; b[i] = 2.25f; }
        for (int k = 0; k < (it / 10) + 10; ++k) vec_add(a, b, c, n);
        double t0 = now();
        for (int k = 0; k < it; ++k) vec_add(a, b, c, n);
        double dtv = (now() - t0) / it;
        t0 = now();
        for (int k = 0; k < it; ++k) scalar_add(a, b, c, n);
        double dts = (now() - t0) / it;
        printf("N=%-8d vec %8.3f us/call %9.4g elem/s | scalar %8.3f us/call %9.4g elem/s | c[0]=%.2f\n",
               n, dtv * 1e6, n / dtv, dts * 1e6, n / dts, (double)c[0]);
        free(a); free(b); free(c);
    }
    return 0;
}
```

# eval — deterministic harness for the llama.cpp CPU vs NPU (ggml-shave) run

This directory answers one question with numbers instead of impressions:
**can the Meteor Lake NPU (ggml-shave / npunlock) actually serve the gemma-4-E4B
model run, and is it worth it?** It is deliberately separate from
`llama.cpp/` and `npunlock/` — it only *reads* them and never edits them.

Model under test: `../models/gemma-4-E4B-it-Q4_0.gguf`
(`ggml-org/gemma-4-E4B-it-GGUF`, ftype Q4_0, 4.59 GB, 7.46 B params).
Two modes are compared with byte-identical invocation arguments except `-dev`:

| mode | `-dev` | meaning |
| --- | --- | --- |
| `cpu` | `none` | CPU only; no device is offered to the scheduler |
| `npu` | `SHAVE0` | the ggml-shave device is registered and selectable by `ggml_backend_sched` |
| `cpu2` | `none` | optional second CPU pass (`--determinism`) to prove run-to-run reproducibility |

## Quick start

```sh
cd /home/asdf/projects/npunlock-work
eval/run_compare.sh                       # cpu + npu, writes a timestamped run dir
eval/run_compare.sh --modes cpu           # baseline only
eval/run_compare.sh --determinism --only arith,repeat,code
eval/run_compare.sh --quick --only arith  # fast smoke (short n, no llama-bench)
```

Each run writes `<out>/summary.md` and `<out>/summary.json`; read
`summary.md` first. Exit status is 0 when every expected-structure check passes
and no mode diverged from the CPU baseline, 1 otherwise.

Requirements: `llama.cpp` built with `-DGGML_SHAVE=ON` (this is what registers
`SHAVE0` and produces `build/bin/ggml-shave-runner` + the generated
`shave-kernels.inc`), the model at `../models/gemma-4-E4B-it-Q4_0.gguf`, and for
the NPU-cost steps a built `npunlock/build/fresh` plus
`npunlock-work/our-add1-fp16.elf`.

## What exactly is compared

1. **Speed.** Per prompt, from llama-cli's own
   `[ Prompt: P t/s | Generation: G t/s ]` line; per mode, the mean of those,
   plus authoritative `llama-bench` numbers (`-p 128 -n 32 -r 2`, JSON) for
   prompt-processing (pp) and generation (tg) t/s.
2. **Output equality.** Greedy decoding (`--temp 0`, fixed `-s 1234`), fixed
   `-c 4096`, fixed `-t 8`, fixed `-b/-ub`, one process per prompt, bounded by
   `-n` *and* a timeout, stdin closed (the binary is a chat build and would
   otherwise enter an interactive loop). For every prompt the generated text of
   each mode is compared with the `cpu` baseline **exactly** (string equality).
   The expectation is *exact* — an oracle-exact path must not change a single
   character — so any divergence is reported as `DIVERGED` and fails the run.
   The CPU path itself is verified reproducible before that claim is made: see
   `baseline/determinism`. `-c 4096` is not cosmetic: the long-context prompt is
   ~2062 tokens and at `-c 2048` llama-cli rejects the request outright
   (`Error: request (2062 tokens) exceeds the available context size`) and
   generates nothing. Records now carry that error text and are marked
   `**RUN ERROR**` instead of silently failing a content check.
3. **Expected-structure checks.** Every prompt carries content checks
   (regex / normalised echo / line shape / minimum length) declared in
   `prompts.json`. These catch "the backend ran but produced garbage" without
   hard-coding a single reference completion for the whole model.
4. **Offload statistics.** `GGML_SHAVE_STATS=1` makes the ggml-shave backend
   print one machine-readable line at exit
   (`ggml-shave-stats {"engine":…,"invocations":…,"offloaded":…,"declined":…,
   "unsupported":…,"lanes":…,"tile_elements":…,"avg_invocation_us":…}`).
   The harness records it per run and tabulates every field it finds, so the
   backend's own claim of what it executed on the NPU is in the summary.
5. **NPU host cost** (the number that decides whether per-op offload can ever
   pay off) — `tools/npu_engine_cost.py` and `tools/npu_cycle_time.py`; see
   below.

## The prompt set — `prompts.json`

Frozen, versioned, and regenerable with `tools/gen_prompts.py` (`--check`
verifies the file on disk matches the generator). Eight prompts covering the
kinds that a backend change can plausibly break:

| id | kind | what it exercises |
| --- | --- | --- |
| `arith` | arithmetic | exact product 47\*89 → `4183` |
| `short` | short answer | capital of France |
| `repeat` | repeat-after-me | verbatim echo, punctuation included |
| `list3` | instruction following | exactly three lines, each a colour |
| `count10` | counting | `1 2 3 … 10`, order and separators |
| `code` | code | `def add(a, b)` / `return a + b` |
| `explain` | explanation | ≥25 words about scattering |
| `longctx` | long-context tail | ≈2 k tokens of filler; answer (`MAGENTA-7741`) only in the final line |

`longctx` is the one that makes prompt-processing hurt (it dominates the
wall-clock of a full pass: ~2062 tokens, so a single run is ~2 minutes against
~15 s for the others) and the only one whose answer cannot be reached by prior
knowledge. It is also the reason the harness pins `-c 4096`.

## How to read `summary.md`

* **Speed** — two tables. The CLI means are what the user actually experiences
  (they include the first-token latency of each process); the llama-bench table
  is the reproducible pp/tg comparison (with stddev over repetitions).
* **Expected-structure checks** — `PASS (n/m)` per mode per prompt, plus an
  `exact` / `**DIVERGED**` verdict of each non-baseline mode against `cpu`.
* **Offload statistics** — the backend's counters. In the recorded baseline
  `npu` shows `invocations=0, offloaded=0` and a large `declined`: the device is
  registered and consulted for every node, but the engine only carries a
  16-lane f16 ADD tile, so `supports_op` declines all model tensors and
  `ggml_backend_sched` falls back to the CPU for the whole graph. That is the
  honest, expected state today — an exact output match is therefore *also* the
  evidence that registering the backend does not perturb the CPU result.
* **NPU host cost** — see the next section.

## NPU host cost (why per-op offload does not pay off today)

Three regimes exist, and the harness reports each with its label (the full
derivation, the sources and the reconciliation are in
`baseline/RESULTS.md` §1):

| regime | what a call does | per invocation | source |
| --- | --- | --- | --- |
| **F** fresh graph per call | loader → device → context → graph create → initialize → first execute → teardown | 22.9–55.4 ms | `multiop/REPORT.md` §3, `ggml-shave` README; the previous engine drove exactly this per frame (`npunlock/src/linux_probe/infer_engine.c` is stateless) |
| **W** live graph, in-process | execute only | 0.143–0.5 ms (149 k → 77.4 M elements/s at N=32…16384) | `multiop/REPORT.md` §3 (mprobe) |
| **W′** shipped engine, warm session | payload staging into a 131072 B tile + one npu-runtime execute + fence wait | ≈1.0 ms idle / **4.85 ms measured in a model run** | engine's own `npu_ms`/`avg_invocation_us`; `ggml-shave` README |

`tools/npu_engine_cost.py` measures W′ the way the engine reports it (it runs
`ggml-shave-selftest` with `GGML_SHAVE_STATS=1` and turns `setup_ms` / `npu_ms` /
`avg_invocation_us` into per-invocation and elements/s figures);
`tools/npu_cycle_time.py` measures the reference CLI flow
(patchblob → bundle → verify → `npunlock-linux-probe`), which pays regime F per
deployment (~275 ms total, of which ~28 ms is the NPU run). The older
`npu_invocation_bench.py` is gone: it drove the child `ggml-shave-runner` frame
protocol, which the warmed engine removed.

**Plainly:** a per-offload-node design pays the regime it uses per offloaded
ggml node. At 4.85 ms (W′) a per-token graph with thousands of offloadable nodes
cannot win against a CPU core doing 2.4e9 f16 adds/s, and at 22–55 ms (F) it
loses by orders of magnitude. Offload pays only when a single invocation covers
work whose per-element cost is high (an `expf`/`rsqrtf` chain), or when NPU and
CPU work overlap. See `baseline/RESULTS.md` §5 for the measured verdict and §4
for which nodes are even eligible.

## Files

```
eval/
  README.md                     this file
  prompts.json                  frozen prompt set + expected-structure checks
  run_compare.sh                the harness (see --help)
  tools/gen_prompts.py          regenerates/validates prompts.json
  tools/dump_prompts.py         materialises prompts as files + TSV manifest
  tools/cli_run.py              one bounded, parsed llama-cli run -> JSON record
  tools/compare.py              scores records, compares modes, writes summary
  tools/npu_engine_cost.py      per-invocation cost, from the engine's own
                                setup_ms/npu_ms/avg_invocation_us counters
  tools/npu_cycle_time.py       patch+probe cycle cost (reference flow)
  baseline/RESULTS.md           recorded numbers + the NPU cost-regime table
  baseline/<run>/               recorded runs: summary.md, summary.json, records
```

`baseline/RESULTS.md` is the written-up result: per-prompt prompt-processing and
generation t/s, the exact/diverged verdicts, the backend's offload counters, and
the NPU host cost split by regime (fresh graph vs live-graph re-execution) with
the verdict on whether per-op offload can pay at model scale.

A recorded run directory contains, per mode: `<id>.json` (parsed record),
`<id>.raw.out` / `<id>.raw.err` (verbatim llama-cli streams, so any claim can be
re-checked without re-running the model), `<id>.cli_run.log`, `bench_<mode>.json`
(llama-bench JSON), `meta_<mode>.json` (device, ctx, threads, seed), and at the
top level `devices.txt`, `npu_engine_cost.json`, `npu_cycle_cost.json`,
`summary.json`, `summary.md`.

All tools are stdlib-only python3 + bash; no wine, no network.

## Determinism and fairness rules

* Greedy (`--temp 0`) with a fixed seed and fixed thread count. Thread count
  matters: CPU reduction order changes with `-t`, so `-t` is pinned and recorded.
* Identical arguments in both modes; only `-dev` differs.
* `-fit off` is passed in **both** modes. It started as a workaround: with
  `-dev SHAVE0`, `--fit` (the memory auto-fit) raised SIGFPE inside
  `common/fit.cpp` during model load (exit 136) because the fit path divided by
  a device bracket that collapses to zero (`mem_high[id] == mem[id]`). A
  zero-bracket guard for exactly that division is present in the working tree
  (`git diff common/fit.cpp`, two guards); this harness has not re-verified the
  `--fit on` + `-dev SHAVE0` path itself, so `-fit off` stays in the frozen
  decoding contract. It also keeps the CPU and NPU invocations identical apart
  from `-dev` and avoids a memory-fit walk that offloads nothing here.
* `eval/baseline/determinism` records the same prompt set twice on the CPU
  (`cpu` vs `cpu2`) so the "outputs match exactly" claim is anchored in a run
  where no new backend is involved at all.  The canonical baseline run does the
  same over the full set in one pass (`--modes cpu,npu --determinism`), so
  `cpu2` is available there too.
* `--out` is made absolute before it is exported as `TMPDIR`.  This matters: the
  ggml-shave engine creates its scratch dir as `$TMPDIR/ggml-shave-<pid>` inside
  a process whose cwd is `$LLAMA_DIR`, so a relative run dir used to make that
  `mkdir` fail (`engine:"unavailable"`, offload silently disabled).
* Timings are recorded, not enforced; the harness never fails a run for being
  slow. It fails only on a check failure or an output divergence.

## Reproducing the recorded baseline

```sh
cd /home/asdf/projects/npunlock-work
eval/run_compare.sh --out eval/baseline/<name> --label "<name>"
eval/run_compare.sh --out eval/baseline/determinism --modes cpu,cpu2 \
    --only arith,repeat,code --skip-cost
```

Then compare the new `summary.md` against the recorded one — speeds will move
with machine load, the `PASS`/`exact` verdicts and the offload counters should
not.

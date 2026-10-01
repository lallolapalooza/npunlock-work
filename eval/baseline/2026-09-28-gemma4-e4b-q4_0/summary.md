# eval comparison — gemma4-e4b-q4_0 cpu/cpu2/npu (32768-lane engine b0671c3c)

- prompt set: `prompts.json` (8 prompts)
- model: `../models/gemma-4-E4B-it-Q4_0.gguf`
- baseline mode: `cpu`

## Speed (llama-cli per-prompt means)

| mode | device | prompt t/s | generation t/s | wall s (mean) |
|---|---|---|---|---|
| cpu | `none` | 18.163 | 6.55 | 23.906 |
| cpu2 | `none` | 13.65 | 5.4 | 26.65 |
| npu | `SHAVE0` | 8.1 | 0.463 | 81.358 |

## Speed (llama-bench, authoritative)

| mode | pp t/s | pp stddev | tg t/s | tg stddev |
|---|---|---|---|---|
| cpu | 28.391 | 0.305 | 8.231 | 0.019 |
| cpu2 | 21.725 | 1.609 | 6.546 | 0.194 |
| npu | 18.889 | 1.183 | 0.458 | 0.009 |

## Expected-structure checks and exact-output agreement

| prompt | kind | cpu checks | cpu2 checks | npu checks | cpu2 vs cpu text | npu vs cpu text |
|---|---|---|---|---|---|---|
| `arith` | arithmetic | PASS (1/1) | PASS (1/1) | PASS (1/1) | exact | exact |
| `short` | short_answer | PASS (1/1) | PASS (1/1) | PASS (1/1) | exact | exact |
| `repeat` | repeat_after_me | PASS (1/1) | PASS (1/1) | PASS (1/1) | exact | exact |
| `list3` | instruction_following | PASS (1/1) | PASS (1/1) | PASS (1/1) | exact | exact |
| `count10` | counting | PASS (1/1) | PASS (1/1) | PASS (1/1) | exact | exact |
| `code` | code | PASS (2/2) | PASS (2/2) | PASS (2/2) | exact | exact |
| `explain` | explanation | PASS (2/2) | PASS (2/2) | PASS (2/2) | exact | **DIVERGED** |
| `longctx` | long_context_tail | FAIL (0/1) | FAIL (0/1) | FAIL (0/1) | exact | **DIVERGED** |

## Backend offload statistics (`GGML_SHAVE_STATS=1`)

| mode | engine | invocations | offloaded | declined | unsupported | lanes | avg_invocation_us | elements_offloaded | npu_ms | ops | per_op_invocations | setup_ms | tile_bytes |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| cpu | ready | 0 | 0 | 4271 | 0 | {'add': 32768, 'mul': 32768, 'copy_bytes': 131072} | 0.0 | 0 | 0.0 | ['add', 'mul', 'copy'] | {'add': 0, 'mul': 0, 'copy': 0} | 13.18 | 131072 |
| cpu2 | ready | 0 | 0 | 4271 | 0 | {'add': 32768, 'mul': 32768, 'copy_bytes': 131072} | 0.0 | 0 | 0.0 | ['add', 'mul', 'copy'] | {'add': 0, 'mul': 0, 'copy': 0} | 10.35 | 131072 |
| npu | ready | 9414 | 9414 | 89567 | 0 | {'add': 32768, 'mul': 32768, 'copy_bytes': 131072} | 4974.6 | 25713152 | 46831.0 | ['add', 'mul', 'copy'] | {'add': 3427, 'mul': 5987, 'copy': 0} | 10.94 | 131072 |

`offloaded`/`invocations` are the backend's own counters; `declined` is the number of `supports_op` calls answered false, i.e. nodes the scheduler left on the CPU.

## NPU host cost (per-op-offload economics)

- **one NPU invocation through the shipped engine (`ggml-shave-selftest`): 7620.5 us (best of 3 runs, median 7637.1 us) -> 131 invocations/s**, tile 131072 B, ops ['add', 'mul', 'copy'], one-off engine setup 7.63 ms, best 1489353.5 elements/s offloaded
- this is the *shipped-engine* regime: payload staging plus one npu-runtime execute per call, as the engine's own `npu_ms` / `avg_invocation_us` report it.  It is not the 0.14-0.5 ms live-graph re-execution measured in-process in `multiop/REPORT.md` section 3, and not the 22-55 ms fresh-graph lifecycle of the previous engine either.
- full patch+probe cycle (patchblob -> bundle -> verify -> NPU run), the reference flow with no worker reuse: 243.4 ms -> **4.1 kernel deployments/s**
  (patchblob 4.3 ms, bundle create 116.6 ms, verify 90.3 ms, NPU run 31.0 ms)

An elementwise ggml op on a model tensor is tens of microseconds of CPU work; a per-invocation round trip of milliseconds cannot amortise unless one offloaded node replaces far more than one tile.



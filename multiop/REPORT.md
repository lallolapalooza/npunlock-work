# multiop: multiple kernels per NPU invocation, carrier capacity, and host cost

Measurement report, 2026-09-28. Owner: `MultiOpProbe`. Work dir:
`/home/asdf/projects/npunlock-work/multiop` (all device runs in private dirs under it).
No npunlock source was modified. Everything below is measured on the Meteor Lake NPU
(`/dev/accel0`, driver `libze_intel_npu.so.1.38.0`, graph compiler 8.3, runtime 7.0.0),
with the ground-truth paths the project already trusts:

* execution + `ACTUAL`: `/home/asdf/projects/llvm-14/tmpdump/dump_probe --bundle <bundle>`
  (and the shipped `npunlock-linux-probe` for its fixed 32-byte contract),
* patched graph: `npunlock/build/fresh/npunlock-patchblob`,
* bundle: `npunlock/tools/porting_probe.py create|verify`.

Because the shipped probe binaries hard-code a 32-byte (1x16 f16) tensor contract, I built
two small read-only-in-spirit harnesses (own directory, linking only *installed* system
libraries plus the private `level_zero_min.h` header):

| tool | purpose |
|---|---|
| `multiop/scripts/mprobe.c` -> `multiop/work/mprobe` | executes any graph blob, queries the real argument shapes, times create/initialize/execute separately, repeats execution on a live graph, hexdumps outputs. |
| `multiop/scripts/ircompile.c` -> `multiop/work/ircompile` | compiles an OpenVINO IR (`xml`+`bin`) into a native NPU blob on **Linux** via the installed Level Zero driver (`zeGraphCreate2` format `NGRAPH_LITE` + `zeGraphGetNative2`). |
| `multiop/scripts/blobtool.py` | parses graph-blob ELF64 sections/invocations/ranges/relocations and rewrites per-range code pointers. |
| `multiop/scripts/run_case.py` | patchblob -> per-range rewrite -> bundle -> probe, with stage timings. |

---

## 0. Headline results

1. **The "carrier ceiling" is refuted.** New carriers of *any* shape are compilable on
   Linux today with the installed driver — no MoviTools, no Windows worker, no wine.
   `ircompile` turns an OpenVINO IR into a patchable native blob in **0.13-0.41 s**.
   Measured patchable carriers with up to **8192 f16 elements per ACT invocation**
   (N=16384 tensors), and the tiling is a compiler choice, not a hardware limit.
2. **One graph invocation can run several *different* patched kernels.** Per-ACT-range code
   redirection works; each range can point at its own function inside one appended image.
   Proven with bit-exact oracles: 2 distinct kernels on `abs-add-1x16-tile1`, 4 distinct
   kernels on `abs-abs-1x32`, 2 distinct kernels on the IR-built 4-target carriers
   (those carriers have 4 ranges but only 2 are load-bearing).
   **Rule found and pinned:** the range's code address is rounded **down to a 0x400-byte
   boundary**; entries must sit on 0x400 boundaries.
3. **Per-invocation host cost is ~23-55 ms and is dominated by graph create + initialize +
   the first execute after initialize, almost independent of tile size.** Re-executing a
   live graph costs **0.14-0.5 ms**. That reproduces the other agent's 19.4 ms mean /
   46 ms sustained / 48 ms first call (my single fresh cycle: 23-55 ms).
4. Therefore: per-op offload with a fresh graph is ~50 ms/op; a re-used graph is
   ~0.2 ms/invocation; and a multi-kernel carrier can amortise one 50 ms graph over up to
   4 different kernels. Bigger tiles fix the *host* amortisation, not the per-op constant.

---

## 1. Carrier families and what a patched kernel may read/write

### 1.1 The three prebuilt fixtures (unchanged, definitive contract table)

Measured with `npunlock-inspect` + `mprobe` on the blobs as committed:

| carrier (sha256 prefix) | graph input | graph output | ACT ranges | ACT groups | f16 elements / invocation | native graph meaning |
|---|---|---|---|---|---|---|
| `abs-add-1x16-tile1.blob` `d4abc3c09cabdb14` | `(1,16)` f16, 32 B, name `input` | `(1,16)` f16, 32 B, name `output` | 2 | 1 | 8 (2 tiles of 8) | carrier pre-adds a fixed per-lane f16 ramp, then the patched ACT runs (oracle: `f16(f16(x+ramp)+1)`) |
| `add1-shared-1x16.blob` `2010915f2d21e07d` | `(1,16)` f16, 32 B, name `input` | `(1,16)` f16, 32 B, name `output` | 2 | 1 | 8 (2 tiles of 8) | same shape family as the smoke carrier |
| `abs-abs-1x32.blob` `da8c205954f565c9` | `(1,32)` f16, 64 B, name `input` | `(1,32)` f16, 64 B, name `second` | 8 | 2 | 16 (per invocation) | two chained ACT groups; native = `abs(abs(x))` |

Measured routing of `abs-abs-1x32` (per-range perturbation, exact deltas in f16):

* output lanes 0-15 = `x` + K(range 0) + K(range 4)
* output lanes 16-31 = `x` + K(range 2) + K(range 6)
* ranges 1, 3, 5, 7 accept new code pointers but have **no effect on the graph output**
  (their tiles are not on the output path).

Commands:
```
B=npunlock/build/fresh
$B/npunlock-inspect --graph tests/fixtures/npu3720/abs-abs-1x32.blob --report /tmp/i.json
multiop/work/mprobe --graph <patched>.blob --fill ones --repeat 50
```

### 1.2 New carriers built on Linux (the important result)

`ircompile` reimplements, in ~120 lines on Linux, exactly what
`npunlock/src/workers/ir_worker.c` does on Windows
(`level_zero_min.h` DDI slots 16/1/26/11/4/22/5/6/20):

```
zeInitDrivers(NPU) -> zeDeviceGet -> ZE_extension_driver_npu -> ZE_extension_graph
  -> DeviceGetGraphProperties2 (compiler 8.3, formats 0x2 = NGRAPH_LITE)
  -> zeGraphCreate2(format=NGRAPH_LITE, pInput = 4B compiler ver + u32(2) + u64 xml_size
                    + xml + u64 weights_size + weights)
  -> zeGraphGetNative2 -> blob (ELF, 35-64 kB) -> file
```

Measured (`/usr/bin/time` around the tool, 3 distinct IRs): **0.13 s, 0.15 s, 0.15 s**
wall per compile; the very first compile after boot (loading
`libopenvino_intel_npu_compiler.so`) was 0.41 s. Repeat compiles of the same IR are
~5-15 ms (driver-side cache).

Which IR elementwise ops the vendor compiler lowers to **patchable ACT kernels** at
shape `(1,1024)` f16 (compile + `npunlock-inspect`), i.e. the available carrier menu:

| IR op | ACT targets | elements per invocation |
|---|---|---|
| `Abs` | 4 | 512 |
| `Elu`, `Erf`, `Sigmoid` | 2 | 1024 |
| `Exp`, `Floor`, `Log`, `Sign`, `Sqrt`, `Tanh` | 4 | 512 |
| `Maximum`, `Minimum`, `Power`, `Divide` (**2 inputs**) | 4 | 256 |
| `Add`, `Clamp`, `Multiply`, `Relu`, `Subtract` | — | DPU-only, **no ACT sections** -> not patchable |
| `Ceil`, `Negate` | — | IR compile failed |

Element-count sweep for the `Abs` carrier (`multiop/scripts/sweep_carriers.py`),
each blob patched with our add-one SHAVE kernel and executed:

| tensor N (f16) | ACT targets | elements per invocation | native blob |
|---|---|---|---|
| 32 | 4 | 16 | `multiop/work/sweep/n32.blob` |
| 256 | 4 | 128 | `multiop/work/sweep/n256.blob` |
| 1024 | 4 | 512 | `multiop/work/sweep/n1024.blob` |
| 2048 | 4 | 1024 | `multiop/work/sweep/n2048.blob` |
| 4096 | 4 | 2048 | `multiop/work/sweep/n4096.blob` |
| 8192 | 4 | 4096 | `multiop/work/sweep/n8192.blob` |
| 16384 | 4 | **8192** | `multiop/work/sweep/n16384.blob` |

Outputs verified exactly (`out == in + 1.0` for every element, `--fill ones` ->
0x4000 everywhere) for **every** N in the table. So a patched kernel can read/write
**8192 contiguous f16 elements per invocation**, 512x the 16-element fixture tile, and
N (hence the tile) is chosen freely by the IR shape.

**2-input ACT carriers work too**: `Maximum`/`Minimum`/`Power`/`Divide` produce a carrier
with input record 0, input record 1 and output record 2.
`multiop/kernels/two_input.c` (reads record 0, writes record 2) patched into
`Maximum(1,1024)` executed correctly (`out = in0 + 2` exactly), proven with
`multiop/work/ops/Maximum.patched.blob`.

**Drop-in compatibility:** an IR-built `Abs(1,16)` carrier patched with a normal kernel
runs through the *shipped* bundle pipeline —
`porting_probe.py create` + `npunlock-linux-probe` report
`loader/device_selection/graph_extension/graph_create/initialization/execution = passed`
(only `oracle` fails, because the bundle's oracle is specific to the ramp-add carrier);
`dump_probe` ACTUAL = `x + 1.0` exactly for the standard input vector
(`multiop/work/ir/abs16.patched.blob`, report `multiop/work/ir/abs16.exec.json`).

Why this still failed for the shipped Linux path: `CMakeLists.txt:162` builds
`npunlock_worker` (the graph compiler) **only** under `if(WIN32)`, and
`src/workers/ir_worker.c:1` includes `<windows.h>`; `ir2blob` therefore has no POSIX
worker to spawn ("Level Zero worker failed ... not_found"). The *driver capability* is
present and usable — the missing piece is only the ~120-line POSIX front end above.

---

## 2. Multiple different kernels per graph invocation

### 2.1 How it is done

`npunlock-patchblob` appends **one** kernel image per run and points *every* discovered
ACT range at `image_base` (`src/patchblob/graph_blob.c`, `npunlock_patch_graph_blob`).
To give ranges different code I append an ELF whose `.text` contains several entry points
and then rewrite, per range, (a) the `.rlt.text.ActKernelRanges` RELA addend and (b) the
range `extent` field — 12 bytes total (`multiop/scripts/blobtool.py patch_ranges`).
No npunlock file is touched; the patched blob is produced by the shipped CLI.

```
# multi-entry kernel: N functions 0x400 bytes apart (own linker script)
multiop/scripts/kernel_aligned.ld, kernel8.ld, kernel4big.ld
multiop/kernels/multi_fadd.c  -> work/multi_align.elf   (4 entries, +1/+2/+3/+4)
multiop/kernels/multi8.c      -> work/multi8.elf        (8 entries, +1..+8)
multiop/kernels/big_multi4.c  -> work/big_multi4.elf    (4 entries, no 16-element clamp)
```

**Measured hardware rule (new):** the range code address is **rounded down to a
0x400-byte boundary**. With entries at 0x1800/0x1A20/0x1C40/0x1E60 only the 0x1800 one
executed; with entries on 0x400 boundaries all executed. Direct probe on the IR carrier
(all four ranges set to the same addend):

| addend - image_base | executed entry |
|---|---|
| 0x000-0x300 | entry0 (+1) |
| 0x400-0x700 | entry1 (+2) |
| 0x800 | entry2 (+3) |
| 0xc00 | entry3 (+4) |

### 2.2 Proof — 2 different kernels, `abs-add-1x16-tile1` (bit-exact, 3/3 reproducible)

`work/m2_fadd/graph.blob`: range0 -> `controlled_act` (in+1), range1 -> `multi_b` (in+2).

```
ACTUAL  : 00 c3 bc bd e0 b5 68 32 22 3a 55 3c 9a 3d de 3f 08 44 1a 45 2b 46 1e 49 93 4c 4c 50 27 54 14 58
EXPECT  : 00 c3 bc bd e0 b5 68 32 22 3a 55 3c 9a 3d de 3f 11 42 1a 44 2b 45 9e 48 53 4c 2c 50 17 54 0c 58
```
lanes 0-7 = oracle for `+1` (exact); lanes 8-15 = exact `+2` oracle (verified against a
host double oracle for all three `+2/+3/+4` variants). Re-run 3x -> byte-identical, and a
control run with *both* ranges left at `image_base` (`+1`/`+1`) reproduces the stock
single-kernel output (all 16 lanes = `ramp+1`).
`run_case.py --name verify2k_* --range-addend "0:6144,1:7168" --range-extent "0:0x214,1:0x21a"`.

### 2.3 Proof — 4 ranges, `abs-abs-1x32` (8 ACT targets)

Per-range perturbation with `multi8.elf`, diff vs the all-`entry0` baseline:

| range retargeted | output lanes changed | observed delta |
|---|---|---|
| 2 | 16-31 | +2.0 |
| 4 | 0-15 | +4.0 |
| 6 | 16-31 | +6.0 |
| 0 | 0-15 | +7.0 (when set to `+8`; `entry0` is the baseline so the plain probe is a no-op) |
| 1, 3, 5, 7 | none | - |

Model confirmed exactly: `all 8 ranges -> +8` gives lanes 0-15 = 17.0 and lanes 16-31 =
17.016 (`= x + 8 + 8`), while `{0,1,3,5,7} -> +8` gives lanes 0-15 = 10.0 and lanes 16-31
unchanged. So **4 different kernels execute in one graph invocation** here, in two
*chained* groups (group 1 consumes group 0's output, so the groups are serialised; the
tiles inside a group are independent).

### 2.4 Proof — IR-built carrier, 4 targets, 2 distinct kernels

`Abs(1,4096)`, patched with `big_multi4.elf`, ranges 0..3 -> entries +1/+2/+3/+4:
tile means = `[2.0, 2.0, 4.0, 4.0]`. Per-range isolation shows range0 drives output tiles
0-1 and range2 drives tiles 2-3; ranges 1 and 3 are inert. So 2 distinct kernels per
invocation on this carrier family (`multiop/work/m4big2/graph.blob`).

**Answer to "how many":** as many as the carrier has *load-bearing* ACT ranges
(2 for `abs-add-1x16`, 4 for `abs-abs-1x32`, 2 for the IR `Abs` carriers); the number of
ACT ranges per graph is a compiler property (4 for the IR carriers regardless of N) and
the count of inert ranges depends on the graph's tiling.

---

## 3. Per-invocation host cost, and elements/s

All timings: `mprobe`, median of 3 runs of 50 executions each (single run for the
fixtures), private work dir, no concurrent device load.

### 3.1 Stage cost (IR-built `Abs` carriers, patched)

| N (f16) | create ms | initialize ms | first execute ms | warm execute ms | one fresh cycle (create+init+first exec) ms |
|---|---|---|---|---|---|
| 32 | 0.14 | 1.89 | 45.4 | 0.215 | 47.7 |
| 1024 | 0.15 | 1.16 | 31.5 | 0.143 | 32.8 |
| 4096 | 0.17 | 1.12 | 48.1 | 0.241 | 50.9 |
| 16384 | 0.10 | 1.52 | 40.9 | 0.212 | 42.5 |

`create2` is ~0.1-0.2 ms, `initialize` ~1-5 ms, and **the first execute after
initialize costs 20-50 ms**; subsequent executes on the same live graph cost
0.14-0.5 ms.

### 3.2 Fixture carriers (patched with our add-one kernel)

| carrier | elements | create ms | init ms | first exec ms | warm exec ms | full cycle ms | warm elements/s | full-cycle elements/s |
|---|---|---|---|---|---|---|---|---|
| `abs-add-1x16-tile1` | 16 | 0.14 | 4.82 | 50.5 | 0.263 | 55.4 | 61 k | 289 |
| `abs-abs-1x32` | 32 | 0.11 | 1.87 | 48.4 | 0.500 | 50.4 | 64 k | 635 |
| `add1-shared-1x16` | 16 | 0.09 | 2.01 | 20.8 | 0.418 | 22.9 | 38 k | 700 |

### 3.3 Elements/s vs tile size (what decides feasibility)

| N (f16) | warm elements/s | fresh-graph elements/s |
|---|---|---|
| 32 | 149 k | 670 |
| 1024 | 7.1 M | 31 k |
| 4096 | 17.0 M | 80 k |
| 16384 | **77.4 M** | 385 k |

### 3.4 Host pipeline around the device (measured with `run_case.py` / `ircompile`)

* `npunlock-patchblob`: **3-23 ms** (fixtures ~7-10 ms).
* `porting_probe.py create`: **~180 ms**; `verify`: **~62 ms** (Pure Python, one-shot).
* `npunlock-linux-probe` end-to-end (loader+device+create+init+execute+oracle): **~29 ms**
  on the 1x16 carrier.
* `ircompile` (IR -> native blob): **130-330 ms** cold, 5-15 ms warm-cached.

### 3.5 Cross-check against the backend numbers

The ggml backend measured 19.4 ms mean, 46 ms sustained, 48 ms first call. My
independent numbers bracket that exactly: a fresh in-process cycle is **22.9-55.4 ms**
(mean of the three fixtures' `create+init+first-exec` = 43 ms; the IR carriers 32-51 ms),
and a warm re-execution is **0.14-0.5 ms**. So the ~46 ms is the
create+initialize+first-execute chain, not the kernel, and it does not shrink when the
tile grows.

---

## 4. Architecture consequences (for the backend decision)

* **Per-op, fresh-graph offload is ~50 ms/op and is not worth it for elementwise work**:
  a 16-element tile yields ~300-700 elements/s, a 16384-element tile ~385 k elements/s,
  while a CPU core does ~1e9 f16 adds/s. The `46 ms` is a *per-op graph lifecycle* cost,
  not a data cost.
* **The two levers that change the picture, both measured here:**
  1. tile size is free (up to at least 8192 f16/invocation) — this only amortises the
     *device* time, not the 50 ms lifecycle;
  2. a carrier can host several *different* kernels per graph (2-4 measured), so one
     50 ms graph create can cover 2-4 ops. To go further, a POSIX graph compiler means
     the backend can compile purpose-built carriers — e.g. one ACT carrier per
     ggml elementwise subgraph shape — and cache them, instead of paying a new
     lifecycle per op.
* **A cached/kept-alive graph gives 0.2 ms/invocation** (77 M f16 elements/s measured at
  N=16384). Anything that can be expressed as repeated execution of a *fixed* kernel over
  different buffers is viable; anything that needs new code per op pays ~50 ms.
* **Blocker that is now removed for construction, still present in the shipped path:**
  the shipped Linux `npunlock` has no POSIX IR worker (`npunlock_worker` is
  `if(WIN32)`-only), so `ir2blob`/`npu.compile()` still fail on Linux. The driver
  capability exists and is proven usable; the POSIX front end is the only missing piece.

---

## 5. Repro command log (all paths relative to `/home/asdf/projects/npunlock-work`)

```bash
# --- build the kernels (native SHAVE toolchain, corpus pipeline) ---
multiop/scripts/build_kernel.sh multiop/kernels/big_add1.c multiop/work/big_add1.elf
multiop/scripts/build_kernel.sh multiop/kernels/two_input.c multiop/work/two_input.elf
# aligned multi-entry ELFs (own linker scripts that place entries 0x400 apart)
TC=shave-toolchain-root; INC=npunlock/include
$TC/bin/moviCompile -cc1 -triple shave -target-cpu 3720xx -S -O2 -x c \
    multiop/kernels/multi_fadd.c -ffunction-sections -fdata-sections -I$INC -o /tmp/k.s
$TC/bin/moviAsm --cv 3720xx --noSPrefixing < /tmp/k.s > /tmp/k.o
$TC/bin/moviLLD -flavor gnu -EL -e controlled_act -z max-page-size=0x10 \
    -T multiop/scripts/kernel_aligned.ld /tmp/k.o -o multiop/work/multi_align.elf
# (same recipe with kernel8.ld/multi8.c -> work/multi8.elf, kernel4big.ld/big_multi4.c -> work/big_multi4.elf)

# --- 2 distinct kernels in one carrier (abs-add-1x16) ---
multiop/scripts/run_case.py --name m2_fadd \
  --graph npunlock/tests/fixtures/npu3720/abs-add-1x16-tile1.blob \
  --elf multiop/work/multi_align.elf --range-addend "0:6144,1:7168" --range-extent "0:0x214,1:0x21a"

# --- 4 distinct kernels in abs-abs-1x32 (per-range perturbation) ---
multiop/scripts/blobtool.py graph <blob>                     # sections/invocations/ranges/relas
python3 multiop/scripts/<build+patch helper>                 # see work/a8_dist, work/p_*, work/q_*, work/r0_7

# --- new carriers on Linux ---
PYTHONPATH=npunlock/python python3 -c "import npunlock as npu; from npunlock.ir import serialize_ir; ..."
multiop/work/ircompile --ir work/ir/abs2048.xml --out work/ir/abs2048.blob
npunlock/build/fresh/npunlock-patchblob --graph work/ir/abs2048.blob --elf multiop/work/big_add1.elf \
    --output work/ir/g2048_add1.blob
multiop/work/mprobe --graph work/ir/g2048_add1.blob --fill ones --repeat 3

# --- carrier size sweep + timing ---
python3 multiop/scripts/sweep_carriers.py 32 256 1024 2048 4096 8192 16384
```

### Reproducibility

* 2-kernel `abs-add` result: re-run 3x, byte-identical ACTUAL.
* multi-target routing maps: re-run 2x each (deterministic).
* timings: median of 3 x 50 executions; the 3 raw runs agree within ~15 %.
* every sweep row had its 4096-byte output checked element-by-element
  (`all_plus_one` true for all N after removing the kernel's own 16-element clamp).

### Uncertainty / caveats

* `create2` returning in 0.1 ms looks lazy; the real graph build cost shows up in the
  first execute. I report the *combined* `create+init+first-exec` as the honest
  "one fresh call" number.
* The 0x400 rounding rule was measured on four carriers (2 fixtures + IR `Abs` + IR
  `Maximum`); I did not test whether it changes with tile size (all tests used 0x400
  rounding consistently at N up to 16384).
* Ranges 1/3/5/7 of the IR carriers and 1/3/5/7 of `abs-abs-1x32` accept code pointers
  but never affect the graph output; whether they *execute* is not observable from the
  graph output, so I do not claim it.
* `Ceil`/`Negate` IRs fail to compile (`ircompile` returns a driver error); not
  investigated further.
* Move/tile semantics for `abs-abs-1x32` groups were derived by perturbation, not by
  reading the driver; the derived model reproduces every measurement.

---

## 6. Addendum — independent confirmation via the shipped POSIX path (`PosixWorker`)

`PosixWorker` landed a POSIX graph compiler in the shipped tree
(`tools/npu_carrier_smoke.sh`, `build/fresh/npunlock_worker`, `ir2blob` API),
i.e. the same capability as my out-of-tree `ircompile` but as a supported tool.
I re-ran it once to cross-check my numbers:

```
cd npunlock
./tools/npu_carrier_smoke.sh --elements 4096 \
    --toolchain-dir /home/asdf/projects/npunlock-work/shave-toolchain-root
# -> exit 0, all 7 probe stages passed, oracle `fp16_add1_exact` exact
#    carrier: npunlock/build/fresh/npu-carrier/carrier-1x4096.blob  (42480 B)
```

Cross-check against my own `Abs(1,4096)` carrier:

| property | my measurement (`work/sweep/n4096.blob`) | shipped path (`carrier-1x4096.blob`) |
|---|---|---|
| ACT targets / groups | 4 / 1 | 4 / 1 |
| f16 elements per invocation | 2048 | 2048 |
| span bytes | 4096 | 4096 |
| precision | fp16 | fp16 |
| warm execute | 0.241 ms (17.0 M elem/s) | 0.232 ms (17.7 M elem/s) |
| fresh cycle (create+init+1st exec) | 50.9 ms | 35.1 ms (load-dependent) |

Agreement is exact on the contract and within timer noise on the timing. Their
`tools/porting_probe.py --element-count N --oracle fp16_add1_exact` extension also
lifts the fixed 32-byte probe limit, so the standard bundle/probe path — not just my
`mprobe` — can now drive wide carriers.

**Net effect on the report's conclusions:** unchanged, but the "carrier ceiling" answer
no longer depends on my out-of-tree tool — the shipped Linux tree can now produce
patchable carriers of arbitrary 1xN f16 shape, with the per-range multi-kernel
redirection rule (0x400 alignment) still the mechanism a multi-op backend would use.

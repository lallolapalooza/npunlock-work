# kernel-lib — precompiled SHAVE/NPU3720 kernel library

Ready-to-patch ACT kernels for the Intel Meteor Lake NPU (`/dev/accel0`, Level
Zero), built with the native (wine-free) SHAVE toolchain
(`shave-toolchain-root`), plus the manifest that maps a ggml operation to the
ELF that implements it.

The point is to take kernel compilation off the run-time path: the ggml-shave
backend (or NPURuntime's held-session runtime) picks a kernel from
`manifest.json`, patches it into a carrier graph it already has, and only the
*graph* ever needs a compiler.

```
kernel-lib/
  build.sh              reproducible build: toolchain -> elf/ -> validate -> manifest.json
  manifest.json         machine-readable index (contracts x build x NPU evidence)
  engine-ops.py         the ENGINE_OPS tuple ggml-shave's build_kernels.py consumes
  src/                  one .c per op/dtype; src/<name>.opts = per-kernel build flags
  src/npunlock/         vendored ACT ABI header, src/shave_kernel.ld, src/kernel_quad.ld
  elf/<name>.elf        built kernels (sha256 recorded in the manifest)
  carriers/             IR-built carriers + their measured ACT contracts (JSON sidecars)
  tools/make_carrier.py   build a patchable carrier on Linux (driver graph compiler)
  tools/patch_ranges.py   point one ACT range at its own kernel entry (multi-op carriers)
  tools/klib_probe.c      arbitrary-shape NPU execution probe (npunlock's engine)
  tools/run_npu.py        hardware evidence runner (ACTUAL vs host reference)
  tools/make_manifest.py / fixup_asm.py / elf_entries.py / validate_elf.py
  reports/npu-evidence.json   raw ACTUAL/EXPECT/INPUT + measured toolchain rules
```

## Status (2026-09-28)

| op | dtype | form | where it runs today | verification |
| --- | --- | --- | --- | --- |
| `add` | f16 | packed pair | `abs-abs-1x32` (16 el/inv), `Abs(1,1024/16384)` (512/8192) | **byte-exact**; byte-identical to ggml-shave's pinned add |
| `add` | f16 | two records | `Maximum(1,1024/16384)` (256/4096) | **byte-exact** |
| `mul` | f16 | packed pair | `Abs(1,1024/16384)` | **byte-exact** in the first window |
| `mul` | f16 | two records | `Maximum(1,1024/16384)` | **byte-exact** |
| `scale` | f16 | two records | `Maximum(1,1024/16384)` | **byte-exact** |
| `copy` | f16 | unary | all carriers | **byte-exact** (byte-transparent) |
| `silu` | f16 | unary | `abs-add-1x16`, `Abs(1,1024/16384)` | **bit-exact, 0 mismatched elements** (mlibm `expf`) |
| `gelu` | f16 | unary | `abs-add-1x16`, `Abs(1,1024/16384)` | within 2e-3 (fp16 arithmetic; see tolerances) |
| `multi4` | f16 | 4 entries, one image | `abs-abs-1x32`, `Abs(1,1024/16384)` | **byte-exact**: range 0 = packed mul, range 2 = copy, one invocation |
| `mul`/`scale`/`add`/`vec_dot` | f32 | both | compiles+validates | no f32 carrier built yet |
| `silu`/`gelu` | f32 | unary | compiles+validates | no f32 carrier built yet |
| `rms_norm`, `softmax`, `rope` | f16/f32 | reductions | source present | toolchain item; see `manifest.json` per-kernel `build` block |

Exact per-kernel numbers (compile/assemble/link/validate, `.text` bytes, ELF
sha256, entry offsets, elements per invocation, ACTUAL vs reference, wall time):
`manifest.json` → `kernels[]`. Raw device bytes: `reports/npu-evidence.json`.

## Where this plugs into the consumers

**ggml-shave** (`llama.cpp/ggml/src/ggml-shave/`) consumes an `ENGINE_OPS` table
plus a drop-in directory of ELFs. `engine-ops.py` is generated verbatim by the
build:

```python
ENGINE_OPS = (
    {"name": "add",  "class": "binary", "lanes": 16,
     "stage0": "add_f16_packed.c", "stage1": "copy_f16.c"},
    {"name": "copy", "class": "unary",  "lanes": 32,
     "stage0": "copy_f16.c",       "stage1": "copy_f16.c"},
)
```

Verified integration: driving the backend's own `tools/build_kernels.py` with
that tuple and `--kernels kernel-lib/src --prebuilt kernel-lib/elf` pins
`add_f16_packed.elf` + `copy_f16.elf`, patches both ACT groups of
`abs-abs-1x32.blob` and emits `shave-kernels.inc` (573402 bytes, 2 engine ops).

**NPURuntime** consumes `kernels[].elf` + `entry_symbol` (always
`controlled_act`), `links_mlibm`, and the `{group: elf}` plan recorded in
`kernels[].execution[0].plan` (its `npurt_kernel_create_layered` has the same
shape as `tools/run_npu.py`'s plan).

## How a backend picks a kernel

1. **op** — `op`, `ggml_ops[]`, `dtype` in `kernels[]`.
2. **shape contract** — `constraints` + `abi_form` (`unary`, `binary`,
   `unary_scalar`, `packed_pair`, `reduce_rows`, `reduce_all`, `multi_entry`).
3. **carrier** — `carriers[]` gives each carrier's IR op, tensor shape, input
   count, ACT groups/targets, **elements per invocation**, load-bearing ranges
   and sha256; `kernels[].execution` records which carrier actually ran it.
   A `packed_pair` kernel needs a single-input carrier whose invocation window
   has a partner window `element_count` away; a `binary` kernel needs a carrier
   with two input records (records 0/1 → output 2).
4. **status** — only `EXECUTED_VERIFIED` (byte-exact) and
   `EXECUTED_VERIFIED_TOLERANT` have run on the NPU against a host reference.
   `VALIDATED` = compiles/links/passes the npunlock ELF contract, never run.
   `COMPILE_FAIL` carries the verbatim diagnostic in `build.message`.
5. Anything unmatched stays on the CPU — the capability gate belongs to the
   backend, the library only reports what it has.

## ACT ABI

`void controlled_act(unsigned layerParams)`; `layerParams` points at 0x28-byte
records (`src/npunlock/npu3720_kernel.h`):

| field | meaning |
| --- | --- |
| `params + 0x28*k + 0x00` | data pointer of record `k` |
| `params + 0x0c` | rank (1..15) |
| `params + 0x10` | pointer to `rank` u32 dimensions |
| `element_count` | product of the dimensions = the **invocation-local chunk** |

Every kernel indexes the full `element_count`, so one ELF serves every
invocation of a patched range. Reduce-row kernels take `ne00 = dims32[0]` and
walk rows with `base + ne00 <= element_count`. The chunk is the *compiler's*
choice (512 f16 on `Abs(1,1024)`, 8192 on `Abs(1,16384)`), never the whole
tensor — kernels must not assume otherwise.

## Carriers

Carriers are the one thing the library cannot synthesise from a kernel alone.
Three families exist today:

| carrier | how it is made | inputs | elements/invocation | verified |
| --- | --- | --- | --- | --- |
| `abs-abs-1x32`, `abs-add-1x16-tile1` | prebuilt vendor fixtures | 1 | 16 / 8 | add, copy, silu, gelu, multi4 |
| `abs_1in_1xN`, `maximum_2in_1xN` | `tools/make_carrier.py` (IR → Linux driver graph compiler) | 1 / 2 | 512 / 256 (N=1024), 8192 / 4096 (N=16384) | all of the above at large tiles |
| `shipped:*` | npunlock's `tools/npu_carrier_smoke.sh --elements N` (PosixWorker) | 1 or 2 | compiler-chosen | recorded in the manifest when present |

`Add`/`Multiply`/`Relu`/`Clamp`/`Subtract` IR ops lower to DPU-only code and are
**not patchable**; use `Abs`/`Exp`/`Tanh`/… (unary) or `Maximum`/`Minimum`/
`Divide`/`Power` (2-input) and overwrite the ACT code with a kernel.

## Multi-op carriers (several kernels, one graph invocation)

One graph invocation can run *different* kernels in different ACT ranges: append
the multi-entry image (`src/multi4.c`, entries forced to 0x400 boundaries by
`src/kernel_quad.ld`) and re-point each range with `tools/patch_ranges.py`
(rewrites the `.rlt.text.ActKernelRanges` RELA addend and the range extent).
The alignment rule is measured: **a range's code address is rounded down to
0x400**, so entries must sit on 0x400 boundaries.

Verified on hardware, byte-exact, at all three tile sizes:

```
./tools/run_npu.py --case multi4_pair32      # abs-abs-1x32, 16 el/inv
./tools/run_npu.py --case multi4_abs1024     # Abs(1,1024),   512 el/inv
./tools/run_npu.py --case multi4_abs16384    # Abs(1,16384), 8192 el/inv
```

range 0 → `kb_mul_packed` (+0x800), range 2 → copy (+0x000): the first window
computes `a*b` against the second window, the second echoes itself, in a single
invocation.

Open observation (not blocking): re-pointing a range at the image's *mlibm
caller* entry (`kb_silu`, +0xc00) does not complete on the device, while the
same silu code patched alone runs bit-exact on the same carriers. The
mlibm-free entries are the ones verified above.

## Measured toolchain rules that shape the sources

The full list with evidence pointers is `manifest.json` →
`measured_toolchain_rules` and `reports/npu-evidence.json`:

* **D1** fp32 ADD/SUB of two *runtime* registers needs the `; fmt=2` register
  hint (`tools/fixup_asm.py`); without it the silicon reads the operand as a
  hardware-constant selector. `add_f16_packed` is byte-exact with the hint.
* **D2** a *literal* that is one of the nine hardware constants (1.0, 0.5, 2.0,
  3.0, 1.41494, 0.70604, π, e, 10.0) must NOT be rewritten — the compiler
  encodes it as a selector on purpose. Kernels with such literals carry
  `FIXUP=off` in `src/<name>.opts` (`silu_*`).
* **D3** fp16-typed arithmetic is emitted with the correct register form by the
  compiler itself, which is why `mul_f16`, `scale_f16`, `gelu_f16` and the
  packed entries evaluate in `__fp16` (the fp32 multiply of two runtime
  registers was measured not to compute while that fix was in flight).
* **D4** mlibm CALLs work: `expf`, `tanhf` and `sqrtf` return values that make
  `silu_f16` bit-exact against a host `numpy` reference.
* **D5** a constant used *after* a call in fp16-typed code is emitted in the
  register form without being materialised; kernels spell such expressions
  without constants after the call (`gelu_f16` computes `hx + hx*tanh`).

## Host references and tolerances

References are computed on the host with `numpy` float64 → float32 → float16
(the *host* libm), and the comparison result is reported two ways: exact
element match and max absolute error. Basis per kernel:

| kernel | reference | result |
| --- | --- | --- |
| add/mul/scale/copy, packed forms | fp16 arithmetic in numpy | byte-exact (0 mismatched elements) |
| silu f16 | `x/(1+exp(-x))` in float64 | **bit-exact** — the vendor `expf` agrees with the host reference on all measured lanes |
| gelu f16 | the same polynomial with every intermediate rounded to fp16 | max abs error 4.9e-4 (16 el) / 2e-3 (1024/16384 el); the residual is the fp16 arithmetic path plus vendor `tanhf`'s last bit |

Vendor-vs-host libm differences are therefore *absorbed into a stated
tolerance* for `gelu_f16` and shown to be absent for `silu_f16`; the manifest
records `tolerance` and `max_abs_error` per case.

## Latency

Per-invocation cost with a *persistent* session (measured by the npu-runtime
workstream): **0.80 ms graph execute + fence wait**, ~1.0–1.2 ms per reused
call, ~46 ms when the graph is re-created per call, 5.1 M elements/s batched at
2048 elements. `kernel-lib`'s own probe (fresh process, one graph create per
run) measures 0.03–0.09 s end-to-end, i.e. the graph-recreate class — it is
recorded as `mean_wall_seconds` and is **not** kernel time. The lever is the
carrier's tile size (free up to at least 8192 f16/invocation), not kernel
tuning.

## Reproducing

```sh
cd /home/asdf/projects/npunlock-work/kernel-lib
./build.sh                 # ELFs + validation + manifest.json + engine-ops.py
./build.sh --npu           # the above + hardware evidence
python3 tools/make_carrier.py --op Abs --shape 1024          # unary carrier
python3 tools/make_carrier.py --op Maximum --shape 1024 --inputs 2
python3 tools/run_npu.py --list
python3 tools/run_npu.py --case add_bin1024 --repeat 3
```

`build/toolchain-stamp.txt` records the sha256 of every toolchain component
used; `manifest.json` records sha256 for every source, ELF and carrier. Build
with the **live** `shave-toolchain-root` (the `corpus/toolchain-snapshot*`
trees are stale). The build retries a stage up to four times when the shared
toolchain wrapper fails without a diagnostic (sibling agents relink
`llvm-14/build/bin/clang` and `ld.lld` live); genuine diagnostics are never
retried.

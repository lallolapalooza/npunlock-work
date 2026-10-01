# MATHLINK_REPORT — does the `mlibm.a` math-library path work with the native SHAVE toolchain?

Owner: agent `MathLink`. All artifacts under `/home/asdf/projects/npunlock-work/mathlink/`
(scripts + `src/` repros + `out/` ELFs + `logs/` stage transcripts). Nothing outside this
directory was edited; `/home/asdf/projects/npunlock-work/npunlock` was only executed.

## 0. Verdict (short)

| half of the path | verdict | evidence |
|---|---|---|
| **Link half** — our object with `tanhf` relocations → vendor `lib/mlibm.a` → `npunlock_parse_shave_elf` | **WORKS** | §3–§6: `VALID` ELF, `tanhf`+`expf`+`floorf` pulled in, no undefined symbols, no leftover relocations, all applied fixups numerically exact; **169/204** public libm symbols link, 144 of them into contract-valid images |
| **Compile half** — a C kernel that *calls* `tanhf` | **BLOCKED (compiler)** | §2: `SHAVE: calls are unsupported in wave 1` — `SHAVETargetLowering::LowerCall` is an unconditional `report_fatal_error` |
| **End-to-end GELU / "arbitrary user C" with libm** | **FAILS today** | §2.1/§2.2 (two independent compile-stage blockers); §2.3 (`npu.compile` can't even reach `shavecc` in this environment) |
| **Runtime** — ELF containing resolved mlibm `.text` executed by the ACT carrier | **executes** (no fault/hang) | §7: all probe stages passed incl. `execution`, `oracle` failed because the probe intentionally stores nothing (§7.1 shows that failure is not math-specific) |

Single-sentence verdict: **the toolchain can link vendor `mlibm.a` code into a valid 3720xx
ACT kernel image, but it cannot yet compile any kernel that calls into it, because the SHAVE
backend has no call lowering (`LowerCall` aborts) — and the `MLIBM_DEFINE_LINK_COMPAT` helper
block that the repo's own GELU examples rely on additionally trips the unimplemented signed-compare
path on the current clang build.**

## 1. Environment and exact tool versions

Peer agents were editing `llvm-14/` and `shave-isa/shave_asm.py` throughout this session, so every
measurement below carries the tool identity that produced it.

```
$ sha256sum /home/asdf/projects/npunlock-work/shave-toolchain-root/lib/mlibm.a
4769ac96201c3fbd4c974151a631a42b8339b30ea26d8f54072375092bda54a3  .../lib/mlibm.a
```
→ identical to the archive documented in `npunlock/ABI/MLIBM_SYMBOLS.md`.

| tool | identity during this run |
|---|---|
| `bin/moviCompile` → `llvm-14/build/bin/clang` | symlink to `clang-14`; rebuilt 4× while I worked (sha256 seen: `9f2eea9b…`, `393e3143…`, and **`18da5b3ac3cfc373…` at the 13:13 final re-check**) |
| `bin/moviAsm` → `shave-isa/shave_asm.py` | changed under me several times (`b58d18af… → 65f9b2… → 39a155e8… → 5cb174a5… → 9174ac6f…`); peer `AsmEncodingRE` owns it |
| `bin/moviLLD` → `llvm-14/build/bin/ld.lld` | md5 `211d19e13617c1559d9a2af7b506994d` |
| linker script | `npunlock/src/shavecc/shave_kernel.ld` (unmodified) |
| validator | `npunlock/build/fresh/libnpunlock.so:npunlock_parse_shave_elf`, via `validate_elf.py` |

**Final re-verification (13:13, clang `18da5b3a…`, shave_asm `9174ac6f…`)**: every headline result below
was re-run on this revision — `tanhf_min_nocompat.c` still dies with *calls are unsupported*,
`compat_only.c` still dies with *unsupported condition code 20*, `compat_off.c` still compiles clean,
and `out/tanhf_linkprobe_safe.elf` still links (`rc=0`) / validates `VALID` / 640 B with its 200/202/209
fixups formula-exact and executes on the NPU with all stages but `oracle` passed. Only the *sizes* of
probe-owned sections can drift with `shave_asm.py` revisions; the `mlibm.a`-derived numbers are stable.

Reproduce the validator verdict for any ELF:

```sh
cd /home/asdf/projects/npunlock-work/mathlink
python3 validate_elf.py out/tanhf_linkprobe_safe.elf
# VALID   out/tanhf_linkprobe_safe.elf: status=0 (ok) file_offset=0xc0 size=640 (0x280) address=0x1d000000 entry=0x1d000000
```

## 2. Compile half — BLOCKED

### 2.1 A kernel that calls `tanhf` cannot be compiled

`src/tanhf_min.c` (the minimal GELU-shaped kernel: load invocation, loop, `out[i] = tanhf(in[i])`)
built stage-by-stage with the exact argv `shavecc.c` uses (`kernel_build.sh`):

```sh
$ ./kernel_build.sh src/tanhf_min_nocompat.c tanhf_min_nocompat
compile rc=1
entry.c:9:16: warning: implicitly declaring library function 'tanhf' with type 'float (float)'
fatal error: error in backend: SHAVE: calls are unsupported in wave 1 (callee GlobalAddress); see TARGET_NOTES.md
```

Source of record (`llvm-14/llvm/lib/Target/SHAVE/SHAVEISelLowering.cpp:823-835`):

```cpp
SDValue SHAVETargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                                       SmallVectorImpl<SDValue> &InVals) const {
  ...
  OS << "SHAVE: calls are unsupported in wave 1 (callee " ...
  report_fatal_error(StringRef(OS.str()), false);
```

→ unconditional abort; **no** call (direct or through a symbol) can be emitted today, so *any* kernel
referencing `tanhf`, `expf`, `sqrtf`, … is uncompilable. This is a backend gap, not a linker or
archive problem.

### 2.2 Second, independent compile blocker: the `MLIBM_DEFINE_LINK_COMPAT` helper block

The repo's GELU examples (`npunlock/examples/example_gelu{,_f32}.py`) both start with

```c
#define MLIBM_DEFINE_LINK_COMPAT 1
#include <npunlock/npu3720_kernel.h>
```

Minimal isolation (`src/compat_only.c` vs `src/compat_off.c` = same TU, only the `#define` added/removed):

```sh
$ ./kernel_build.sh src/compat_only.c compat_only      # MLIBM block ON
compile rc=1
fatal error: error in backend: SHAVE: unsupported condition code 20 in wave 1
            (floating-point and signed compares are unsupported); see TARGET_NOTES.md

$ ./kernel_build.sh src/compat_off.c compat_off        # MLIBM block OFF
compile rc=0
assemble rc=0
```

cc 20 = `ISD::SETLT`. The trigger is `__fixsfdi` in `npunlock/include/npunlock/npu3720_kernel.h`
(`if (exponent < 0) { return 0; }`). The backend only implements *unsigned* i32 compares
(`SHAVEISelLowering.cpp:987-1006`; `default:` → the fatal above), and on the **current** clang
build the optimizer no longer folds that `exponent < 0` into an unsigned test.

**This is an in-flight regression, not a permanent property:** at 12:58 (older clang build) the very
same TU compiled cleanly and produced a working object — `out/compat_helpers.o`, 892 B,
sha256 `7f0dda8f8abcadbb063705c6c324107dba1985d296cd344fa93b34a9ed66681e`, `nm`: `strtof`, `__truncdfsf2`, `__fixsfdi`. The body it
emitted for that test is the preserved evidence that the *code shape* is compilable:

```
__fixsfdi:                              // @__fixsfdi
	IAU.SHR.U32 i0, i18, 23            // bits.u >> 23
	LSU0.LDIL i1, 0xFF ; IAU.AND i2, i0, i1
	LSU0.LDIL i1, 0x7F
	CMU.CMII.U32 i1, i2                // <-- UNSIGNED compare (127 vs masked exponent)
	LSU0.LDIL i3, 0x01 ; IAU.XOR i1, i1, i3
	BRU.BRA i1, LBB2_10                // signed `exponent < 0` lowered as an unsigned test
	…
```

Re-running the identical source with the 13:0x binaries fails with cc 20, so today's build no longer
performs that fold. (`out/compat_helpers.s` is 0 B: a later failing `kernel_build.sh` run truncated it
before the compiler aborted; the 12:58 object is intact and is what §6.2 links.)

### 2.3 Consequences for the two GELU examples

```
$ PYTHONPATH=…/npunlock/python NPUNLOCK_NATIVE_DIR=…/npunlock/build/fresh \
  NPUNLOCK_SHAVE_TOOLCHAIN_DIR=…/shave-toolchain-root python3 src/example_gelu_f32.py
npunlock._native.NativeError: ir2blob failed: not_found (6):
 {"status":"not_found","stage":"ir2blob.compile","message":"Level Zero worker failed …"}
```

The example dies even earlier: the IR→blob stage needs the Level Zero graph-compiler worker, which
does not exist in this (Linux, no MoviTools graph compiler) environment. So the examples cannot reach
`shavecc` here at all; their kernel-side failure is the one reproduced directly in §2.1/§2.2.

## 3. The archive: what `tanhf` actually pulls in

```sh
$ ar t shave-toolchain-root/lib/mlibm.a
halfmath.o math_lite.o shave_fenv.o shave_fp64.o shave_fpclassify.o shave_signbit.o
ef_j0.o ef_j1.o ef_jn.o wf_j0.o wf_j1.o wf_jn.o ef_sqrt.o
```

`tanhf` is `math_lite.o:.text.tanhf` (0xc9 = 201 B); its relocations are

```
.rela.text.tanhf: @0x23 type 209 (.text.tanhf+0x8b)      self branch
                  @0x8d type 202 (expf+0)   @0x90 type 200 (expf+0)     call to expf
.rela.text.expf : @0x1d type 209, @0x44 type 202/0x47 type 200 (floorf)
```

Relocation-type census of the whole archive (all 13 members):
`208 × 18953`, `209 × 1131`, `202 × 828`, `200 × 828`, `210 × 17` —
**types 201/203 never appear**, so "does lld handle them" is vacuous for this archive.

Closed dependency closure of `tanhf`: `tanhf → expf → floorf`. `mlibm.a` defines *none* of the
compiler-runtime helpers that other math functions need: `__fixsfdi` is referenced by
`llrintf/llroundf/lrintf/lroundf`, `strtof` by `nanf`, `__truncdfsf2` by `nexttowardf` — the compat
block exists precisely to supply them from the kernel TU.

### 3.1 No byte-exact vendor oracle exists for these bytes

`/tmp/prebuilt3720/*.elf` (375 vendor-linked kernels, moviLLD v5.0.0) and
`/tmp/activation_tanh.3720xx.elf` were linked against a **different** mlibm build: searching every
prebuilt `.text` for ≥100-byte literal runs of our archive's `tanhf`/`expf` sections gives **0 hits**
(`tanhf/expf hits in 375 prebuilt ELFs: 0`). Consequence: relocation fixups are verified against the
formulas documented in `lld-sparc/RELOC_NOTES.md` (§4), not against a vendor-linked byte oracle.

## 4. Link half — WORKS (verified)

Because the compiler cannot emit a call, I isolated the linker with a hand-written 3-instruction
probe that only materialises the symbol address (`LDIL`/`LDIH` → types 200/202), i.e. the minimum
object that forces `tanhf` out of the archive:

```sh
$ cat src/tanhf_linkprobe_safe.s
        .text
        .section        .text.controlled_act,"ax",@progbits
        .globl  controlled_act
controlled_act:
        LSU0.LDIL i17, tanhf
        LSU0.LDIH i17, tanhf
        BRU.JMP i30
        .size   controlled_act, .-controlled_act

$ TC=…/shave-toolchain-root
$ $TC/bin/moviAsm --cv 3720xx --noSPrefixing < src/tanhf_linkprobe_safe.s > out/tanhf_linkprobe_safe.o
legalize: 6 delay-slot NOP(s)
$ $TC/bin/moviLLD -flavor gnu -EL -e controlled_act -z max-page-size=0x10 --gc-sections \
      $TC/lib/mlibm.a -T …/npunlock/src/shavecc/shave_kernel.ld \
      out/tanhf_linkprobe_safe.o -o - > out/tanhf_linkprobe_safe.elf
link rc=0                          # no diagnostics at all
```

(This is exactly the driver's link argv: `mlibm.a -T script object -o -`, with the archive *before*
the object — the order `shavecc.c:519-525` produces.)

### 4.0 The archive really is passed — captured from the real driver

`out/mocktc_probe.py` drives npunlock's own `NativeLibraries.compile_shave` against a *mock* toolchain
directory (three shell stubs that log their argv and print a dummy output). Whatever the kernel
contents, the stages the driver runs are:

```
ARGV[.../bin/moviCompile]: -cc1 -triple shave -target-cpu 3720xx -S -O2 -x c entry.c
                           -ffunction-sections -fdata-sections -o -
ARGV[.../bin/moviAsm]: --cv 3720xx --noSPrefixing
ARGV[.../bin/moviLLD]: -flavor gnu -EL -e controlled_act -z max-page-size=0x10 --gc-sections
                       <toolchain-dir>/lib/mlibm.a -T /tmp/npunlock-script-XXXX <object> -o -
```

So: the **kernel-TU source is expanded for `#include <npunlock/npu3720_kernel.h>` by the driver**
(`expand_npu3720_kernel_include`), and **the driver (not the `moviLLD` shell wrapper) appends
`<toolchain-root>/lib/mlibm.a` when that file is readable** — the wrapper only rewrites `-o -` to a temp
file. `kernel_build.sh` reproduces this argv stage by stage for inspection.

With the *real* toolchain the same probe link gives:

```
$ python3 validate_elf.py out/tanhf_linkprobe_safe.elf
VALID  status=0 (ok) size=640 (0x280) address=0x1d000000 entry=0x1d000000
$ nm out/tanhf_linkprobe_safe.elf
1d000000 T controlled_act      1d000020 T floorf/floor   1d000070 T tanhf/tanh   1d000140 T expf/exp
$ readelf -r out/tanhf_linkprobe_safe.elf      # → "There are no relocations in this file."
$ readelf -S  …                                # .arg.data size 0x0 at 0x1e000000, .text 0x280, no .rela.*
```

* link succeeds (`rc=0`), **no unresolved symbols**, **no leftover relocation sections**
  (`readelf -r` → *no relocations*, and `readelf -S` lists no `.rela.*` at all);
* side note (fidelity, not a blocker): our link line has **no `--strip-debug`**, while the vendor
  `npu_compiler` passes it — so every image we link with `mlibm.a` also emits the archive's DWARF
  (53 KB of `.debug_*` here, from 2136 type-208 fixups). The ACT image itself (`.text`, `.arg.data`)
  is unaffected;
* `npunlock_parse_shave_elf` accepts it (ELF32/EM_SPARC, entry == `.text` start, `.arg.data` empty);
  note the ACT contract is satisfied even though the driver puts `mlibm.a` **before** the kernel
  object on the link line: lld only extracts archive members while resolving the kernel object's
  symbols, so the kernel's own `.text.controlled_act` still lands first at `0x1d000000`;
* the link **resolves `tanhf` from `lib/mlibm.a` and pulls its closure** `expf` + `floorf`.

### 4.1 `.text` sizes

| image | `.text` |
|---|---|
| bare entry stub (no symbol reference) | 0x10 (16 B) |
| `tanhf` closure as *code*: `floorf` 0x44 + `tanhf` 0xc9 + `expf` 0x136 | **0x243 = 579 B** |
| same closure after 16-B section alignment (probe image, closure portion) | 0x260 = 608 B |
| full valid probe ELF (32-B entry stub + closure, aligned) | **0x280 = 640 B** |
| 50-function fan-out image (see §6.2) | 0x3e20 = 15904 B |

## 5. Relocation fixups — verified numerically exact

`check_relocs.py` independently re-implements the formulas from `lld-sparc/RELOC_NOTES.md`
(200 `field |= (S+A)&0xffff`, 202 `field |= ((S+A)>>16)`, 209 `field += S+A-P` mod 2^16,
210 bits[19:4] `+= S+A-P`, 208 `LE32 = S+A`), locates each pulled-in input section inside the output
`.text` (via literal runs), and compares with the bytes our lld wrote. `check_debug_relocs.py`
does the same for the *non-alloc* debug sections using the `lld -Map` layout.

```
tanhf probe image   : 200: 2 ok | 202: 2 ok | 209: 2 ok
50-function image   : 200: 20 ok | 202: 20 ok | 209: 111 ok | 210: 1 ok
                     208 (debug, lld -Map based): math_lite.o 2061 ok | shave_fenv.o 75 ok | 0 bad
```
**0 mismatches in every run.** 211 type-210 fixups exist in the whole archive; the 1 that the fan-out
image pulled in is also formula-exact. Types 201/203 are absent from the archive (nothing to apply).

Worked example — the two absolute-address sites inside the pulled-in `tanhf` closure
(`out/tanhf_linkprobe_safe.elf`, `tanhf` at `0x1d000070`, `expf` at `0x1d000140`, `floorf` at `0x1d000020`):

```
tanhf+0x8d : pre-link 00 00 f2 00 00   →   post-link 00 1d f2 40 01
             hi16 LE @0x8d = 0x1d00, opcode byte @0x8f = 0xf2 (untouched), lo16 LE @0x90 = 0x0140
             reconstructed absolute = 0x1d000140 = expf        ✓
expf +0x44 : hi16 = 0x1d00, opcode byte 0xf2 untouched, lo16 = 0x0020
             reconstructed absolute = 0x1d000020 = floorf       ✓
```

So the vendor's 3-bytes-apart `[hi16][opcode 0xf2][lo16]` call/address construct is filled in exactly,
with the opcode byte preserved. (Note: this construct is *not* what our assembler emits for
`LDIL`/`LDIH sym` — ours places the two relocations 4 bytes apart, `r_offset`s 2 and 6 — so even
though the *linker* handles both shapes, the *encoder* still has to reproduce the vendor call form
before a compiler-generated call can be expected to execute.)

## 6. Per-symbol resolvability of `mlibm.a`

`probe_symbols.py` builds one probe per symbol, links it with the real script + archive (+ the
compat helper object), and records the resulting sizes and the npunlock verdict. Results:
`out/symprobe*.json`, `out/SYMBOL_TABLE.tsv`, full rendered table `out/SWEEP_SECTION.md`,
per-symbol ELFs in `out/symprobe/`.

### 6.1 The f32 elementary set — every symbol links, 46/50 `VALID` as-is

| symbol | linked `.text` B | `.arg.data` B |
|---|---:|---:|
| `sinf` | 432 | 0 |
| `cosf` | 432 | 0 |
| `tanf` | 400 | 0 |
| `expf` | 432 | 0 |
| `exp2f` | 224 | 0 |
| `expm1f` | 624 | 0 |
| `logf` | 336 | 0 |
| `log2f` | 528 | 0 |
| `log10f` | 576 | 0 |
| `log1pf` | 1072 | 0 |
| `sqrtf` | 368 | 0 |
| `cbrtf` | 272 | 0 |
| `tanhf` | 640 | 0 |
| `atanhf` | 608 | 0 |
| `sinhf` | 1056 | 0 |
| `coshf` | 576 | 0 |
| `asinhf` | 2000 | 0 |
| `acoshf` | 1008 | 0 |
| `asinf` | 816 | 0 |
| `acosf` | 768 | 0 |
| `atanf` | 352 | 0 |
| `atan2f` | 560 | 0 |
| `powf` | 1552 | 0 |
| `fmodf` | 416 | 0 |
| `hypotf` | 688 | 0 |
| `ldexpf` | 208 | 0 |
| `frexpf` | 144 | 0 |
| `fmaxf` | 80 | 0 |
| `fminf` | 80 | 0 |
| `ceilf` | 96 | 0 |
| `floorf` | 112 | 0 |
| `truncf` | 80 | 0 |
| `roundf` | 112 | 0 |
| `nearbyintf` | 544 | 4 |
| `copysignf` | 48 | 0 |
| `fabsf` | 48 | 0 |
| `ilogbf` | 96 | 0 |
| `logbf` | 96 | 0 |
| `fdimf` | 128 | 0 |
| `fmaf` | 1600 | 0 |
| `scalbnf` | 208 | 0 |
| `modff` | 96 | 0 |
| `remquof` | 672 | 0 |
| `remainderf` | 576 | 0 |
| `lrintf` | 592 | 4 |
| `llrintf` | 912 | 4 |
| `rintf` | 544 | 4 |
| `lroundf` | 160 | 0 |
| `llroundf` | 480 | 0 |
| `scalblnf` | 208 | 0 |

(`.text` = the 32-B probe stub + that symbol's own closure out of `mlibm.a`. All 50 link with
`rc=0`; every one yields a `VALID` ELF except the four marked with a non-zero `.arg.data`.)

### 6.2 Fan-out: 50 functions in one image

`src/manylibm_A.s` roots all 50 symbols in one kernel:

* first attempt without helper stubs → `ld.lld: error: undefined symbol: __fixsfdi` (referenced by
  `llrintf`, `llroundf`, …) — the *only* unresolved symbol for the whole f32 set, and precisely the
  helper the header's compat block defines;
* after adding `out/compat_helpers.o` (that block, compiled by our clang at 12:58):
  `link rc=0`, `.text = 0x3e20` (15904 B), 104 symbols defined; §5 verified its fixups
  (20×200, 20×202, 111×209, 1×210, 2061×208).
* this image is one of the 25 `unsupported` cases below: `nearbyintf/rintf/llrintf/lrintf` are in the
  set, so the 4-byte `roundMode` global lands in `.arg.data`.

### 6.3 Full public-symbol sweep — 204 symbols from the `MLIBM_SYMBOLS.md` inventory

`out/SWEEP_SECTION.md` + `out/SYMBOL_TABLE.tsv` contain the per-symbol table. Summary:

```
symbols probed: 204   linked: 169   link failures: 35
of the 169 linked: 144 produce a contract-valid image (.arg.data == 0)
                    25 are REJECTED by npunlock: "SHAVE .arg.data must be empty at 0x1e000000"
families            total  linked  arg.data!=0  link-fail
  float (f suffix)     65      59            4          6
  double               77      71            8          6
  long double (l)      62      39           13         23
```

**The 25 contract-violating images** (non-empty `.arg.data`, i.e. the function drags a *writable*
global — the linker script routes `*(.data*)` into `.arg.data`):
`acoshl asinhl atanhl cosl exp_unsafe62 expl expm1l fegetround fesetround lanczos_ln_gamma62
llrint llrintf log10l log2l logl lrint lrintf nearbyint nearbyintf rint rintf sinhl sinl tanhl tanl`
— the `rint/nearbyint/lrint/llrint` family pulls `roundMode`/`__fe_dfl_env` (4 B) out of
`shave_fenv.o`; the long-double set pulls the fp64 coefficient tables (hundreds of bytes, e.g.
`acoshl` 400 B, `asinhl` 464 B). The pure f32/f64 elementary functions are all clean.

**The 35 link failures** are all *missing external runtime* symbols, i.e. they are not resolvable
from `mlibm.a` alone (they would need `mlibc` / a compiler-rt object on the link line):

```
__errno (12)      j0 j0f j1 j1f jn jnf y0 y0f y1 y1f yn ynf          (Bessel family, mlibc)
soft-float df (23) acosl asinl atan2l atanl coshl erfcl erfl exp2l fdiml fmal lgammal
                   llrintl llroundl log1pl logbl lrintl lroundl nearbyintl powl remainderl
                   remquol rintl tgammal                                (long double, compiler-rt)
                   → __adddf3 __subdf3 __muldf3 __floatsidf __fixdfdi __fixdfsi
                     __ltdf2 __gedf2 __gtdf2 __nedf2 __eqdf2
```

Cross-check against `npunlock/ABI/MLIBM_SYMBOLS.md`: the inventory lists **334** defined symbols
(309 `T`, 25 `D`). This sweep covers the **204 public C-named** ones (everything a kernel author would
spell, i.e. all non-`__` names). The remaining 130 are `__`-prefixed internals (`__exps`, `__poly_evaluationl`,
the `D` coefficient tables, …) that a kernel would only reference indirectly — and when it does, they
are pulled in automatically by the same archive extraction that §5 verified.

Practical answer to "which symbols can our toolchain resolve?":

* **yes, fully usable**: every elementary `f32` and `f64` function — `sinf/cosf/tanf/expf/logf/…/powf/fmodf/fmaf/hypotf/erff/tgammaf/lgammaf` and their double twins — with an empty `.arg.data` and a `VALID` ELF;
* **yes, but the image fails the ACT contract** unless the data dependency is removed: the `rint`/`nearbyint`/`lrint`/`llrint` family (4 B `roundMode`) and the long-double set (fp64 tables);
* **no**: Bessel `j*`/`y*` (need `__errno`) and 23 long-double functions (need soft-float `__*df*` helpers) — `mlibm.a` simply does not contain those symbols;
* **helper runtime needed regardless**: `__fixsfdi` (`llrintf/llroundf/lrintf/lroundf`),
  `strtof` (`nanf`), `__truncdfsf2` (`nexttowardf`) — supplied by the kernel side, i.e. exactly the
  `MLIBM_DEFINE_LINK_COMPAT` block that currently fails to compile (§2.2).


## 7. Runtime check on the real MTL NPU

`out/tanhf_linkprobe_safe.elf` (§4) — a *valid* kernel image whose `.text` contains the resolved
vendor `mlibm` code and which returns immediately (no store, so the add1 oracle cannot be satisfied;
the point is load/execute status, not the oracle):

```sh
$ cd …/npunlock
$ NPUNLOCK_SMOKE_WORK_DIR=…/mathlink/out/npu-smoke timeout 240 \
    ./tools/npu_add1_smoke.sh --elf …/mathlink/out/tanhf_linkprobe_safe.elf
… "stages": {"loader":"passed","device_selection":"passed","graph_extension":"passed",
             "graph_create":"passed","initialization":"passed","execution":"passed",
             "oracle":"failed"} …
SMOKE_EXIT=1
```

* the ELF is accepted by `npunlock-patchblob`, `porting_probe.py create/verify` and executes on the
  NPU without fault or hang (`execution":"passed"`, `level_zero_result 0x0`);
* `oracle` fails as designed: `dump_probe` shows `ACTUAL` = carrier-init bytes, not `INPUT`/`EXPECT`
  (the probe performs no store);
* this exact run was repeated after the final tool rebuild (`out/npu-smoke-final/execution-report.json`,
  13:13) with the identical result;
* **control**: the gold fixture passes right now —
  `npu_add1_smoke.sh --elf tests/fixtures/npu3720/add1-fp16.elf` → `PASS … oracle exact`, exit 0 —
  so the harness/NPU were healthy for these runs.

Direct consequence: **no runtime evidence for or against the *math* semantics could be produced**,
because the compiler cannot build a kernel that actually calls `tanhf`. The runtime result only shows
that a formally valid image containing vendor math code is loadable and executable by the ACT carrier.

### 7.1 Control — is that oracle failure specific to the math image? No.

`src/add1_fp16_header.c` is the *gold fixture's own semantics* (fp16 in/out, `out[i] = in[i] + 1.0f`)
written through the ABI header instead (it needs no mlibm and no compat block). It compiles,
assembles and links cleanly into a 432-byte `VALID` ELF:

```sh
$ ./kernel_build.sh src/add1_fp16_header.c add1_fp16_header
compile rc=0 / assemble rc=0 / link rc=0
VALID  size=432 (0x1b0) address=0x1d000000 entry=0x1d000000
$ NPUNLOCK_SMOKE_WORK_DIR=… ./tools/npu_add1_smoke.sh --elf out/add1_fp16_header.elf
stages: … "execution":"passed","oracle":"failed"            EXIT=1
dump_probe ACTUAL = 00 00 00 00 ff ff ff 00 …  (unrelated carrier bytes, not INPUT, not EXPECT)
```

So a *compiler-produced* kernel with gold's exact semantics also writes nothing today: the oracle
failure of the math probe is the project's known "our kernels do not land stores yet" scheduling
blocker (peer-owned), **not** anything introduced by linking `mlibm.a`.

## 8. Blockers for the math path (ranked)

1. **`LowerCall` unimplemented** (`SHAVEISelLowering.cpp:823`) — unconditional fatal. Blocks every
   kernel that calls any library/helper function, including all of `mlibm.a`. *(llvm-14 owner.)*
2. **Signed / FP compares unimplemented** (`SHAVEISelLowering.cpp:~1002`, unsigned-only setcc) — the
   repo's `MLIBM_DEFINE_LINK_COMPAT` helper block contains `exponent < 0`, which the *current* clang
   build no longer folds away ⇒ even the helper TU fails to compile. Also blocks ordinary user math
   code (`if (x > 0)`) and is the reason `fcmp` is listed as unsupported in TARGET_NOTES. *(llvm-14 owner.)*
3. **Return convention still provisional** — `TARGET_NOTES.md` lists `ret` = `BRU.JMP i30` as
   PROVISIONAL while `i30` is also the return-*value* register; call/return is the one ABI area that
   must be settled together with (1). *(llvm-14 owner.)*
3b. **Vendor call/address encoding not reproducible by our assembler** — our `LDIL`/`LDIH sym`
   emits the two relocation sites 4 bytes apart (`r_offset`s 2 and 6), while the vendor's
   absolute-address construct places them 3 bytes apart around an untouched opcode byte `0xf2`
   (measured in `tanhf`/`expf`, §5). Our *linker* fills both shapes correctly, but a compiler-emitted
   call must use the vendor's shape to execute; whoever implements (1) needs that form from
   `shave-isa` (`AsmEncodingRE`) or direct emission in the backend. *(shave-isa + llvm-14 owners.)*
4. **Helper runtime** — `mlibm.a` references `__fixsfdi`/`strtof`/`__truncdfsf2`, which the archive
   does not define; they must come from the kernel TU (compat block) or a supplied runtime object.
   Link-wise this is solved (proved in §6.2) but it requires (2) to be fixed, or the helper source to
   avoid signed compares.
5. **Data dependency** — `llrintf/lrintf/nearbyintf/rintf` pull the 4-byte `roundMode` global into
   `.arg.data`, which the ACT contract rejects (`.arg.data` must be empty). Not fixable in the linker
   script without a policy decision (see LIMITATIONS.md).
6. **Environment** — `npu.compile()`'s IR→blob stage needs the Level Zero graph-compiler worker, so
   the GELU examples cannot even reach `shavecc` on this machine (§2.3).

## 9. Files in this directory

```
MATHLINK_REPORT.md   this report                    REPORT.md          pointer to this report
validate_elf.py      npunlock_parse_shave_elf via ctypes
expand_header.py     reproduction of shavecc's npu3720_kernel.h expansion
kernel_build.sh      stage-by-stage MOVI build with the driver's exact argv
gen_linkprobe.py     linker-probe .s generator
probe_symbols.py     per-symbol link/validate sweep (out/symprobe*.json)
check_relocs.py      independent verification of types 200/202/209/210
check_debug_relocs.py independent verification of type 208 (lld -Map based)
cmpsections.py       ELF32 section byte extractor
compile_via_npunlock.py  compile through npunlock's own NativeLibraries.compile_shave
out/mocktc_probe.py  capture the driver's real stage argv with a mock toolchain root
make_symbol_table.py / make_sweep_section.py   fold sweep JSON into the report artefacts

src/  tanhf_min.c tanhf_min_decl.c tanhf_min_nocompat.c tanhf_gelu_f32.c expf_min.c
      expf_min_decl.c compat_only.c compat_off.c compat_helpers.c nocall_twin.c
      nocall_twin_nocompat.c add1_fp16_header.c
      tanhf_linkprobe.s tanhf_linkprobe_safe.s manylibm_A.s example_gelu_f32.py
out/  *.elf *.o *.s, SYMBOL_TABLE.tsv, SWEEP_SECTION.md, symprobe/ (per-symbol ELFs),
      symprobe*.json, npu-smoke*/ and npu-smoke-add1hdr/ (probe bundles + reports)
logs/ per-stage stdout/stderr, link maps
```

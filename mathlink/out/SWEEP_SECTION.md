## 6.3 Full public-symbol sweep (results: `out/SYMBOL_TABLE.tsv`)

* symbols probed: **204**; linked: **169**; link/assemble failures: **35**
* npunlock verdict `VALID`: **144/169**; not valid: [('acoshl', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('asinhl', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('atanhl', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('cosl', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('exp_unsafe62', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('expl', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('expm1l', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('fegetround', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('fesetround', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('lanczos_ln_gamma62', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('llrint', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('llrintf', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('log10l', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('log2l', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('logl', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('lrint', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('lrintf', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('nearbyint', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('nearbyintf', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('rint', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('rintf', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('sinhl', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('sinl', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('tanhl', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg'), ('tanl', 'INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg')]
* non-empty `.arg.data` (ACT contract violation): `acoshl` (400 B), `asinhl` (464 B), `atanhl` (320 B), `cosl` (224 B), `exp_unsafe62` (192 B), `expl` (192 B), `expm1l` (528 B), `fegetround` (4 B), `fesetround` (4 B), `lanczos_ln_gamma62` (672 B), `llrint` (4 B), `llrintf` (4 B), `log10l` (320 B), `log2l` (320 B), `logl` (320 B), `lrint` (4 B), `lrintf` (4 B), `nearbyint` (4 B), `nearbyintf` (4 B), `rint` (4 B), `rintf` (4 B), `sinhl` (240 B), `sinl` (224 B), `tanhl` (256 B), `tanl` (224 B)
* failures: `acosl`: ld.lld: error: undefined symbol: __ltdf2
>>> referenced by C:\\j_ws\\mCompile_main\\Compil; `asinl`: ld.lld: error: undefined symbol: __ltdf2
>>> referenced by shave_fp64.c:1985 (C:\\j_ws\\mC; `atan2l`: ld.lld: error: undefined symbol: __nedf2
>>> referenced by shave_fp64.c:1764 (C:\\j_ws\\mC; `atanl`: ld.lld: error: undefined symbol: __gedf2
>>> referenced by shave_fp64.c:1717 (C:\\j_ws\\mC; `coshl`: ld.lld: error: undefined symbol: __gtdf2
>>> referenced by shave_fp64.c:2118 (C:\\j_ws\\mC; `erfcl`: ld.lld: error: undefined symbol: __adddf3
>>> referenced by shave_fp64.c:3676 (C:\\j_ws\\m; `erfl`: ld.lld: error: undefined symbol: __adddf3
>>> referenced by shave_fp64.c:3676 (C:\\j_ws\\m; `exp2l`: ld.lld: error: undefined symbol: __subdf3
>>> referenced by C:\\j_ws\\mCompile_main\\Compi; `fdiml`: ld.lld: error: undefined symbol: __eqdf2
>>> referenced by shave_fp64.c:782 (C:\\j_ws\\mCo; `fmal`: ld.lld: error: undefined symbol: __muldf3
>>> referenced by shave_fp64.c:4271 (C:\\j_ws\\m; `j0`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_j0.c:37 (C:/j_ws/mCompile_ma; `j0f`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_j0.c:37 (C:/j_ws/mCompile_ma; `j1`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_j1.c:39 (C:/j_ws/mCompile_ma; `j1f`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_j1.c:39 (C:/j_ws/mCompile_ma; `jn`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_jn.c:35 (C:/j_ws/mCompile_ma; `jnf`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_jn.c:35 (C:/j_ws/mCompile_ma; `lgammal`: ld.lld: error: undefined symbol: __eqdf2
>>> referenced by C:\\j_ws\\mCompile_main\\Compil; `llrintl`: ld.lld: error: undefined symbol: __fixdfdi
>>> referenced by C:\\j_ws\\mCompile_main\\Comp; `llroundl`: ld.lld: error: undefined symbol: __fixdfdi
>>> referenced by C:\\j_ws\\mCompile_main\\Comp; `log1pl`: ld.lld: error: undefined symbol: __eqdf2
>>> referenced by shave_fp64.c:1888 (C:\\j_ws\\mC; `logbl`: ld.lld: error: undefined symbol: __floatsidf
>>> referenced by shave_fp64.c:863 (C:\\j_ws\; `lrintl`: ld.lld: error: undefined symbol: __fixdfsi
>>> referenced by C:\\j_ws\\mCompile_main\\Comp; `lroundl`: ld.lld: error: undefined symbol: __fixdfsi
>>> referenced by C:\\j_ws\\mCompile_main\\Comp; `nearbyintl`: ld.lld: error: undefined symbol: __subdf3
>>> referenced by shave_fp64.c:2910 (C:\\j_ws\\m; `powl`: ld.lld: error: undefined symbol: __fixdfdi
>>> referenced by C:\\j_ws\\mCompile_main\\Comp; `remainderl`: ld.lld: error: undefined symbol: __adddf3
>>> referenced by shave_fp64.c:3060 (C:\\j_ws\\m; `remquol`: ld.lld: error: undefined symbol: __adddf3
>>> referenced by shave_fp64.c:3060 (C:\\j_ws\\m; `rintl`: ld.lld: error: undefined symbol: __subdf3
>>> referenced by shave_fp64.c:2910 (C:\\j_ws\\m; `tgammal`: ld.lld: error: undefined symbol: __fixdfsi
>>> referenced by shave_fp64.c:2678 (C:\\j_ws\\; `y0`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_j0.c:58 (C:/j_ws/mCompile_ma; `y0f`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_j0.c:58 (C:/j_ws/mCompile_ma; `y1`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_j1.c:61 (C:/j_ws/mCompile_ma; `y1f`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_j1.c:61 (C:/j_ws/mCompile_ma; `yn`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_jn.c:56 (C:/j_ws/mCompile_ma; `ynf`: ld.lld: error: undefined symbol: __errno
>>> referenced by wf_jn.c:56 (C:/j_ws/mCompile_ma

Every probed symbol resolves out of `mlibm.a`, links, and yields an ELF that `npunlock_parse_shave_elf` accepts — except the `.arg.data` group above.

Symbols in `ABI/MLIBM_SYMBOLS.md` not in this sweep (130): `__acoshs`, `__acoss`, `__add_unsafel`, `__asinhs`, `__asins`, `__atan2s`, `__atanhs`, `__atans`, `__cbrts`, `__ceils`, `__clampl`, `__comparelt`, `__comparelte`, `__copysigns`, `__coshs`, `__cospi_reduced`, `__coss`, `__double_to_float_unsafel`, `__erfcs`, `__erfs`, `__exact_sqrtl`, `__exp2s`, `__expm1_unsafe62`, `__expm1s`, `__exps`, `__fdims`, `__fe_dfl_env`, `__float_to__fp16_denormal_support`, `__float_to_double_unsafe`, `__floor_unsafel`, `__floors`, `__fmas`, `__fmaxl_logic_lookup_table`, `__fmaxs`, `__fminl_logic_lookup_table`, `__fmins`, `__fmods`, `__fp16_to_float_denormal_support`, `__fpclassifyd`, `__fpclassifyf` …

### .text size of the linked image per symbol (probe stub 32 B + closure)

| symbol | .text B | .arg.data B | ELF B | verdict |
|---|---:|---:|---:|---|
| `tanl` | 11248 | 224 | 335292 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `tanhl` | 11040 | 256 | 335068 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `lanczos_ln_gamma62` | 10704 | 672 | 335212 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `acoshl` | 10240 | 400 | 334524 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `asinhl` | 10096 | 464 | 334332 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `sinhl` | 9920 | 240 | 333932 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `atanhl` | 9632 | 320 | 333720 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `cosl` | 7552 | 224 | 331564 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `sinl` | 7408 | 224 | 331420 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `expm1l` | 7328 | 528 | 331672 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `cbrtl` | 6960 | 0 | 330620 | VALID |
| `hypotl` | 6528 | 0 | 330228 | VALID |
| `expl` | 6400 | 192 | 330392 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `sqrtl` | 5840 | 0 | 329428 | VALID |
| `exp_unsafe62` | 4992 | 192 | 328876 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `logl` | 4400 | 320 | 328428 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `log10l` | 4288 | 320 | 328296 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `lgamma` | 4128 | 0 | 56988 | VALID |
| `lgammaf` | 4128 | 0 | 56988 | VALID |
| `log2l` | 3840 | 320 | 327844 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `tgamma` | 3104 | 0 | 56040 | VALID |
| `tgammaf` | 3104 | 0 | 56040 | VALID |
| `range_reduction_payne_hanek` | 2960 | 0 | 326508 | VALID |
| `erfc` | 2816 | 0 | 55676 | VALID |
| `erfcf` | 2816 | 0 | 55676 | VALID |
| `asinh` | 2000 | 0 | 54860 | VALID |
| `asinhf` | 2000 | 0 | 54860 | VALID |
| `fmodl` | 1776 | 0 | 325372 | VALID |
| `fma` | 1600 | 0 | 54372 | VALID |
| `fmaf` | 1600 | 0 | 54372 | VALID |
| `erf` | 1552 | 0 | 54324 | VALID |
| `erff` | 1552 | 0 | 54324 | VALID |
| `pow` | 1552 | 0 | 54324 | VALID |
| `powf` | 1552 | 0 | 54324 | VALID |
| `scalblnl` | 1152 | 0 | 324704 | VALID |
| `scalbnl` | 1152 | 0 | 324700 | VALID |
| `ldexpl` | 1104 | 0 | 324628 | VALID |
| `log1p` | 1072 | 0 | 53888 | VALID |
| `log1pf` | 1072 | 0 | 53888 | VALID |
| `sinh` | 1056 | 0 | 53916 | VALID |
| `sinhf` | 1056 | 0 | 53916 | VALID |
| `acosh` | 1008 | 0 | 53868 | VALID |
| `acoshf` | 1008 | 0 | 53868 | VALID |
| `modfl` | 928 | 0 | 324452 | VALID |
| `llrint` | 912 | 4 | 54004 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `llrintf` | 912 | 4 | 54004 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `asin` | 816 | 0 | 53632 | VALID |
| `asinf` | 816 | 0 | 53632 | VALID |
| `acos` | 768 | 0 | 53584 | VALID |
| `acosf` | 768 | 0 | 53584 | VALID |
| `nextafterl` | 736 | 0 | 324380 | VALID |
| `nexttowardl` | 736 | 0 | 324380 | VALID |
| `hypot` | 688 | 0 | 53508 | VALID |
| `hypotf` | 688 | 0 | 53508 | VALID |
| `remquo` | 672 | 0 | 53448 | VALID |
| `remquof` | 672 | 0 | 53448 | VALID |
| `tanh` | 640 | 0 | 53500 | VALID |
| `tanhf` | 640 | 0 | 53500 | VALID |
| `expm1` | 624 | 0 | 53484 | VALID |
| `expm1f` | 624 | 0 | 53484 | VALID |
| `atanh` | 608 | 0 | 53424 | VALID |
| `atanhf` | 608 | 0 | 53424 | VALID |
| `lrint` | 592 | 4 | 53656 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `lrintf` | 592 | 4 | 53656 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `cosh` | 576 | 0 | 53436 | VALID |
| `coshf` | 576 | 0 | 53436 | VALID |
| `log10` | 576 | 0 | 53396 | VALID |
| `log10f` | 576 | 0 | 53396 | VALID |
| `remainder` | 576 | 0 | 53360 | VALID |
| `remainderf` | 576 | 0 | 53360 | VALID |
| `atan2` | 560 | 0 | 53336 | VALID |
| `atan2f` | 560 | 0 | 53336 | VALID |
| `roundl` | 560 | 0 | 324084 | VALID |
| `nearbyint` | 544 | 4 | 53560 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `nearbyintf` | 544 | 4 | 53560 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `rint` | 544 | 4 | 53560 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `rintf` | 544 | 4 | 53560 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `log2` | 528 | 0 | 53300 | VALID |
| `log2f` | 528 | 0 | 53300 | VALID |
| `floorl` | 496 | 0 | 324020 | VALID |
| `ilogbl` | 480 | 0 | 324004 | VALID |
| `llround` | 480 | 0 | 53332 | VALID |
| `llroundf` | 480 | 0 | 53332 | VALID |
| `cos` | 432 | 0 | 53204 | VALID |
| `cosf` | 432 | 0 | 53204 | VALID |
| `exp` | 432 | 0 | 53248 | VALID |
| `expf` | 432 | 0 | 53248 | VALID |
| `sin` | 432 | 0 | 53204 | VALID |
| `sinf` | 432 | 0 | 53204 | VALID |
| `fmod` | 416 | 0 | 53188 | VALID |
| `fmodf` | 416 | 0 | 53188 | VALID |
| `frexpl` | 416 | 0 | 323940 | VALID |
| `tan` | 400 | 0 | 53172 | VALID |
| `tanf` | 400 | 0 | 53172 | VALID |
| `sqrt` | 368 | 0 | 53140 | VALID |
| `sqrtf` | 368 | 0 | 53140 | VALID |
| `atan` | 352 | 0 | 53124 | VALID |
| `atanf` | 352 | 0 | 53124 | VALID |
| `log` | 336 | 0 | 53108 | VALID |
| `logf` | 336 | 0 | 53108 | VALID |
| `fminl` | 320 | 0 | 323844 | VALID |
| `truncl` | 320 | 0 | 323844 | VALID |
| `fmaxl` | 288 | 0 | 323812 | VALID |
| `nexttoward` | 288 | 0 | 53104 | VALID |
| `nexttowardf` | 288 | 0 | 53104 | VALID |
| `x_mod_2_62` | 288 | 0 | 323816 | VALID |
| `cbrt` | 272 | 0 | 53044 | VALID |
| `cbrtf` | 272 | 0 | 53044 | VALID |
| `exp2` | 224 | 0 | 52996 | VALID |
| `exp2f` | 224 | 0 | 52996 | VALID |
| `ldexp` | 208 | 0 | 53080 | VALID |
| `ldexpf` | 208 | 0 | 53080 | VALID |
| `scalbln` | 208 | 0 | 53080 | VALID |
| `scalblnf` | 208 | 0 | 53080 | VALID |
| `scalbn` | 208 | 0 | 53080 | VALID |
| `scalbnf` | 208 | 0 | 53080 | VALID |
| `ceill` | 192 | 0 | 323760 | VALID |
| `nextafter` | 192 | 0 | 52976 | VALID |
| `nextafterf` | 192 | 0 | 52976 | VALID |
| `lround` | 160 | 0 | 52984 | VALID |
| `lroundf` | 160 | 0 | 52984 | VALID |
| `frexp` | 144 | 0 | 52920 | VALID |
| `frexpf` | 144 | 0 | 52920 | VALID |
| `fdim` | 128 | 0 | 52900 | VALID |
| `fdimf` | 128 | 0 | 52900 | VALID |
| `fegetexceptflag` | 112 | 0 | 4156 | VALID |
| `fesetround` | 112 | 4 | 4224 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `floor` | 112 | 0 | 52888 | VALID |
| `floorf` | 112 | 0 | 52888 | VALID |
| `isnanl` | 112 | 0 | 323636 | VALID |
| `nan` | 112 | 0 | 52908 | VALID |
| `nanf` | 112 | 0 | 52908 | VALID |
| `round` | 112 | 0 | 52888 | VALID |
| `roundf` | 112 | 0 | 52888 | VALID |
| `ceil` | 96 | 0 | 52868 | VALID |
| `ceilf` | 96 | 0 | 52868 | VALID |
| `fesetenv` | 96 | 0 | 4132 | VALID |
| `feupdateenv` | 96 | 0 | 4136 | VALID |
| `ilogb` | 96 | 0 | 52872 | VALID |
| `ilogbf` | 96 | 0 | 52872 | VALID |
| `is_denormal_or_zero` | 96 | 0 | 323636 | VALID |
| `isinfl` | 96 | 0 | 323620 | VALID |
| `iszerol` | 96 | 0 | 323624 | VALID |
| `logb` | 96 | 0 | 52868 | VALID |
| `logbf` | 96 | 0 | 52868 | VALID |
| `modf` | 96 | 0 | 52868 | VALID |
| `modff` | 96 | 0 | 52868 | VALID |
| `copysignl` | 80 | 0 | 323608 | VALID |
| `fabsl` | 80 | 0 | 323604 | VALID |
| `fmax` | 80 | 0 | 52852 | VALID |
| `fmaxf` | 80 | 0 | 52852 | VALID |
| `fmin` | 80 | 0 | 52852 | VALID |
| `fminf` | 80 | 0 | 52852 | VALID |
| `trunc` | 80 | 0 | 52856 | VALID |
| `truncf` | 80 | 0 | 52856 | VALID |
| `x_times_half_unsafel` | 80 | 0 | 323620 | VALID |
| `feclearexcept` | 64 | 0 | 4108 | VALID |
| `fegetenv` | 64 | 0 | 4100 | VALID |
| `fegetround` | 64 | 4 | 4176 | INVALID: {"status":"unsupported","stage":"elf32.validate","message":"SHAVE .arg |
| `feraiseexcept` | 64 | 0 | 4108 | VALID |
| `fesetexceptflag` | 64 | 0 | 4108 | VALID |
| `is_nan_or_infl` | 64 | 0 | 323596 | VALID |
| `copysign` | 48 | 0 | 52828 | VALID |
| `copysignf` | 48 | 0 | 52828 | VALID |
| `fabs` | 48 | 0 | 52820 | VALID |
| `fabsf` | 48 | 0 | 52820 | VALID |
| `feholdexcept` | 48 | 0 | 4088 | VALID |
| `fetestexcept` | 48 | 0 | 4088 | VALID |
| `nanl` | 48 | 0 | 323572 | VALID |

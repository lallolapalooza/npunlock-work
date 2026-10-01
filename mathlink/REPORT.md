# REPORT.md → see MATHLINK_REPORT.md

The math-library (`mlibm.a`) link-path verification report lives in
[`MATHLINK_REPORT.md`](MATHLINK_REPORT.md) (scripts in `src/`, artifacts in `out/`, stage
transcripts in `logs/`).

Headline: **the link half works** (vendor `tanhf`/`expf`/`floorf` resolve out of
`shave-toolchain-root/lib/mlibm.a` into an ELF that `npunlock_parse_shave_elf` accepts, with all
200/202/208/209/210 fixups verified formula-exact), but **the compile half is blocked**: the SHAVE
backend has no call lowering (`LowerCall` aborts), and the repo's own `MLIBM_DEFINE_LINK_COMPAT`
helper block additionally hits the unimplemented signed-compare path on the current clang build.

#!/bin/sh
# Manual MOVI-stage build of one kernel source, mirroring the argv the npunlock
# shavecc driver uses (src/shavecc/shavecc.c) but driven stage-by-stage so each
# stage's stdout/stderr is captured separately.
#
# usage: kernel_build.sh SRC.c NAME
#   out/NAME.expanded.c  out/NAME.s  out/NAME.o  out/NAME.elf
set -u
SRC="$1"; NAME="$2"
HERE=$(dirname "$0")
TC=/home/asdf/projects/npunlock-work/shave-toolchain-root
LD=/home/asdf/projects/npunlock-work/npunlock/src/shavecc/shave_kernel.ld
mkdir -p "$HERE/out" "$HERE/logs"

python3 "$HERE/expand_header.py" "$SRC" > "$HERE/out/$NAME.expanded.c" || exit 1

# stage 1: compile (shavecc compile_base + compile_tail)
"$TC/bin/moviCompile" -cc1 -triple shave -target-cpu 3720xx -S -O2 -x c entry.c \
    -ffunction-sections -fdata-sections -o - \
    < "$HERE/out/$NAME.expanded.c" > "$HERE/out/$NAME.s" 2> "$HERE/logs/$NAME.compile.err"
rc=$?
echo "compile rc=$rc"; sed -n '1,20p' "$HERE/logs/$NAME.compile.err"
[ $rc -ne 0 ] && exit $rc

# stage 2: assemble (shavecc assemble_args)
"$TC/bin/moviAsm" --cv 3720xx --noSPrefixing \
    < "$HERE/out/$NAME.s" > "$HERE/out/$NAME.o" 2> "$HERE/logs/$NAME.asm.err"
rc=$?
echo "assemble rc=$rc"; sed -n '1,10p' "$HERE/logs/$NAME.asm.err"
[ $rc -ne 0 ] && exit $rc

# stage 3: link (shavecc link_base + mlibm.a + -T script + object -o -)
"$TC/bin/moviLLD" -flavor gnu -EL -e controlled_act -z max-page-size=0x10 --gc-sections \
    "$TC/lib/mlibm.a" -T "$LD" "$HERE/out/$NAME.o" "$HERE/out/compat_helpers.o" -o - \
    > "$HERE/out/$NAME.elf" 2> "$HERE/logs/$NAME.link.err"
rc=$?
echo "link rc=$rc"; sed -n '1,20p' "$HERE/logs/$NAME.link.err"
exit $rc

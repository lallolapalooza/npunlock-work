#!/bin/sh
# build_kernel.sh SRC.c OUT.elf  -- compile one ACT kernel with the LIVE toolchain.
set -e
ROOT=/home/asdf/projects/npunlock-work
TC=$ROOT/shave-toolchain-root
LD=$ROOT/npunlock/src/shavecc/shave_kernel.ld
SRC=$1; OUT=$2; WORK=$(dirname "$OUT"); STEM=$(basename "$SRC" .c)
mkdir -p "$WORK"
"$TC/bin/moviCompile" -cc1 -triple shave -target-cpu 3720xx -S -O2 -x c entry.c \
    -ffunction-sections -fdata-sections -o - -I "$(dirname "$SRC")" < "$SRC" > "$WORK/$STEM.s"
# NOTE: the live toolchain emits the register form (and the "; fmt=2" hint) itself;
# no post-fixup is applied.
"$TC/bin/moviAsm" --cv 3720xx --noSPrefixing < "$WORK/$STEM.s" > "$WORK/$STEM.o"
rm -f "$OUT"
"$TC/bin/moviLLD" -flavor gnu -EL -e controlled_act -z max-page-size=0x10 --gc-sections \
    -T "$LD" "$WORK/$STEM.o" -o "$OUT"
echo "built $OUT"

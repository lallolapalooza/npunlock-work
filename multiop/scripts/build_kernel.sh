#!/usr/bin/env bash
# Build one C kernel with the native SHAVE toolchain (corpus pipeline, no --gc-sections
# so every function in the file survives into .text).
# usage: build_kernel.sh SRC.c OUT.elf [ENTRY]
set -euo pipefail
SRC="$1"; OUT="$2"; ENTRY="${3:-controlled_act}"
ROOT=/home/asdf/projects/npunlock-work
TC="$ROOT/shave-toolchain-root"
LDSCRIPT="$ROOT/npunlock/src/shavecc/shave_kernel.ld"
INC="$ROOT/npunlock/include"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
"$TC/bin/moviCompile" -cc1 -triple shave -target-cpu 3720xx -S -O2 \
  -x c "$SRC" -ffunction-sections -fdata-sections -I"$INC" -o "$TMP/k.s"
"$TC/bin/moviAsm" --cv 3720xx --noSPrefixing < "$TMP/k.s" > "$TMP/k.o"
"$TC/bin/moviLLD" -flavor gnu -EL -e "$ENTRY" -z max-page-size=0x10 \
  -T "$LDSCRIPT" "$TMP/k.o" -o "$OUT"
echo "built $OUT ($(stat -c%s "$OUT") bytes)"

#!/usr/bin/env bash
# Pin the *current* live toolchain into corpus/toolchain-snapshot so corpus runs
# are reproducible while siblings rebuild /home/asdf/projects/llvm-14 and edit
# shave-isa.  Reads (never writes) every foreign tree.
#
# usage: tools/make_snapshot.sh
set -euo pipefail

CORPUS="$(cd "$(dirname "$0")/.." && pwd)"
LIVE=/home/asdf/projects/llvm-14
ISA=/home/asdf/projects/npunlock-work/shave-isa
LIVE_TC=/home/asdf/projects/npunlock-work/shave-toolchain-root
SNAP="${SNAP_DIR:-$CORPUS/toolchain-snapshot}"

rm -rf "$SNAP"
mkdir -p "$SNAP/bin" "$SNAP/lib/clang" "$SNAP/isa"

CLANG_REAL=$(readlink -f "$LIVE/build/bin/clang")
cp "$CLANG_REAL" "$SNAP/bin/clang-14"
ln -sf clang-14 "$SNAP/bin/clang"
cp "$(readlink -f "$LIVE/build/bin/ld.lld")" "$SNAP/bin/ld.lld"
cp "$ISA/shave_asm.py" "$ISA/encodings.json" "$SNAP/isa/"
cp "$LIVE_TC/lib/mlibm.a" "$SNAP/lib/"

# clang needs its builtin-header resource directory alongside the binary
RESDIR=$(cd "$LIVE/build/lib/clang" && ls -d */ | head -1)
RESDIR="${RESDIR%/}"
ln -sfn "$(cd "$LIVE/build/lib/clang/$RESDIR" && pwd)" "$SNAP/lib/clang/$RESDIR"

cat > "$SNAP/bin/moviCompile" <<'EOF'
#!/bin/sh
SNAP=$(cd "$(dirname "$0")/.." && pwd)
for arg do
  shift
  [ "$arg" = "entry.c" ] && arg="-"
  set -- "$@" "$arg"
done
exec "$SNAP/bin/clang" "$@" -main-file-name entry.c
EOF

cat > "$SNAP/bin/moviAsm" <<'EOF'
#!/bin/sh
SNAP=$(cd "$(dirname "$0")/.." && pwd)
exec python3 "$SNAP/isa/shave_asm.py" obj "$@"
EOF

cat > "$SNAP/bin/moviLLD" <<'EOF'
#!/bin/sh
SNAP=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp "${TMPDIR:-/tmp}/moviLLD-XXXXXX") || exit 1
trap 'rm -f "$TMP"' 0
trap 'exit 2' 1 2 3 15
prev=""
for arg do
  shift
  if [ "$arg" = "-" ] && [ "$prev" = "-o" ]; then arg="$TMP"; fi
  set -- "$@" "$arg"
  prev="$arg"
done
"$SNAP/bin/ld.lld" "$@"
status=$?
if [ "$status" -eq 0 ]; then cat "$TMP" || status=$?; fi
exit "$status"
EOF
chmod +x "$SNAP/bin/moviCompile" "$SNAP/bin/moviAsm" "$SNAP/bin/moviLLD"

{
  echo "snapshot: $(date -Is)"
  echo "source clang: $CLANG_REAL"
  sha256sum "$SNAP/bin/clang-14" "$SNAP/bin/ld.lld" "$SNAP/isa/shave_asm.py" \
            "$SNAP/isa/encodings.json" "$SNAP/lib/mlibm.a"
  python3 -c "import ast;ast.parse(open('$SNAP/isa/shave_asm.py').read())" 2>/dev/null \
    && echo "shave_asm.py: parses" || echo "shave_asm.py: SYNTAX-BROKEN"
} > "$SNAP/REVISION.txt"
cat "$SNAP/REVISION.txt"

# sanity anchor: the snapshot must reproduce the live toolchain on the fixture
"$SNAP/bin/moviCompile" -cc1 -triple shave -target-cpu 3720xx -S -O2 -x c entry.c \
  -ffunction-sections -fdata-sections -o - \
  < /home/asdf/projects/npunlock-work/npunlock/tests/fixtures/npu3720/add1-fp16.c > /tmp/snap_add1.s
"$SNAP/bin/moviAsm" --cv 3720xx --noSPrefixing < /tmp/snap_add1.s > /tmp/snap_add1.o
"$SNAP/bin/moviLLD" -flavor gnu -EL -e controlled_act -z max-page-size=0x10 --gc-sections \
  -T /home/asdf/projects/npunlock-work/npunlock/src/shavecc/shave_kernel.ld /tmp/snap_add1.o -o - > /tmp/snap_add1.elf
python3 "$CORPUS/tools/validate_elf.py" /tmp/snap_add1.elf
echo "SNAP_DIR=$SNAP"

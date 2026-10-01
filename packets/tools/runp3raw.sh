#!/usr/bin/env bash
# runp3.sh — build a .s (or .c) SHAVE source into a kernel ELF, patch into the
# carrier, create the bundle, run dump_probe (ground-truth ACTUAL) and report
# the full smoke oracle only on request.
#   usage: runp3.sh SRC.{s,c} [--oracle] [--tag NAME]
set -u
SRC="$1"; shift || true
ASMFLAGS="${ASMFLAGS:---no-legalize}"
WANT_ORA=0; TAG=""
while [ $# -gt 0 ]; do
  case "$1" in
    --oracle) WANT_ORA=1; shift;;
    --tag) TAG="$2"; shift 2;;
    *) shift;;
  esac
done
LLVM=/home/asdf/projects/llvm-14
WORK=/home/asdf/projects/npunlock-work
ISA=$WORK/shave-isa
ROOT=$WORK/shave-toolchain-root
REPO=$WORK/npunlock
BUILD=$REPO/build/fresh
BASE=${OUT:-/tmp/runp3}
NAME=$(basename "$SRC"); NAME=${NAME%.*}
[ -n "$TAG" ] && NAME="$NAME-$TAG"
OUT="$BASE/$NAME"
rm -rf "$OUT"; mkdir -p "$OUT"
ELF=$OUT/$NAME.elf

case "$SRC" in
  *.s) cp "$SRC" "$OUT/$NAME.s" ;;
  *.c)
    "$LLVM/build/bin/clang" -cc1 -triple shave -target-cpu 3720xx -S -O2 -I "$REPO/include" -x c "$SRC" \
      -ffunction-sections -fdata-sections -o "$OUT/$NAME.s" || { echo "COMPILE FAIL"; exit 1; } ;;
esac
python3 "$ISA/shave_asm.py" obj $ASMFLAGS "$OUT/$NAME.s" -o "$OUT/$NAME.o" || { echo "ASM FAIL"; exit 1; }
"$LLVM/build/bin/ld.lld" -flavor gnu -EL -e controlled_act -z max-page-size=0x10 --gc-sections \
  "$ROOT/lib/mlibm.a" -T "$REPO/src/shavecc/shave_kernel.ld" \
  "$OUT/$NAME.o" -o "$ELF" || { echo "LINK FAIL"; exit 1; }
echo "ELF: $ELF ($(stat -c%s "$ELF") bytes)"

"$BUILD/npunlock-patchblob" --graph "$REPO/tests/fixtures/npu3720/abs-add-1x16-tile1.blob" \
  --elf "$ELF" --output "$OUT/graph.blob" --report "$OUT/patch-report.json" >/dev/null || { echo "PATCH FAIL"; exit 1; }
python3 "$REPO/tools/porting_probe.py" create --graph "$OUT/graph.blob" --output-dir "$OUT/bundle" \
  --producer-platform "Linux x86_64" --device "Intel NPU (/dev/accel0)" \
  --driver-version unknown --graph-compiler-version unknown >/dev/null || { echo "BUNDLE FAIL"; exit 1; }
"$LLVM/tmpdump/dump_probe" --bundle "$OUT/bundle" 2>&1 | grep -E "INPUT|EXPECT|ACTUAL|oracle" | head -4

if [ $WANT_ORA -eq 1 ]; then
  "$REPO/tools/npu_add1_smoke.sh" --elf "$ELF" >"$OUT/$NAME.smoke.log" 2>&1
  echo "SMOKE rc=$?"
  grep -E "oracle|PASS:" "$OUT/$NAME.smoke.log" | tail -3 || tail -5 "$OUT/$NAME.smoke.log"
fi

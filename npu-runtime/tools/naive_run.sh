#!/usr/bin/env bash
# Naive (baseline) NPU execution flow, exactly the stages a caller would have
# to reproduce by hand today:
#   shavecc-compile -> npunlock-patchblob -> porting_probe.py create -> probe
# Each stage is timed; the totals are the baseline our cached runtime must beat.
#
# usage: naive_run.sh --src KERNEL.c [--work DIR] [--toolchain-dir ROOT] [--tries N]
# Output: one line per stage with elapsed milliseconds; a TSV summary on stdout.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_ROOT="$(cd "$HERE/.." && pwd)"
WORK_ROOT="/home/asdf/projects/npunlock-work"
NPUNLOCK="$WORK_ROOT/npunlock"
UNLOCK_ROOT="$WORK_ROOT"

SRC=""
WORK="/tmp/NPURuntime/naive"
TC="$WORK_ROOT/shave-toolchain-root"
TRIES=1
while [[ $# -gt 0 ]]; do
  case "$1" in
    --src) SRC="$2"; shift 2 ;;
    --work) WORK="$2"; shift 2 ;;
    --toolchain-dir) TC="$2"; shift 2 ;;
    --tries) TRIES="$2"; shift 2 ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done
[[ -n "$SRC" && -f "$SRC" ]] || { echo "--src is required and must exist" >&2; exit 2; }

BUILD="${NPUNLOCK_BUILD_DIR:-$NPUNLOCK/build/fresh}"
PATCH="$BUILD/npunlock-patchblob"
PROBE="$BUILD/npunlock-linux-probe"
CARRIER="$NPUNLOCK/tests/fixtures/npu3720/abs-add-1x16-tile1.blob"
LDSCRIPT="$NPUNLOCK/src/shavecc/shave_kernel.ld"
INCLUDE_DIR="$WORK_ROOT/corpus/include"
ENTRY="${ENTRY:-controlled_act}"
CFLAGS_EXTRA="${CFLAGS_EXTRA:-}"
LINKLIB="${LINKLIB:-}"
PROBE_JSON="$PROBE"

for f in "$PATCH" "$PROBE" "$CARRIER" "$LDSCRIPT"; do
  [[ -e "$f" ]] || { echo "missing prerequisite: $f" >&2; exit 1; }
done

now_ms() { python3 -c 'import time;print(int(time.time()*1000))'; }

mkdir -p "$WORK"
TRIES="$TRIES" SRC="$SRC" WORK="$WORK" TC="$TC" BUILD="$BUILD" PATCH="$PATCH" \
  PROBE="$PROBE" CARRIER="$CARRIER" LDSCRIPT="$LDSCRIPT" INCLUDE_DIR="$INCLUDE_DIR" \
  ENTRY="$ENTRY" CFLAGS_EXTRA="$CFLAGS_EXTRA" LINKLIB="$LINKLIB" NPUNLOCK="$NPUNLOCK" \
  now_ms_fn=1 bash -s <<'EOS'
set -uo pipefail
name="$(basename "$SRC" .c)"
printf 'stage\ttry\tms\n'
for try in $(seq 1 "$TRIES"); do
  rm -rf "$WORK/bundle" "$WORK/out.s" "$WORK/out.o" "$WORK/out.elf" "$WORK/graph.blob"
  t0=$(python3 -c 'import time;print(int(time.time()*1000))')
  # shellcheck disable=SC2086
  timeout 25 "$TC/bin/moviCompile" -cc1 -triple shave -target-cpu 3720xx -S -O2 \
    -x c entry.c -ffunction-sections -fdata-sections -I"$INCLUDE_DIR" $CFLAGS_EXTRA -o - \
    < "$SRC" > "$WORK/out.s" 2> "$WORK/compile.err"
  t1=$(python3 -c 'import time;print(int(time.time()*1000))')
  timeout 25 "$TC/bin/moviAsm" --cv 3720xx --noSPrefixing < "$WORK/out.s" > "$WORK/out.o" 2> "$WORK/asm.err"
  t2=$(python3 -c 'import time;print(int(time.time()*1000))')
  linkline=("$TC/bin/moviLLD" -flavor gnu -EL -e "$ENTRY" -z max-page-size=0x10 --gc-sections)
  [[ -n "$LINKLIB" ]] && linkline+=("$TC/lib/$LINKLIB.a")
  linkline+=(-T "$LDSCRIPT" "$WORK/out.o" -o "$WORK/out.elf")
  timeout 25 "${linkline[@]}" 2> "$WORK/link.err"
  t3=$(python3 -c 'import time;print(int(time.time()*1000))')
  "$PATCH" --graph "$CARRIER" --elf "$WORK/out.elf" --output "$WORK/graph.blob" \
    --report "$WORK/patch-report.json" > "$WORK/patch.out" 2>&1
  t4=$(python3 -c 'import time;print(int(time.time()*1000))')
  python3 "$NPUNLOCK/tools/porting_probe.py" create --graph "$WORK/graph.blob" \
    --output-dir "$WORK/bundle" --producer-platform "$(uname -s) $(uname -m)" \
    --device "Intel NPU (/dev/accel0)" --driver-version unknown \
    --graph-compiler-version unknown > "$WORK/create.out" 2>&1
  t5=$(python3 -c 'import time;print(int(time.time()*1000))')
  "$PROBE" --bundle "$WORK/bundle" --report "$WORK/execution-report.json" \
    > "$WORK/probe.out" 2>&1
  rc=$?
  t6=$(python3 -c 'import time;print(int(time.time()*1000))')
  printf 'compile\t%s\t%s\n' "$try" "$((t1-t0))"
  printf 'assemble\t%s\t%s\n' "$try" "$((t2-t1))"
  printf 'link\t%s\t%s\n' "$try" "$((t3-t2))"
  printf 'patchblob\t%s\t%s\n' "$try" "$((t4-t3))"
  printf 'bundle_create\t%s\t%s\n' "$try" "$((t5-t4))"
  printf 'probe_run\t%s\t%s\n' "$try" "$((t6-t5))"
  printf 'total\t%s\t%s\n' "$try" "$((t6-t0))"
  printf 'probe_exit\t%s\t%s\n' "$try" "$rc"
done
EOS

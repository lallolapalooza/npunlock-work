#!/usr/bin/env bash
# Corpus driver: compile -> assemble -> link -> validate every kernel in
# corpus/kernels/*.c with the native SHAVE toolchain (shave-toolchain-root).
#
# This script is READ-ONLY with respect to every tree except npunlock-work/corpus.
#
# usage: run_corpus.sh [--only NAME[,NAME...]] [--timeout SECONDS]
#
# Per-kernel sidecar file kernels/<NAME>.opts (optional), one setting per line:
#   CFLAGS=<extra compile flags>      e.g. -DMLIBM_DEFINE_LINK_COMPAT=1
#   LINKLIB=mlibm                     add lib/mlibm.a to the link line
#   ENTRY=<symbol>                    entry symbol (default controlled_act)
#   TIMEOUT=<seconds>                 stage timeout override
#
# Outputs: results.tsv (one row per kernel, tab separated),
#          logs/<NAME>.{compile,asm,link}.err, build/<NAME>.{s,o,elf}
set -uo pipefail

CORPUS="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$CORPUS/.." && pwd)"
# pinned toolchain root; override with NPUNLOCK_SHAVE_TOOLCHAIN_DIR
# (tools/make_snapshot.sh produces corpus/toolchain-snapshot for reproducible runs)
TC="${NPUNLOCK_SHAVE_TOOLCHAIN_DIR:-$ROOT/shave-toolchain-root}"
LDSCRIPT="$ROOT/npunlock/src/shavecc/shave_kernel.ld"
INCLUDE_DIR="$CORPUS/include"
BUILD="$CORPUS/build"
LOGS="$CORPUS/logs"

ONLY=""
TAG=""
STAGE_TIMEOUT=25
KERNELDIR="$CORPUS/kernels"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --only) ONLY="$2"; shift 2 ;;
    --dir) KERNELDIR="$2"; shift 2 ;;
    --tag) TAG="$2"; shift 2 ;;
    --timeout) STAGE_TIMEOUT="$2"; shift 2 ;;
    -h | --help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

KERNELS="${KERNELDIR:-$CORPUS/kernels}"
LOG_PREFIX="${TAG:-$(basename "$KERNELS")}"
mkdir -p "$BUILD" "$LOGS"

# record the exact toolchain revisions used for this run (the assembler tree is
# under active development, so results are only meaningful with this stamp)
{
  echo "run: $(date -Is)"
  sha256sum "$TC/bin/moviCompile" "$TC/bin/moviAsm" "$TC/bin/moviLLD" 2>/dev/null
  sha256sum "$TC/bin/clang-14" "$TC/bin/ld.lld" "$ROOT/shave-isa/shave_asm.py" \
          "$ROOT/shave-isa/encodings.json" 2>/dev/null
  ASM_PY="$ROOT/shave-isa/shave_asm.py"
  [ -f "$TC/isa/shave_asm.py" ] && ASM_PY="$TC/isa/shave_asm.py"
  if python3 -c "import ast,sys; ast.parse(open('$ASM_PY').read())" 2>/dev/null; then
    echo "shave_asm.py ($ASM_PY): parses"
  else
    echo "shave_asm.py ($ASM_PY): SYNTAX-BROKEN (assembler stage unusable)"
  fi
  sha256sum "$ASM_PY" 2>/dev/null
} > "$LOGS/$LOG_PREFIX.toolchain.txt"
RESULTS="$CORPUS/results_$LOG_PREFIX.tsv"

# machine-readable stage outcome, plus a short classification hint
hint_for() { # $1 = stage, $2 = logfile
  local stage="$1" log="$2"
  [[ -s "$log" ]] || { echo ""; return; }
  if grep -q "PLEASE submit a bug report\|Stack dump" "$log"; then echo "LLVM-CRASH"; return; fi
  if grep -q "Cannot select" "$log"; then echo "ISEL-CannotSelect"; return; fi
  if grep -q "LLVM ERROR" "$log"; then echo "LLVM-FATAL"; return; fi
  if grep -qi "error:" "$log"; then echo "error"; return; fi
  echo "$stage-diagnostic"
}

printf 'kernel\tcompile\trc\tasm\trc\tlink\trc\tvalidate\ttext_bytes\tclass\tfirst_message\n' > "$RESULTS"

for src in "$KERNELS"/*.c; do
  name="$(basename "$src" .c)"
  if [[ -n "$ONLY" && ",$ONLY," != *",$name,"* ]]; then continue; fi
  cflags="-I$INCLUDE_DIR"
  linklib=""
  entry="controlled_act"
  stage_timeout="$STAGE_TIMEOUT"
  opts="$KERNELS/$name.opts"
  if [[ -f "$opts" ]]; then
    while IFS= read -r line; do
      case "$line" in
        CFLAGS=*) cflags="$cflags ${line#CFLAGS=}" ;;
        LINKLIB=*) linklib="${line#LINKLIB=}" ;;
        ENTRY=*) entry="${line#ENTRY=}" ;;
        TIMEOUT=*) stage_timeout="${line#TIMEOUT=}" ;;
        ""|\#*) ;;
      esac
    done < "$opts"
  fi

  cerr="$LOGS/$LOG_PREFIX.$name.compile.err"; aerr="$LOGS/$LOG_PREFIX.$name.asm.err"; lerr="$LOGS/$LOG_PREFIX.$name.link.err"
  : > "$cerr"; : > "$aerr"; : > "$lerr"
  rm -f "$BUILD/$LOG_PREFIX.$name.s" "$BUILD/$LOG_PREFIX.$name.o" "$BUILD/$LOG_PREFIX.$name.elf"

  # ---- stage 1: compile (C on stdin, assembly on stdout) ----
  # shellcheck disable=SC2086
  timeout "$stage_timeout" "$TC/bin/moviCompile" -cc1 -triple shave -target-cpu 3720xx -S -O2 \
    -x c entry.c -ffunction-sections -fdata-sections $cflags -o - \
    < "$src" > "$BUILD/$LOG_PREFIX.$name.s" 2> "$cerr"
  crc=$?
  if [[ $crc -eq 124 ]]; then cls="HANG-compile"; cstat="HANG"; else
    if [[ $crc -ne 0 ]]; then cstat="FAIL"; cls="$(hint_for compile "$cerr")"; else cstat="OK"; cls=""; fi
  fi

  astat="-"; arc="-"; lstat="-"; lrc="-"; vstat="SKIP"; text="-"
  linkline=("$TC/bin/moviLLD" -flavor gnu -EL -e "$entry" -z max-page-size=0x10 --gc-sections)

  if [[ "$cstat" == "OK" ]]; then
    # ---- stage 2: assemble ----
    timeout "$stage_timeout" "$TC/bin/moviAsm" --cv 3720xx --noSPrefixing \
      < "$BUILD/$LOG_PREFIX.$name.s" > "$BUILD/$LOG_PREFIX.$name.o" 2> "$aerr"
    arc=$?
    if [[ $arc -eq 124 ]]; then astat="HANG"; else
      if [[ $arc -ne 0 ]]; then astat="FAIL"; [[ -z "$cls" ]] && cls="$(hint_for assemble "$aerr")"; else astat="OK"; fi
    fi
  fi

  if [[ "$astat" == "OK" ]]; then
    # ---- stage 3: link ----
    [[ -n "$linklib" ]] && linkline+=("$TC/lib/$linklib.a")
    linkline+=(-T "$LDSCRIPT" "$BUILD/$LOG_PREFIX.$name.o" -o -)
    timeout "$stage_timeout" "${linkline[@]}" > "$BUILD/$LOG_PREFIX.$name.elf" 2> "$lerr"
    lrc=$?
    if [[ $lrc -eq 124 ]]; then lstat="HANG"; else
      if [[ $lrc -ne 0 ]]; then lstat="FAIL"; [[ -z "$cls" ]] && cls="$(hint_for link "$lerr")"; else lstat="OK"; fi
    fi
  fi

  if [[ "$lstat" == "OK" ]]; then
    # ---- stage 4: structural contract validation ----
    vout="$(python3 "$CORPUS/tools/validate_elf.py" "$BUILD/$LOG_PREFIX.$name.elf" 2>&1)"
    if [[ $? -eq 0 ]]; then
      vstat="OK"; text="$(sed -n 's/.*text=\([0-9]*\).*/\1/p' <<<"$vout")"
      [[ "$text" == "0" ]] && text="-"
      [[ -z "$cls" ]] && cls="PASS"
    else
      vstat="FAIL"; [[ -z "$cls" ]] && cls="STRUCT:${vout#FAIL }"
    fi
  fi

  msg=""
  for f in "$cerr" "$aerr" "$lerr"; do
    if [[ -s "$f" ]]; then msg="$(grep -v '^$' "$f" | head -1 | tr '\t' ' ' | cut -c1-200)"; break; fi
  done
  [[ -z "$msg" && "$vstat" == "FAIL" ]] && msg="${vout#FAIL }"

  printf '%s\t%s\t%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$name" "$cstat" "$crc" "$astat" "$arc" "$lstat" "$lrc" "$vstat" "$text" "$cls" "$msg" >> "$RESULTS"
  printf '%-32s compile=%-5s asm=%-5s link=%-5s validate=%-5s text=%-6s %s\n' \
    "$name" "$cstat" "$astat" "$lstat" "$vstat" "$text" "$cls"
done

echo "results: $RESULTS"

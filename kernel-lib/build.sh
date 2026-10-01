#!/usr/bin/env bash
# kernel-lib: reproducible build of the precompiled SHAVE/NPU kernel library.
#
#   build.sh [--clean] [--npu] [--only NAME[,NAME...]]
#
# Environment:
#   NPUNLOCK_SHAVE_TOOLCHAIN_DIR  toolchain root (bin/moviCompile, bin/moviAsm,
#                                 bin/moviLLD, lib/mlibm.a).  Default:
#                                 /home/asdf/projects/npunlock-work/shave-toolchain-root
#   NPUNLOCK_ROOT                 npunlock checkout, used only to cross-check the
#                                 vendored ABI header/ld script.  Default:
#                                 /home/asdf/projects/npunlock-work/npunlock
#   NPUNLOCK_SMOKE_BUILD_DIR      npunlock build tree with the CLIs.
#                                 Default: $NPUNLOCK_ROOT/build/fresh
#   NPUNLOCK_SMOKE_WORK_DIR       private scratch dir for NPU runs.
#                                 Default: /tmp/kernellib
#
# Stages per kernel (the exact npunlock shavecc contract, see
# npunlock/src/shavecc/shavecc.c):
#   compile  moviCompile -cc1 -triple shave -target-cpu 3720xx -S -O2 -x c
#            entry.c -ffunction-sections -fdata-sections [-D..] -I src -o -
#   assemble moviAsm --cv 3720xx --noSPrefixing
#   link     moviLLD -flavor gnu -EL -e controlled_act -z max-page-size=0x10
#            --gc-sections [-L lib -lmlibm] -T src/shave_kernel.ld <obj> -o -
#   validate tools/validate_elf.py (mirrors npunlock_parse_shave_elf)
#
# Outputs: elf/<name>.elf, build/logs/<name>.{compile,asm,link}.err,
#          build/status.json (per-kernel stage results), manifest.json.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TC="${NPUNLOCK_SHAVE_TOOLCHAIN_DIR:-/home/asdf/projects/npunlock-work/shave-toolchain-root}"
NPUNLOCK_ROOT="${NPUNLOCK_ROOT:-/home/asdf/projects/npunlock-work/npunlock}"
BUILD_DIR="${NPUNLOCK_SMOKE_BUILD_DIR:-$NPUNLOCK_ROOT/build/fresh}"
SRC="$HERE/src"
ELF_DIR="$HERE/elf"
WORK="$HERE/build"
LOGS="$WORK/logs"
STAGE_TIMEOUT="${KERNEL_LIB_STAGE_TIMEOUT:-60}"

CLEAN=0
NPU=0
ONLY=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean) CLEAN=1 ;;
    --npu) NPU=1 ;;
    --only) ONLY="$2"; shift ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
  shift
done

die() { echo "build.sh: $*" >&2; exit 1; }

# The shared toolchain wrappers exec binaries that sibling agents rebuild live
# (llvm-14/build/bin/clang, ld.lld), so a stage can fail with "exec: ..." or a
# BUS error and no toolchain diagnostic.  Those are retried; a real diagnostic
# (compiler error, Cannot select, assembler/linker error) is returned verbatim.
run_stage() { # run_stage OUTFILE ERRLOG -- command...
  local out="$1" err="$2"
  shift 2
  local attempt=1 rc=0
  while :; do
    timeout "$STAGE_TIMEOUT" "$@" > "$out" 2> "$err"
    rc=$?
    [[ $rc -eq 0 ]] && return 0
    [[ $rc -eq 124 ]] && return 124
    grep -qE "error:|Cannot select|unsupported|warning:" "$err" && return $rc
    if [[ $attempt -ge 4 ]]; then return $rc; fi
    echo "note: retrying '$1' after transient failure (rc=$rc, attempt $attempt)" >&2
    attempt=$((attempt + 1))
    sleep 5
  done
}


for tool in moviCompile moviAsm moviLLD; do
  [[ -x "$TC/bin/$tool" ]] || die "toolchain binary not found: $TC/bin/$tool"
done
[[ -f "$TC/lib/mlibm.a" ]] || die "mlibm.a not found: $TC/lib/mlibm.a"
command -v python3 >/dev/null || die "python3 is required"

if [[ $CLEAN -eq 1 ]]; then
  rm -rf "$ELF_DIR" "$LOGS"
fi
mkdir -p "$ELF_DIR" "$LOGS" "$WORK"

# --- NPU execution probe (reuses npunlock's own infer_engine.c) --------------
if command -v cc >/dev/null; then
  cc -O2 -Wall -Wextra -o "$WORK/klib_probe" "$HERE/tools/klib_probe.c" \
     "$NPUNLOCK_ROOT/src/linux_probe/infer_engine.c" \
     -I"$NPUNLOCK_ROOT/src/linux_probe" -I"$NPUNLOCK_ROOT/src/workers" -ldl \
     || die "klib_probe build failed"
fi

# --- cross-check the vendored ABI header / linker script against npunlock -----
if [[ -f "$NPUNLOCK_ROOT/include/npunlock/npu3720_kernel.h" ]]; then
  if ! cmp -s "$SRC/npunlock/npu3720_kernel.h" \
              "$NPUNLOCK_ROOT/include/npunlock/npu3720_kernel.h"; then
    echo "WARNING: vendored npu3720_kernel.h differs from $NPUNLOCK_ROOT/include" >&2
  fi
fi
if [[ -f "$NPUNLOCK_ROOT/src/shavecc/shave_kernel.ld" ]]; then
  if ! cmp -s "$SRC/shave_kernel.ld" "$NPUNLOCK_ROOT/src/shavecc/shave_kernel.ld"; then
    echo "WARNING: vendored shave_kernel.ld differs from $NPUNLOCK_ROOT/src/shavecc" >&2
  fi
fi

{
  echo "toolchain_root: $TC"
  for tool in moviCompile moviAsm moviLLD; do
    printf '%s: ' "$tool"; sha256sum "$TC/bin/$tool" | cut -d' ' -f1
  done
  printf 'mlibm.a: '; sha256sum "$TC/lib/mlibm.a" | cut -d' ' -f1
  printf 'npu3720_kernel.h: '; sha256sum "$SRC/npunlock/npu3720_kernel.h" | cut -d' ' -f1
  printf 'shave_kernel.ld: '; sha256sum "$SRC/shave_kernel.ld" | cut -d' ' -f1
} > "$WORK/toolchain-stamp.txt"

STATUS_DIR="$WORK/status"
mkdir -p "$STATUS_DIR"
printf -v STATUS_HEADER 'name\tcompile\trc\tasm\trc\tlink\trc\tvalidate\ttext_bytes\tsha256\tfirst_message\tentries'

for src in "$SRC"/*.c; do
  name="$(basename "$src" .c)"
  if [[ -n "$ONLY" && ",$ONLY," != *",$name,"* ]]; then continue; fi

  cflags="-I$SRC -ffunction-sections -fdata-sections"
  linklib=""
  fixup=1
  ldscript="$SRC/shave_kernel.ld"
  gc=1
  # optional sidecar: src/<name>.opts, one KEY=VALUE per line
  if [[ -f "$SRC/$name.opts" ]]; then
    while IFS= read -r optline; do
      case "$optline" in
        FIXUP=off) fixup=0 ;;
        FIXUP=on) fixup=1 ;;
        LDSCRIPT=*) ldscript="$HERE/${optline#LDSCRIPT=}" ;;
        GC=keep) gc=0 ;;    # multi-entry images must keep every entry function
        CFLAGS=*) cflags="$cflags ${optline#CFLAGS=}" ;;
        ""|\#*) ;;
        *) echo "WARNING: unknown option in $name.opts: $optline" >&2 ;;
      esac
    done < "$SRC/$name.opts"
  fi
  # mlibm kernels declare the compat stubs themselves; detect the marker.
  if grep -q "MLIBM_DEFINE_LINK_COMPAT" "$src"; then
    cflags="$cflags -DMLIBM_DEFINE_LINK_COMPAT=1"
    linklib="$TC/lib/mlibm.a"
  fi

  cerr="$LOGS/$name.compile.err"; aerr="$LOGS/$name.asm.err"; lerr="$LOGS/$name.link.err"
  s_file="$WORK/$name.s"; o_file="$WORK/$name.o"; elf_file="$ELF_DIR/$name.elf"
  : > "$cerr"; : > "$aerr"; : > "$lerr"
  rm -f "$s_file" "$o_file" "$elf_file"

  cstat="-"; crc="-"; astat="-"; arc="-"; lstat="-"; lrc="-"; vstat="SKIP"; text="-"

  run_stage "$s_file.raw" "$cerr" -- "$TC/bin/moviCompile" -cc1 -triple shave -target-cpu 3720xx \
    -S -O2 -x c entry.c $cflags -o - < "$src"
  crc=$?
  if [[ $crc -eq 0 ]]; then
    if [[ $fixup -eq 1 ]]; then
      # fp32 ALU register form (see tools/fixup_asm.py); raw .s kept for diffing
      python3 "$HERE/tools/fixup_asm.py" < "$s_file.raw" > "$s_file" || crc=1
    else
      cp "$s_file.raw" "$s_file"
    fi
  else
    : > "$s_file"
  fi
  if [[ $crc -eq 124 ]]; then cstat="HANG"; elif [[ $crc -ne 0 ]]; then cstat="FAIL"; else cstat="OK"; fi

  if [[ "$cstat" == "OK" ]]; then
    run_stage "$o_file" "$aerr" -- "$TC/bin/moviAsm" --cv 3720xx --noSPrefixing < "$s_file"
    arc=$?
    if [[ $arc -eq 124 ]]; then astat="HANG"; elif [[ $arc -ne 0 ]]; then astat="FAIL"; else astat="OK"; fi
  fi

  if [[ "$astat" == "OK" ]]; then
    linkline=("$TC/bin/moviLLD" -flavor gnu -EL -e controlled_act -z max-page-size=0x10)
    [[ $gc -eq 1 ]] && linkline+=(--gc-sections)
    [[ -n "$linklib" ]] && linkline+=("$linklib")
    linkline+=(-T "$ldscript" "$o_file" -o -)
    run_stage "$elf_file" "$lerr" -- "${linkline[@]}"
    lrc=$?
    if [[ $lrc -eq 124 ]]; then lstat="HANG"; elif [[ $lrc -ne 0 ]]; then lstat="FAIL"; else lstat="OK"; fi
  fi

  if [[ "$lstat" == "OK" ]]; then
    vout="$(python3 "$HERE/tools/validate_elf.py" "$elf_file" 2>&1)"
    if [[ $? -eq 0 ]]; then
      vstat="OK"; text="$(sed -n 's/.*text=\([0-9]*\).*/\1/p' <<<"$vout")"
    else
      vstat="FAIL"; text="-"; echo "$vout" > "$LOGS/$name.validate.err"
    fi
  fi

  msg=""
  for f in "$cerr" "$aerr" "$lerr"; do
    if [[ -s "$f" ]]; then msg="$(grep -v '^$' "$f" | head -1 | tr '\t' ' ' | cut -c1-220)"; break; fi
  done

  sha="-"
  [[ "$vstat" == "OK" ]] && sha="$(sha256sum "$elf_file" | cut -d' ' -f1)"

  entries="-"
  if [[ "$vstat" == "OK" ]]; then
    entries="$(python3 "$HERE/tools/elf_entries.py" "$elf_file" | tr '\n' ';' | sed 's/;$//')"
    [[ -z "$entries" ]] && entries="-"
  fi

  # one file per kernel: re-running a subset (--only) never loses other rows
  { printf '%s\n' "$STATUS_HEADER"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "$name" "$cstat" "$crc" "$astat" "$arc" "$lstat" "$lrc" "$vstat" "$text" "$sha" "$msg" "$entries"
  } > "$STATUS_DIR/$name.tsv"

  printf '%-18s compile=%-5s asm=%-5s link=%-5s validate=%-5s text=%-5s %s\n' \
    "$name" "$cstat" "$astat" "$lstat" "$vstat" "$text" "$(echo "$entries" | cut -c1-60)"
done

# aggregate view for humans (make_manifest reads build/status/<name>.tsv)
find "$STATUS_DIR" -name '*.tsv' | sort | xargs -r cat > "$WORK/status.tsv"

# --- manifest.json (contracts x build status x NPU evidence) ------------------
python3 "$HERE/tools/make_manifest.py" "$HERE" || die "manifest generation failed"

if [[ $NPU -eq 1 ]]; then
  echo "== NPU evidence run"
  python3 "$HERE/tools/run_npu.py" --all || die "NPU evidence run failed"
  python3 "$HERE/tools/make_manifest.py" "$HERE" || die "manifest regeneration failed"
fi

echo "kernel-lib: built into $ELF_DIR; manifest: $HERE/manifest.json"

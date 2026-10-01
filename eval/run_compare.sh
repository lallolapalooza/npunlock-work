#!/usr/bin/env bash
# Deterministic CPU-vs-NPU evaluation harness for the gemma-4-E4B-it-Q4_0 run.
#
# What it does
#   1. runs the frozen prompt set (eval/prompts.json) through llama-cli once per
#      mode -- (a) CPU only (-dev none), (b) with the NPU backend selected
#      (-dev SHAVE0, ggml-shave, owner GGMLBackend) -- with greedy decoding,
#      a fixed seed, fixed -c/-t, and a bounded -n;
#   2. runs llama-bench per mode for authoritative prompt-processing (pp) and
#      generation (tg) tokens/s;
#   3. scores every prompt against its recorded expected-structure checks and
#      compares the generated text of each mode against the CPU baseline
#      byte-for-byte (they must agree exactly on the oracle-exact prompts);
#   4. records the backend's own offload statistics (GGML_SHAVE_STATS=1).
#
# Usage
#   eval/run_compare.sh                       # cpu + npu, fresh run dir
#   eval/run_compare.sh --modes cpu           # baseline only
#   eval/run_compare.sh --determinism         # adds a second CPU pass (cpu2)
#   eval/run_compare.sh --only arith,repeat   # subset of the prompt set
#   eval/run_compare.sh --quick               # tiny n_predict, no llama-bench
#   eval/run_compare.sh --out DIR --label L
#
# Exit status: 0 when every check passes and no mode diverged from the CPU
# baseline; 1 otherwise (the summary is still written).
#
# Nothing in llama.cpp/ or npunlock/ is modified: this script is read-only with
# respect to both trees.

set -uo pipefail

EVAL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_ROOT="$(cd "$EVAL_DIR/.." && pwd)"
TOOLS="$EVAL_DIR/tools"
PYTHON="${PYTHON:-python3}"

LLAMA_DIR="$WORK_ROOT/llama.cpp"
BIN="$LLAMA_DIR/build/bin/llama-cli"
BENCH_BIN="$LLAMA_DIR/build/bin/llama-bench"
MODEL="../models/gemma-4-E4B-it-Q4_0.gguf"
PROMPTSET="$EVAL_DIR/prompts.json"

MODES="cpu,npu"
DEVICE_CPU="none"
DEVICE_NPU="SHAVE0"
THREADS=8
# 4096 = the frozen decoding contract in prompts.json.  The long-context prompt
# is ~2062 tokens, so 2048 would make llama-cli reject it outright ("request
# exceeds the available context size") and generate nothing.
CTX=4096
SEED=1234
TIMEOUT=300
ONLY=""
LABEL=""
OUT_DIR=""
DETERMINISM=0
QUICK=0
BENCH_PP=128
BENCH_NG=32
BENCH_REPS=2
SKIP_COST=0
SCORE_ONLY=0

usage() {
    # Print the leading comment block verbatim (shebang dropped).  The block is
    # found by reading until the first non-comment line instead of a hard-coded
    # `sed -n '2,Np'` range, so editing the header can never truncate the help
    # text or spill code into it.
    awk 'NR > 1 { if ($0 !~ /^#/) exit; sub(/^# ?/, ""); print }' "${BASH_SOURCE[0]}"
    cat <<'EOF'

Options:
  --modes LIST         comma-separated modes: cpu, cpu2, npu   (default cpu,npu)
  --determinism        add a second CPU pass (cpu2) to the mode list
  --only IDS           comma-separated prompt ids from prompts.json
  --quick             short -n per prompt, skip llama-bench and cost steps
  --out DIR            run directory (default eval/runs/<utc timestamp>)
  --label LABEL        human label recorded in the summary
  --model PATH         GGUF path (default ../models/gemma-4-E4B-it-Q4_0.gguf)
  --llama-dir DIR      llama.cpp checkout (default eval/../llama.cpp)
  --promptset FILE     frozen prompt set (default eval/prompts.json)
  --device-cpu DEV     value for -dev in cpu/cpu2 modes (default none)
  --device-npu DEV     value for -dev in npu mode        (default SHAVE0)
  --threads N          -t (default 8)     --ctx N    -c (default 4096)
  --seed N             -s (default 1234)  --timeout S  per-prompt bound (300)
  --bench-pp/--bench-ng/--bench-reps N    llama-bench -p/-n/-r (128/32/2)
  --skip-cost          skip the NPU engine-cost / patch+probe cycle benches
  --score-only         skip every run/bench/cost step and re-score the
                       records already in --out (idempotent summary)
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --modes) MODES="$2"; shift 2 ;;
        --device-cpu) DEVICE_CPU="$2"; shift 2 ;;
        --device-npu) DEVICE_NPU="$2"; shift 2 ;;
        --llama-dir) LLAMA_DIR="$2"; BIN="$LLAMA_DIR/build/bin/llama-cli"
                     BENCH_BIN="$LLAMA_DIR/build/bin/llama-bench"; shift 2 ;;
        --model) MODEL="$2"; shift 2 ;;
        --promptset) PROMPTSET="$2"; shift 2 ;;
        --threads) THREADS="$2"; shift 2 ;;
        --ctx) CTX="$2"; shift 2 ;;
        --seed) SEED="$2"; shift 2 ;;
        --timeout) TIMEOUT="$2"; shift 2 ;;
        --only) ONLY="$2"; shift 2 ;;
        --out) OUT_DIR="$2"; shift 2 ;;
        --label) LABEL="$2"; shift 2 ;;
        --determinism) DETERMINISM=1; shift ;;
        --quick) QUICK=1; shift ;;
        --bench-pp) BENCH_PP="$2"; shift 2 ;;
        --bench-ng) BENCH_NG="$2"; shift 2 ;;
        --bench-reps) BENCH_REPS="$2"; shift 2 ;;
        --skip-cost) SKIP_COST=1; shift ;;
        --score-only) SCORE_ONLY=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "run_compare.sh: unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ ! -r "$PROMPTSET" ]]; then
    echo "run_compare.sh: prompt set not found: $PROMPTSET" >&2; exit 2
fi
if [[ ! -x "$BIN" ]]; then
    echo "run_compare.sh: llama-cli not found: $BIN" >&2; exit 2
fi

if [[ -z "$OUT_DIR" ]]; then
    OUT_DIR="$EVAL_DIR/runs/$(date -u +%Y%m%dT%H%M%SZ)"
fi
mkdir -p "$OUT_DIR"
# Absolute before anything exports it: TMPDIR below goes to llama-cli, which
# runs with cwd=$LLAMA_DIR, and the ggml-shave engine creates its scratch dir
# as "$TMPDIR/ggml-shave-<pid>".  With a relative --out that mkdir resolves
# against llama.cpp/ and fails, so the backend reports
# `engine:"unavailable"` and (in -dev SHAVE0 mode) silently offloads nothing.
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
PROMPTS_DIR="$OUT_DIR/prompts"
rm -rf "$PROMPTS_DIR"
# A private scratch dir keeps the ggml-shave engine (and any sibling run) from
# sharing state; the NPU device itself is shared, so never reuse a common path.
export TMPDIR="$OUT_DIR/tmp"
mkdir -p "$TMPDIR"
export GGML_SHAVE_STATS=1

if [[ "$DETERMINISM" == "1" && "$MODES" == *"cpu"* && "$MODES" != *"cpu2"* ]]; then
    MODES="$MODES,cpu2"
fi

device_for() {
    case "$1" in
        cpu|cpu2) echo "$DEVICE_CPU" ;;
        npu) echo "$DEVICE_NPU" ;;
        *) echo "$1" ;;
    esac
}

echo "== eval/run_compare.sh"
echo "   out dir : $OUT_DIR"
echo "   modes   : $MODES"
echo "   model   : $MODEL (cwd $LLAMA_DIR)"
echo "   threads : $THREADS   ctx: $CTX   seed: $SEED   n bound per prompt"

# --- prompt materialisation ---------------------------------------------------
mapfile -t ROWS < <("$PYTHON" "$TOOLS/dump_prompts.py" \
    --promptset "$PROMPTSET" --out-dir "$PROMPTS_DIR" ${ONLY:+--only "$ONLY"})
if [[ "${#ROWS[@]}" -eq 0 ]]; then
    echo "run_compare.sh: no prompts selected" >&2; exit 2
fi

# --- device inventory (recorded, and validated for the NPU mode) --------------
"$BIN" --list-devices > "$OUT_DIR/devices.txt" 2>&1 || true
SHAVE_DEVICE="$(grep -oE '^[[:space:]]+[A-Za-z0-9_]+:' "$OUT_DIR/devices.txt" | head -1 | tr -d ' :')"
echo "   devices : $(tr '\n' ' ' < "$OUT_DIR/devices.txt")"
if [[ "$MODES" == *"npu"* && -z "$SHAVE_DEVICE" ]]; then
    echo "run_compare.sh: no NPU device registered; build llama.cpp with -DGGML_SHAVE=ON" >&2
fi

# --- per-prompt runs ----------------------------------------------------------
# --score-only re-scores whatever is already in --out: no model run, no bench,
# no cost measurement, so a summary can be regenerated (or modes combined) after
# the fact without touching the recorded evidence.
if [[ "$SCORE_ONLY" != "1" ]]; then
for mode in ${MODES//,/ }; do
    device="$(device_for "$mode")"
    mkdir -p "$OUT_DIR/$mode"
    echo "== mode $mode (device ${device:-default})"

    for row in "${ROWS[@]}"; do
        IFS=$'\t' read -r id kind n_predict <<<"$row"
        if [[ "$QUICK" == "1" && "$n_predict" -gt 24 ]]; then n_predict=24; fi
        printf '   [%s] %-12s %s' "$mode" "$id" "$kind"
        start=$(date +%s)
        "$PYTHON" "$TOOLS/cli_run.py" \
            --model "$MODEL" --prompt-file "$PROMPTS_DIR/$id.txt" \
            --id "$id" --n "$n_predict" \
            --ctx "$CTX" --threads "$THREADS" --seed "$SEED" \
            --binary "$BIN" --cwd "$LLAMA_DIR" --device "$device" \
            --timeout "$TIMEOUT" \
            --out "$OUT_DIR/$mode/$id.json" \
            > "$OUT_DIR/$mode/$id.cli_run.log" 2>&1
        rc=$?
        elapsed=$(( $(date +%s) - start ))
        tps="$("$PYTHON" -c "import json,sys;r=json.load(open(sys.argv[1]));print(r.get('gen_tps'))" "$OUT_DIR/$mode/$id.json" 2>/dev/null || echo '?')"
        printf '  %ss  gen=%s t/s  (rc=%s)\n' "$elapsed" "$tps" "$rc"
        if [[ $rc -ne 0 ]]; then
            echo "   !! record tool failed for $id in mode $mode" >&2
        fi
    done

    "$PYTHON" - "$OUT_DIR" "$mode" "$device" "$MODEL" "$THREADS" "$CTX" "$SEED" <<'PY'
import json, pathlib, sys, datetime
out, mode, device, model, threads, ctx, seed = sys.argv[1:8]
pathlib.Path(out, f"meta_{mode}.json").write_text(json.dumps({
    "schema": "npunlock.eval.runmeta.v1",
    "mode": mode,
    "device": device,
    "model": model,
    "threads": int(threads),
    "ctx": int(ctx),
    "seed": int(seed),
    "temperature": 0.0,
    "bin": "build/bin/llama-cli",
    "recorded_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
}, indent=2) + "\n")
PY
done

# --- llama-bench --------------------------------------------------------------
if [[ "$SCORE_ONLY" != "1" && "$QUICK" != "1" && -x "$BENCH_BIN" ]]; then
    for mode in ${MODES//,/ }; do
        device="$(device_for "$mode")"
        echo "== llama-bench $mode (device ${device:-default})  pp=$BENCH_PP ng=$BENCH_NG reps=$BENCH_REPS"
        # llama-bench runs from $LLAMA_DIR: --model is relative to the llama.cpp
        # checkout (cli_run.py gets the same treatment via --cwd).  Without this
        # the model fails to load and every bench_*.json stays empty.
        ( cd "$LLAMA_DIR" && timeout $((TIMEOUT * 4)) "$BENCH_BIN" -m "$MODEL" \
            -p "$BENCH_PP" -n "$BENCH_NG" -r "$BENCH_REPS" -t "$THREADS" \
            --no-warmup -dev "$device" -o json ) \
            > "$OUT_DIR/bench_$mode.json" 2> "$OUT_DIR/bench_$mode.err" \
            || echo "   !! llama-bench failed for $mode (see bench_$mode.err)" >&2
    done
fi

# --- NPU host-cost measurements -------------------------------------------------
# Step 4 of the evaluation contract: the per-invocation host cost decides
# whether per-op offload can pay off at all.
if [[ "$SCORE_ONLY" != "1" && "$MODES" == *"npu"* && "$SKIP_COST" != "1" ]]; then
    echo "== NPU engine cost (shipped ggml-shave engine, self-reported setup_ms/npu_ms)"
    "$PYTHON" "$TOOLS/npu_engine_cost.py" \
        --llama-dir "$LLAMA_DIR" --repeats 3 \
        --work-dir "$OUT_DIR/tmp/engine-cost" \
        --out "$OUT_DIR/npu_engine_cost.json" \
        > "$OUT_DIR/npu_engine_cost.log" 2>&1 \
        || { echo "   !! engine cost bench failed (see npu_engine_cost.log)" >&2
             rm -f "$OUT_DIR/npu_engine_cost.json"; }
    tail -3 "$OUT_DIR/npu_engine_cost.log" 2>/dev/null

    echo "== NPU patch+probe cycle cost"
    "$PYTHON" "$TOOLS/npu_cycle_time.py" --repeats 3 \
        --work-dir "$OUT_DIR/tmp/cycle" --out "$OUT_DIR/npu_cycle_cost.json" \
        > "$OUT_DIR/npu_cycle_cost.log" 2>&1 \
        || { echo "   !! cycle bench failed (see npu_cycle_cost.log)" >&2
             rm -f "$OUT_DIR/npu_cycle_cost.json"; }
    tail -3 "$OUT_DIR/npu_cycle_cost.log" 2>/dev/null
fi

fi  # --score-only wrapper around the run/bench/cost stages

# --- known prompts subset: synthesise a filtered prompt set for the scorer ----
SCORER_SET="$PROMPTSET"
if [[ -n "$ONLY" ]]; then
    SCORER_SET="$OUT_DIR/prompts.selected.json"
    "$PYTHON" - "$PROMPTSET" "$ONLY" "$SCORER_SET" <<'PY'
import json, pathlib, sys
src, only, dst = sys.argv[1], sys.argv[2], sys.argv[3]
data = json.loads(pathlib.Path(src).read_text())
keep = {item for item in only.split(",") if item}
data["prompts"] = [p for p in data["prompts"] if p["id"] in keep]
pathlib.Path(dst).write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")
PY
fi

# --- score + compare ----------------------------------------------------------
echo "== compare"
"$PYTHON" "$TOOLS/compare.py" \
    --dir "$OUT_DIR" --promptset "$SCORER_SET" \
    --modes "$MODES" --baseline cpu \
    --label "${LABEL:-$(basename "$OUT_DIR")}" --model "$MODEL" \
    > "$OUT_DIR/compare.log" 2>&1
compare_rc=$?
cat "$OUT_DIR/compare.log"

echo
echo "artifacts:"
echo "  summary : $OUT_DIR/summary.md"
echo "  json    : $OUT_DIR/summary.json"
echo "  records : $OUT_DIR/{$(echo "${MODES//,/,}" | sed 's/,/ /g')}/"
exit $compare_rc

#!/usr/bin/env python3
"""Score and compare recorded llama-cli runs against the frozen prompt set.

Input : a run directory written by eval/run_compare.sh
          <dir>/meta.json
          <dir>/<mode>/<prompt-id>.json      (records from tools/cli_run.py)
        eval/prompts.json                    (the frozen prompt set)
Output: <dir>/summary.json and <dir>/summary.md

The comparison contract:
  * every prompt is scored against its recorded expected-structure checks
    (pure text checks -- decoding correctness, not speed);
  * for every prompt, the candidate mode's generated text is compared with the
    CPU baseline **exactly** (byte-for-byte after the harness trims the trailing
    newline), because greedy decoding on an oracle-exact path must not diverge;
  * per-mode speed is the mean of the per-prompt llama-cli timings plus, when
    present, llama-bench numbers.
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import statistics

ORACLE_EXACT_KINDS = {
    "arithmetic", "short_answer", "repeat_after_me", "instruction_following",
    "counting", "long_context_tail",
}


def normalize(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def check_one(text: str, check: dict) -> tuple[bool, str]:
    kind = check["type"]
    if kind == "regex":
        ok = re.search(check["pattern"], text) is not None
    elif kind == "equals_normalized":
        value = normalize(check["value"])
        got = normalize(text)
        ok = got == value or value in got
    elif kind == "each_line_matches":
        lines = [line.strip() for line in text.splitlines() if line.strip()]
        ok = (len(lines) == check["count"]
              and all(re.search(check["pattern"], line) for line in lines))
    elif kind == "min_words":
        ok = len(text.split()) >= check["count"]
    else:
        raise SystemExit(f"unknown check type: {kind}")
    return ok, f"{kind}: {check.get('desc', check.get('pattern', ''))}"


def score_record(record: dict, spec: dict) -> dict:
    text = record.get("generated_text") or ""
    results = []
    for check in spec.get("checks", []):
        ok, desc = check_one(text, check)
        results.append({"ok": ok, "desc": desc})
    return {
        "all_pass": all(item["ok"] for item in results),
        "checks": results,
    }


def load_records(directory: pathlib.Path, promptset: dict) -> dict:
    records = {}
    for spec in promptset["prompts"]:
        path = directory / f"{spec['id']}.json"
        if path.exists():
            records[spec["id"]] = json.loads(path.read_text())
    return records


def mean_metric(records: dict, field: str):
    values = [r[field] for r in records.values() if r.get(field) is not None]
    return round(statistics.fmean(values), 3) if values else None


def load_bench(path: pathlib.Path):
    if not path.exists():
        return None
    try:
        payload = json.loads(path.read_text())
    except (json.JSONDecodeError, OSError):
        # A failed or truncated llama-bench run must not sink the whole summary:
        # the speed table already tolerates a missing bench ("n/a").
        return None
    if not isinstance(payload, list):
        return None
    out = {}
    for row in payload:
        if row.get("n_gen", 0) and row.get("n_prompt", 0):
            continue
        key = "pp" if row.get("n_prompt", 0) else "tg"
        out[f"{key}_tps"] = round(row["avg_ts"], 3)
        out[f"{key}_stddev"] = round(row["stddev_ts"], 3)
        out[f"{key}_n"] = row.get("n_prompt", 0) or row.get("n_gen", 0)
    return out or None


def render_markdown(summary: dict) -> str:
    lines = []
    lines.append(f"# eval comparison — {summary['label']}")
    lines.append("")
    lines.append(f"- prompt set: `{summary['promptset']}` "
                 f"({summary['prompt_count']} prompts)")
    lines.append(f"- model: `{summary['model']}`")
    lines.append(f"- baseline mode: `{summary['baseline_mode']}`")
    lines.append("")

    # Speed
    lines.append("## Speed (llama-cli per-prompt means)")
    lines.append("")
    lines.append("| mode | device | prompt t/s | generation t/s | wall s (mean) |")
    lines.append("|---|---|---|---|---|")
    for mode, data in summary["modes"].items():
        lines.append(
            f"| {mode} | `{data['device']}` | {data['prompt_tps_mean']} | "
            f"{data['gen_tps_mean']} | {data['wall_s_mean']} |")
    lines.append("")
    lines.append("## Speed (llama-bench, authoritative)")
    lines.append("")
    lines.append("| mode | pp t/s | pp stddev | tg t/s | tg stddev |")
    lines.append("|---|---|---|---|---|")
    for mode, data in summary["modes"].items():
        bench = data.get("bench") or {}
        lines.append(
            f"| {mode} | {bench.get('pp_tps', 'n/a')} | {bench.get('pp_stddev', '')} | "
            f"{bench.get('tg_tps', 'n/a')} | {bench.get('tg_stddev', '')} |")
    lines.append("")

    # Correctness
    lines.append("## Expected-structure checks and exact-output agreement")
    lines.append("")
    modes = list(summary["modes"].keys())
    header = "| prompt | kind | " + " | ".join(f"{m} checks" for m in modes)
    if summary["baseline_mode"] in modes:
        for mode in modes:
            if mode != summary["baseline_mode"]:
                header += f" | {mode} vs {summary['baseline_mode']} text"
    header += " |"
    lines.append(header)
    lines.append("|---" * (2 + len(modes) + (len(modes) - 1)) + "|")
    for row in summary["rows"]:
        cells = [f"`{row['id']}`", row["kind"]]
        for mode in modes:
            entry = row["modes"].get(mode, {})
            if entry.get("run_ok") is False:
                cells.append("**RUN ERROR**")
            else:
                mark = "PASS" if entry.get("all_pass") else "FAIL"
                cells.append(f"{mark} ({entry.get('passed', 0)}/{entry.get('total', 0)})")
        for mode in modes:
            if mode == summary["baseline_mode"]:
                continue
            entry = row["modes"].get(mode, {})
            verdict = entry.get("match_vs_baseline")
            cells.append({"exact": "exact", "diverged": "**DIVERGED**",
                          "missing": "missing", "baseline": "-"}.get(verdict, "-"))
        lines.append("| " + " | ".join(cells) + " |")
    lines.append("")

    # Offload
    lines.append("## Backend offload statistics (`GGML_SHAVE_STATS=1`)")
    lines.append("")
    priority = ["engine", "invocations", "offloaded", "declined", "unsupported",
                "lanes", "tile_elements", "avg_invocation_us"]
    keys = []
    for _, data in summary["modes"].items():
        for key in (data.get("shave_stats") or {}):
            if key not in keys:
                keys.append(key)
    keys.sort(key=lambda k: (priority.index(k) if k in priority else len(priority), k))
    if keys:
        lines.append("| mode | " + " | ".join(keys) + " |")
        lines.append("|---" * (len(keys) + 1) + "|")
        for mode, data in summary["modes"].items():
            stats = data.get("shave_stats") or {}
            lines.append(f"| {mode} | " +
                         " | ".join(str(stats.get(key, "n/a")) for key in keys) + " |")
        lines.append("")
        lines.append("`offloaded`/`invocations` are the backend's own counters; "
                     "`declined` is the number of `supports_op` calls answered "
                     "false, i.e. nodes the scheduler left on the CPU.")
    else:
        lines.append("_no stats line captured (the backend did not report)_")
    lines.append("")
    # NPU host cost: the number that decides whether per-op offload can pay off.
    eng = summary.get("npu_engine_cost")
    cyc = summary.get("npu_cycle_cost")
    if eng or cyc:
        lines.append("## NPU host cost (per-op-offload economics)")
        lines.append("")
        if eng:
            s = eng["summary"]
            best = s.get("us_per_invocation_best")
            lines.append(
                f"- **one NPU invocation through the shipped engine "
                f"(`{' '.join(pathlib.Path(str(eng['command'][0])).parts[-1:])}`): "
                f"{best} us (best of {len(eng['runs'])} runs, median "
                f"{s.get('us_per_invocation_median')} us) -> "
                f"{round(1e6 / best) if best else 'n/a'} invocations/s**, "
                f"tile {s.get('tile_bytes')} B, ops {s.get('ops')}, "
                f"one-off engine setup {s.get('setup_ms_median')} ms, "
                f"best {s.get('elements_per_s_best')} elements/s offloaded")
            lines.append(
                "- this is the *shipped-engine* regime: payload staging plus one "
                "npu-runtime execute per call, as the engine's own `npu_ms` / "
                "`avg_invocation_us` report it.  It is not the 0.14-0.5 ms "
                "live-graph re-execution measured in-process in "
                "`multiop/REPORT.md` section 3, and not the 22-55 ms fresh-graph "
                "lifecycle of the previous engine either.")
        if cyc:
            p = cyc["summary"]["per_phase_ms"]
            lines.append(
                f"- full patch+probe cycle (patchblob -> bundle -> verify -> "
                f"NPU run), the reference flow with no worker reuse: "
                f"{p['total']['min_ms']} ms -> "
                f"**{cyc['summary']['invocations_per_s_full_cycle']:.1f} "
                f"kernel deployments/s**")
            lines.append(
                f"  (patchblob {p['patch']['min_ms']} ms, bundle create "
                f"{p['bundle_create']['min_ms']} ms, verify "
                f"{p['bundle_verify']['min_ms']} ms, NPU run "
                f"{p['probe_execute']['min_ms']} ms)")
        lines.append("")
        lines.append(
            "An elementwise ggml op on a model tensor is tens of microseconds of "
            "CPU work; a per-invocation round trip of milliseconds cannot "
            "amortise unless one offloaded node replaces far more than one tile."
        )
        lines.append("")

    lines.append("")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dir", required=True, help="run directory")
    parser.add_argument("--promptset", required=True)
    parser.add_argument("--modes", default="cpu,npu")
    parser.add_argument("--baseline", default="cpu")
    parser.add_argument("--label", default="")
    parser.add_argument("--model", default="")
    args = parser.parse_args()

    run_dir = pathlib.Path(args.dir)
    promptset = json.loads(pathlib.Path(args.promptset).read_text())
    modes = [m for m in args.modes.split(",") if m]

    # Shave stats: keep the invocation counts from any record (the numbers are
    # per-process; the last record of a mode is as good as any, and a run that
    # offloaded nothing reports zero everywhere).
    summary = {
        "schema": "npunlock.eval.summary.v1",
        "label": args.label or run_dir.name,
        "promptset": str(pathlib.Path(args.promptset).name),
        "prompt_count": len(promptset["prompts"]),
        "model": args.model,
        "baseline_mode": args.baseline,
        "modes": {},
        "rows": [],
    }

    per_mode: dict[str, dict] = {}
    for mode in modes:
        directory = run_dir / mode
        records = load_records(directory, promptset)
        per_mode[mode] = records
        meta = {}
        meta_path = run_dir / f"meta_{mode}.json"
        if meta_path.exists():
            meta = json.loads(meta_path.read_text())
        stats = None
        for record in records.values():
            if record.get("shave_stats"):
                stats = record["shave_stats"]
        summary["modes"][mode] = {
            "device": meta.get("device", "?"),
            "records": len(records),
            "prompt_tps_mean": mean_metric(records, "prompt_tps"),
            "gen_tps_mean": mean_metric(records, "gen_tps"),
            "wall_s_mean": mean_metric(records, "wall_s"),
            "shave_stats": stats,
            "bench": load_bench(run_dir / f"bench_{mode}.json"),
        }

    for spec in promptset["prompts"]:
        row = {"id": spec["id"], "kind": spec["kind"], "modes": {}}
        baseline_text = None
        if args.baseline in per_mode and spec["id"] in per_mode[args.baseline]:
            baseline_text = per_mode[args.baseline][spec["id"]].get("generated_text")
        for mode in modes:
            record = per_mode[mode].get(spec["id"])
            if record is None:
                row["modes"][mode] = {"passed": 0, "total": len(spec["checks"]),
                                      "all_pass": False, "match_vs_baseline": "missing"}
                continue
            score = score_record(record, spec)
            run_ok = record.get("ok", True)
            entry = {
                "passed": sum(1 for c in score["checks"] if c["ok"]),
                "total": len(spec["checks"]),
                "all_pass": score["all_pass"] and run_ok,
                "run_ok": run_ok,
                "error_lines": record.get("error_lines", []),
                "gen_tps": record.get("gen_tps"),
                "prompt_tps": record.get("prompt_tps"),
            }
            if mode == args.baseline:
                entry["match_vs_baseline"] = "baseline"
            elif baseline_text is None or record.get("generated_text") is None:
                entry["match_vs_baseline"] = "missing"
            elif record["generated_text"] == baseline_text:
                entry["match_vs_baseline"] = "exact"
            else:
                entry["match_vs_baseline"] = "diverged"
            entry["generated_text"] = record.get("generated_text")
            entry["checks"] = score["checks"]
            row["modes"][mode] = entry
        row["oracle_exact"] = spec["kind"] in ORACLE_EXACT_KINDS
        summary["rows"].append(row)

    for cost_name, key in (("npu_engine_cost.json", "npu_engine_cost"),
                           ("npu_cycle_cost.json", "npu_cycle_cost")):
        cost_path = run_dir / cost_name
        if cost_path.exists():
            summary[key] = json.loads(cost_path.read_text())

    summary["verdict"] = {
        "all_checks_pass": all(
            entry["all_pass"]
            for row in summary["rows"] for entry in row["modes"].values()
        ),
        "exact_matches": sum(
            1 for row in summary["rows"] for mode, entry in row["modes"].items()
            if entry.get("match_vs_baseline") == "exact"
        ),
        "divergences": [
            {"id": row["id"], "mode": mode}
            for row in summary["rows"] for mode, entry in row["modes"].items()
            if entry.get("match_vs_baseline") == "diverged"
        ],
    }

    (run_dir / "summary.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n")
    markdown = render_markdown(summary)
    (run_dir / "summary.md").write_text(markdown)
    print(markdown)

    return 0 if not summary["verdict"]["divergences"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

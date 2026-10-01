#!/usr/bin/env python3
"""Per-invocation NPU cost as the *new* ggml-shave engine reports it.

The engine that landed on 2026-09-28 (libggml-shave.so sha256 b0671c3c…,
"via npunlock + npu-runtime") dropped the child `ggml-shave-runner` process and
its frame protocol; it drives npu-runtime in-process and instruments itself.
With `GGML_SHAVE_STATS=1` every process prints one parseable line at exit:

    ggml-shave-stats {"engine":"ready","ops":[…],"invocations":N,"offloaded":N,
      "declined":N,"unsupported":N,"elements_offloaded":N,"tile_bytes":N,
      "lanes":{…},"per_op_invocations":{…},"setup_ms":X,"npu_ms":Y,
      "avg_invocation_us":Z}

`setup_ms` is one-off engine/graph setup, `npu_ms` the accumulated NPU time of
all invocations and `avg_invocation_us` the mean per invocation.  This tool runs
a command several times (fresh process each time, since setup is per process)
and turns those numbers into per-invocation and elements/s figures, so the
"does offload pay" question is answered with the engine's own measurement
rather than a reconstruction.

The default command is the backend's acceptance test
(`build/bin/ggml-shave-selftest`), which is fast and exercises the shipped
engine path; `--cmd` can point at any process that prints the stats line (e.g. a
bounded `llama-cli` run for a model-scale number).

Regimes: this measures the *shipped* engine, whose per-invocation cost is the
whole call (payload staging + npu-runtime execute), not the raw live-graph
re-execute time measured in-process by multiop/REPORT.md §3 (0.14-0.5 ms).
Both numbers are reported in eval/baseline/RESULTS.md with their regime labels.

Usage:
  npu_engine_cost.py [--llama-dir DIR] [--cmd 'CMD …'] [--repeats 3]
                     [--timeout 300] [--work-dir DIR] [--out FILE]
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import shlex
import statistics
import subprocess

STATS_RE = re.compile(r"^ggml-shave-stats (\{.*\})\s*$", re.MULTILINE)


def parse_stats(text: str) -> dict | None:
    matches = STATS_RE.findall(text)
    if not matches:
        return None
    return json.loads(matches[-1])


def run_once(cmd, env, work, timeout):
    import time
    started = time.perf_counter()
    proc = subprocess.run(cmd, capture_output=True, text=True, env=env,
                          cwd=str(work), timeout=timeout)
    wall = time.perf_counter() - started
    text = (proc.stdout or "") + "\n" + (proc.stderr or "")
    stats = parse_stats(text)
    if stats is None:
        raise SystemExit(
            f"no ggml-shave-stats line from {cmd[0]} (rc={proc.returncode}); "
            f"set GGML_SHAVE_STATS=1 and check the command")
    return wall, stats, proc.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    default_llama = pathlib.Path(__file__).resolve().parents[2] / "llama.cpp"
    parser.add_argument("--llama-dir", default=str(default_llama))
    parser.add_argument("--cmd", default="", help="command to run (default: ggml-shave-selftest)")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--work-dir", help="scratch dir handed to the command as cwd/TMPDIR")
    parser.add_argument("--out")
    args = parser.parse_args()

    llama = pathlib.Path(args.llama_dir)
    if args.cmd:
        cmd = shlex.split(args.cmd)
    else:
        cmd = [str(llama / "build/bin/ggml-shave-selftest")]

    work = pathlib.Path(args.work_dir) if args.work_dir else pathlib.Path("/tmp")
    work.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env["GGML_SHAVE_STATS"] = "1"
    # Absolute: the engine creates "$TMPDIR/ggml-shave-<pid>" and the command
    # may run with a different cwd.
    env["TMPDIR"] = str(work.resolve())

    runs = []
    for index in range(args.repeats):
        wall, stats, rc = run_once(cmd, env, work, args.timeout)
        invocations = int(stats.get("invocations") or 0)
        elements = int(stats.get("elements_offloaded") or 0)
        npu_ms = float(stats.get("npu_ms") or 0.0)
        entry = {
            "repeat": index,
            "wall_s": round(wall, 3),
            "exit_code": rc,
            "engine": stats.get("engine"),
            "ops": stats.get("ops"),
            "format": stats.get("format"),
            "invocations": invocations,
            "offloaded": stats.get("offloaded"),
            "declined": stats.get("declined"),
            "unsupported": stats.get("unsupported"),
            "elements_offloaded": elements,
            "tile_bytes": stats.get("tile_bytes"),
            "lanes": stats.get("lanes"),
            "per_op_invocations": stats.get("per_op_invocations"),
            "setup_ms": stats.get("setup_ms"),
            "npu_ms": stats.get("npu_ms"),
            "avg_invocation_us": stats.get("avg_invocation_us"),
            "us_per_invocation_derived": (
                round(npu_ms * 1e3 / invocations, 1) if invocations else None),
            "elements_per_s": (
                round(elements / (npu_ms / 1e3), 1) if npu_ms > 0 and elements else None),
        }
        runs.append(entry)
        print(f"  run {index}: {invocations} invocations, "
              f"{entry['avg_invocation_us']} us/invocation, "
              f"npu {npu_ms:.1f} ms, setup {entry['setup_ms']} ms, "
              f"{entry['elements_per_s']} elements/s", flush=True)

    def med(key):
        values = [r[key] for r in runs if isinstance(r.get(key), (int, float))]
        return round(statistics.median(values), 3) if values else None

    inv = [r["invocations"] for r in runs]
    per_s = [r["avg_invocation_us"] for r in runs if r["avg_invocation_us"]]
    report = {
        "schema": "npunlock.eval.npu-engine-cost.v1",
        "what": ("per-invocation NPU cost reported by the shipped ggml-shave "
                 "engine itself (setup_ms / npu_ms / avg_invocation_us)"),
        "command": cmd,
        "llama_dir": str(llama),
        "repeats": args.repeats,
        "runs": runs,
        "summary": {
            "invocations_median": int(statistics.median(inv)) if inv else 0,
            "setup_ms_median": med("setup_ms"),
            "npu_ms_median": med("npu_ms"),
            "us_per_invocation_best": min(per_s) if per_s else None,
            "us_per_invocation_median": (
                round(statistics.median(per_s), 1) if per_s else None),
            "elements_per_s_best": max(
                (r["elements_per_s"] for r in runs if r["elements_per_s"]), default=None),
            "tile_bytes": runs[0].get("tile_bytes"),
            "lanes": runs[0].get("lanes"),
            "ops": runs[0].get("ops"),
        },
    }
    summary = report["summary"]
    print(f"\nBEST: {summary['us_per_invocation_best']} us/invocation "
          f"({summary['us_per_invocation_best'] and round(1e6 / summary['us_per_invocation_best'], 1)} "
          f"invocations/s), tile {summary['tile_bytes']} B, "
          f"ops {summary['ops']}", flush=True)

    if args.out:
        pathlib.Path(args.out).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out).write_text(json.dumps(report, indent=2) + "\n")
        print(f"wrote {args.out}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

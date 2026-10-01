#!/usr/bin/env python3
"""Time the patch+probe cycle: the host-side cost of getting one kernel onto the
NPU and running it, the way the reference flow does.

    npunlock-patchblob (carrier + ELF -> patched graph)
      -> porting_probe.py create (bundle)
      -> porting_probe.py verify (hashes/contract, no NPU)
      -> npunlock-linux-probe    (Level Zero session, graph create, execute, oracle)

Everything runs on the host plus one real NPU execution.  The last step is the
one that matters for the "how many NPU invocations per second" question when a
worker is *not* kept alive between operations; numpbench (tools/
npu_invocation_bench.py) measures the persistent-worker case and is the number
to use for a per-op-offload design.

There is no timing instrumentation in the probe itself (the execution report
carries no durations), so the phases are timed here around the CLIs, in a
private scratch directory.  Repeated `--repeats` times; the minimum of each
phase is reported next to the mean (the NPU is shared with other work).

Usage:
  npu_cycle_time.py [--repeats 3] [--elf PATH] [--npunlock-dir DIR]
                    [--work-dir DIR] [--out FILE]
"""

from __future__ import annotations

import argparse
import json
import pathlib
import platform
import shutil
import statistics
import subprocess
import time

DEFAULT_NPUNLOCK = pathlib.Path("/home/asdf/projects/npunlock-work/npunlock")
DEFAULT_ELF = pathlib.Path("/home/asdf/projects/npunlock-work/our-add1-fp16.elf")


def timed(argv, **kwargs) -> tuple[float, int]:
    started = time.perf_counter()
    proc = subprocess.run(argv, capture_output=True, **kwargs)
    return time.perf_counter() - started, proc.returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--npunlock-dir", default=str(DEFAULT_NPUNLOCK))
    parser.add_argument("--build-dir", help="npunlock build tree (default <dir>/build/fresh)")
    parser.add_argument("--elf", default=str(DEFAULT_ELF))
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--work-dir", default="/tmp/npunlock-eval-cycle")
    parser.add_argument("--out")
    args = parser.parse_args()

    repo = pathlib.Path(args.npunlock_dir)
    build = pathlib.Path(args.build_dir) if args.build_dir else repo / "build/fresh"
    patch = build / "npunlock-patchblob"
    probe = build / "npunlock-linux-probe"
    carrier = repo / "tests/fixtures/npu3720/abs-add-1x16-tile1.blob"
    elf = pathlib.Path(args.elf)

    for required in (patch, probe, carrier, elf):
        if not pathlib.Path(required).exists():
            raise SystemExit(f"required path missing: {required}")

    work = pathlib.Path(args.work_dir)
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True, exist_ok=True)

    device = "unknown"
    # The Intel NPU exposes /dev/accel/accel0 (the bare /dev/accel0 node is a
    # legacy path), so check both before falling back to "unknown".
    for candidate in ("/dev/accel/accel0", "/dev/accel0"):
        if pathlib.Path(candidate).exists():
            device = f"Intel NPU ({candidate})"
            break
    driver_version = "unknown"
    if shutil.which("dpkg-query"):
        proc = subprocess.run(["dpkg-query", "-W", "-f=${Version}", "intel-level-zero-npu"],
                              capture_output=True, text=True)
        if proc.returncode == 0:
            driver_version = proc.stdout.strip()

    phases = ("patch", "bundle_create", "bundle_verify", "probe_execute", "total")
    runs = []
    for index in range(args.repeats):
        iteration = work / f"iter{index}"
        iteration.mkdir(parents=True, exist_ok=True)
        graph = iteration / "graph.blob"
        bundle = iteration / "bundle"
        durations = {}

        started = time.perf_counter()
        took, rc = timed([str(patch), "--graph", str(carrier), "--elf", str(elf),
                          "--output", str(graph), "--report", str(iteration / "patch.json")])
        if rc != 0:
            raise SystemExit(f"patchblob failed (rc={rc})")
        durations["patch"] = took

        took, rc = timed(["python3", str(repo / "tools/porting_probe.py"), "create",
                          "--graph", str(graph), "--output-dir", str(bundle),
                          "--producer-platform", f"{platform.system()} {platform.machine()}",
                          "--device", device, "--driver-version", driver_version,
                          "--graph-compiler-version", "unknown"])
        if rc != 0:
            raise SystemExit(f"porting_probe create failed (rc={rc})")
        durations["bundle_create"] = took

        took, rc = timed(["python3", str(repo / "tools/porting_probe.py"), "verify",
                          str(bundle)])
        if rc != 0:
            raise SystemExit(f"porting_probe verify failed (rc={rc})")
        durations["bundle_verify"] = took

        took, rc = timed([str(probe), "--bundle", str(bundle),
                          "--report", str(iteration / "execution-report.json")])
        if rc != 0:
            raise SystemExit(f"linux-probe failed (rc={rc})")
        durations["probe_execute"] = took
        durations["total"] = time.perf_counter() - started

        runs.append({key: round(value * 1e3, 1) for key, value in durations.items()})
        print(f"  iter {index}: patch {runs[-1]['patch']} ms  "
              f"bundle {runs[-1]['bundle_create']} ms  "
              f"verify {runs[-1]['bundle_verify']} ms  "
              f"probe(NPU) {runs[-1]['probe_execute']} ms  "
              f"total {runs[-1]['total']} ms", flush=True)

    summary = {}
    per_phase = {}
    for phase in phases:
        values = [run[phase] for run in runs]
        per_phase[phase] = {
            "mean_ms": round(statistics.fmean(values), 1),
            "min_ms": round(min(values), 1),
            "max_ms": round(max(values), 1),
        }
    summary["per_phase_ms"] = per_phase
    summary["invocations_per_s_probe_only"] = round(
        1000.0 / per_phase["probe_execute"]["min_ms"], 1)
    summary["invocations_per_s_full_cycle"] = round(
        1000.0 / per_phase["total"]["min_ms"], 1)

    report = {
        "schema": "npunlock.eval.npu-cycle-cost.v1",
        "carrier": str(carrier),
        "kernel_elf": str(elf),
        "repeats": args.repeats,
        "runs": runs,
        "summary": summary,
    }

    print(f"\nprobe-only  : {per_phase['probe_execute']['min_ms']} ms -> "
          f"{summary['invocations_per_s_probe_only']} NPU graph runs/s")
    print(f"full cycle  : {per_phase['total']['min_ms']} ms -> "
          f"{summary['invocations_per_s_full_cycle']} kernel deployments/s")

    if args.out:
        pathlib.Path(args.out).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out).write_text(json.dumps(report, indent=2) + "\n")
        print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

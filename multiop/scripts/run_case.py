#!/usr/bin/env python3
"""run_case.py - patch a carrier with a kernel (optionally redirect ranges to
distinct entry offsets), build the v1 probe bundle, execute on the NPU and
report stage wall times plus dump_probe ACTUAL/EXPECT.

usage:
  run_case.py --name N --graph CARRIER.blob --elf KERNEL.elf
              [--range-addend R:ADDR,R:ADDR] [--range-extent R:SIZE,R:SIZE]
              [--dump-dir DIR] [--no-exec]
"""
import argparse
import json
import os
import subprocess
import sys
import time

ROOT = "/home/asdf/projects/npunlock-work"
BUILD = f"{ROOT}/npunlock/build/fresh"
PATCH = f"{BUILD}/npunlock-patchblob"
PROBE = f"{BUILD}/npunlock-linux-probe"
PORTING = f"{ROOT}/npunlock/tools/porting_probe.py"
DUMP_PROBE = "/home/asdf/projects/llvm-14/tmpdump/dump_probe"
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__))))
import blobtool as bt  # noqa: E402


def run(cmd, **kw):
    t0 = time.perf_counter()
    p = subprocess.run(cmd, capture_output=True, text=True, **kw)
    return p, time.perf_counter() - t0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", required=True)
    ap.add_argument("--graph", required=True)
    ap.add_argument("--elf", required=True)
    ap.add_argument("--range-addend", default="")
    ap.add_argument("--range-extent", default="")
    ap.add_argument("--work", default=None)
    ap.add_argument("--no-exec", action="store_true")
    args = ap.parse_args()

    work = args.work or os.path.join(ROOT, "multiop", "work", args.name)
    os.makedirs(work, exist_ok=True)
    timing = {}

    patched = os.path.join(work, "graph.patched.blob")
    p, dt = run([PATCH, "--graph", args.graph, "--elf", args.elf, "--output", patched,
                 "--report", os.path.join(work, "patch-report.json")])
    timing["patchblob_ms"] = dt * 1e3
    if p.returncode != 0:
        print(p.stderr, file=sys.stderr)
        return 1
    rep = json.load(open(os.path.join(work, "patch-report.json")))
    timing["patch"] = rep

    final = patched
    if args.range_addend or args.range_extent:
        d = bytearray(open(patched, "rb").read())
        rmap = {}
        for item in filter(None, args.range_addend.split(",")):
            r, a = item.split(":")
            rmap[int(r, 0)] = int(a, 0)
        if rmap:
            bt.patch_ranges(d, rmap)
        for item in filter(None, args.range_extent.split(",")):
            r, s = item.split(":")
            bt.set_extent(d, int(r, 0), int(s, 0))
        final = os.path.join(work, "graph.blob")
        open(final, "wb").write(d)

    bundle = os.path.join(work, "bundle")
    subprocess.run(["rm", "-rf", bundle], check=True)
    p, dt = run(["python3", PORTING, "create", "--graph", final, "--output-dir", bundle,
                 "--producer-platform", "linux", "--device", "Intel NPU (/dev/accel0)",
                 "--driver-version", "test", "--graph-compiler-version", "test"])
    timing["bundle_create_ms"] = dt * 1e3
    if p.returncode != 0:
        print("bundle create failed:", p.stderr, file=sys.stderr)
        return 1
    p, dt = run(["python3", PORTING, "verify", bundle])
    timing["bundle_verify_ms"] = dt * 1e3
    timing["bundle_verify_ok"] = p.returncode == 0

    if not args.no_exec:
        p, dt = run([PROBE, "--bundle", bundle, "--report", os.path.join(work, "exec.json")])
        timing["linux_probe_ms"] = dt * 1e3
        timing["linux_probe_exit"] = p.returncode
        if os.path.exists(os.path.join(work, "exec.json")):
            ex = json.load(open(os.path.join(work, "exec.json")))
            timing["stages"] = ex.get("stages")
            timing["status"] = ex.get("result", {}).get("status")
            timing["diagnostic"] = ex.get("diagnostic", "")
        p, dt = run([DUMP_PROBE, "--bundle", bundle, "--report", os.path.join(work, "dump.json")])
        timing["dump_probe_ms"] = dt * 1e3
        out = p.stdout + p.stderr
        timing["actual"] = ""
        timing["expect"] = ""
        for line in out.splitlines():
            if line.startswith("ACTUAL"):
                timing["actual"] = line
            if line.startswith("EXPECT"):
                timing["expect"] = line
        print(out.strip())
    print(json.dumps({k: v for k, v in timing.items() if k != "patch"}, indent=1, sort_keys=True))
    json.dump(timing, open(os.path.join(work, "timing.json"), "w"), indent=1, sort_keys=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())

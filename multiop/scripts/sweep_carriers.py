#!/usr/bin/env python3
"""sweep_carriers.py - build IR carriers of increasing tile size on Linux with
ircompile, inspect them, patch our add-one kernel and measure execution.

Everything is timed. Results are printed as TSV and written to a JSON file.
"""
import json
import os
import struct
import subprocess
import sys
import time

ROOT = "/home/asdf/projects/npunlock-work"
NC = f"{ROOT}/npunlock"
BUILD = f"{NC}/build/fresh"
MPROBE = f"{ROOT}/multiop/work/mprobe"
IRCOMPILE = f"{ROOT}/multiop/work/ircompile"
WORK = f"{ROOT}/multiop/work/sweep"
KERNEL = f"{ROOT}/multiop/work/big_add1.elf"


def gen_ir(n, op="Abs", path=None):
    path = path or WORK
    code = f"""
import npunlock as npu
from npunlock.ir import serialize_ir
x = npu.input("x", shape=(1,{n}), dtype="f16")
y = npu.op({op!r}, x, _shape=(1,{n}), _dtype="f16", _name="y")
g = npu.Graph(inputs=[x], outputs=[y], name="c{n}")
s = serialize_ir(g)
open({path!r}+"/n{n}.xml","wb").write(s.xml)
open({path!r}+"/n{n}.bin","wb").write(s.weights)
"""
    env = dict(os.environ, PYTHONPATH=f"{NC}/python")
    subprocess.run([sys.executable, "-c", code], check=True, env=env, cwd=ROOT)


def timed(cmd, **kw):
    t0 = time.perf_counter()
    p = subprocess.run(cmd, capture_output=True, text=True, **kw)
    return p, time.perf_counter() - t0


def main():
    sizes = [int(x) for x in sys.argv[1:]] or [32, 256, 1024, 2048, 4096, 8192, 16384]
    os.makedirs(WORK, exist_ok=True)
    rows = []
    for n in sizes:
        gen_ir(n)
        blob = f"{WORK}/n{n}.blob"
        p, dt = timed([IRCOMPILE, "--ir", f"{WORK}/n{n}.xml", "--weights", f"{WORK}/n{n}.bin",
                       "--out", blob])
        if p.returncode:
            print(f"N={n} compile FAILED: {p.stderr.strip()[:200]}")
            continue
        compile_ms = dt * 1e3
        subprocess.run([f"{BUILD}/npunlock-inspect", "--graph", blob,
                        "--report", f"{WORK}/n{n}.inspect.json"], capture_output=True)
        insp = json.load(open(f"{WORK}/n{n}.inspect.json"))
        tgt = insp["act"]["target_count"]
        per = insp["act"]["targets"][0]["element_count"]
        patched = f"{WORK}/n{n}.patched.blob"
        p, dt_patch = timed([f"{BUILD}/npunlock-patchblob", "--graph", blob, "--elf", KERNEL,
                             "--output", patched, "--report", f"{WORK}/n{n}.patch.json"])
        if p.returncode:
            print(f"N={n} patch FAILED: {p.stderr.strip()[:200]}")
            continue
        p, dt_run = timed([MPROBE, "--graph", patched, "--fill", "ones", "--repeat", "10"])
        if p.returncode:
            print(f"N={n} run FAILED: {p.stderr.strip()[:200]}")
            continue
        lines = p.stdout.splitlines()
        args = [l for l in lines if l.startswith("arg[")]
        tim = [l for l in lines if l.startswith("timing")][0]
        out = [l for l in lines if l.startswith("OUTPUT")][0]
        hexv = out.split(":")[1].split()
        vals = struct.unpack(f"<{len(hexv)//2}H", bytes.fromhex("".join(hexv)))
        ok = all(v == 0x4000 for v in vals)
        row = dict(n=n, act_targets=tgt, elems_per_inv=per, compile_ms=round(compile_ms, 1),
                   patch_ms=round(dt_patch * 1e3, 2), probe_total_ms=round(dt_run * 1e3, 2),
                   create_ms=float(tim.split("create_ms=")[1].split()[0]),
                   init_ms=float(tim.split("init_ms=")[1].split()[0]),
                   exec_first_ms=float(tim.split("exec_first_ms=")[1].split()[0]),
                   exec_mean_ms=float(tim.split("exec_mean_ms=")[1].split()[0]),
                   all_plus_one=ok)
        rows.append(row)
        warm_steady = None
        try:
            total = float(tim.split("exec_total_ms=")[1].split()[0])
            first = row["exec_first_ms"]
            warm_steady = (total - first) / 9.0
        except Exception:
            pass
        row["exec_warm_ms"] = round(warm_steady, 4) if warm_steady is not None else None
        row["elems_per_s_warm"] = round(n / (warm_steady / 1e3), 1) if warm_steady else None
        print(json.dumps(row))
    json.dump(rows, open(f"{WORK}/sweep.json", "w"), indent=1)
    print("wrote", f"{WORK}/sweep.json")


if __name__ == "__main__":
    main()

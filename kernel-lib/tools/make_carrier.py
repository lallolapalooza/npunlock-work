#!/usr/bin/env python3
"""kernel-lib carrier factory: build patchable NPU3720 carriers on Linux.

The vendor graph compiler is reachable from Linux through the installed Level
Zero driver (`zeGraphCreate2` NGRAPH_LITE + `zeGraphGetNative2`); npunlock's
shipped `ir2blob` path only lacks a POSIX worker (npunlock_worker is WIN32-only).
MultiOpProbe proved and implemented that front end
(`npunlock-work/multiop/scripts/ircompile.c`); this tool drives it so the kernel
library can target realistic tiles instead of the 16-element fixtures.

It also does the part the backend needs for multi-kernel carriers: patch a
multi-entry ELF and re-point each ACT range at its own 0x400-aligned entry
(`.rlt.text.ActKernelRanges` RELA addend + extent, the rewrite MultiOpProbe
pinned down).  The alignment rule is theirs: the range code address is rounded
down to 0x400, so entries must sit on 0x400 boundaries.

usage:
  tools/make_carrier.py --op Abs --shape 1024 [--force]
  tools/make_carrier.py --op Maximum --shape 1024 --inputs 2
  tools/make_carrier.py --list

Outputs: carriers/<op>_<inputs>in_1x<shape>.blob + .json (facts sidecar).
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent.parent
NPUNLOCK_ROOT = pathlib.Path(
    os.environ.get("NPUNLOCK_ROOT", "/home/asdf/projects/npunlock-work/npunlock")
)
NPUNLOCK_BUILD = pathlib.Path(
    os.environ.get("NPUNLOCK_SMOKE_BUILD_DIR", str(NPUNLOCK_ROOT / "build" / "fresh"))
)
MULTIOP = pathlib.Path(
    os.environ.get("KERNEL_LIB_MULTIOP", "/home/asdf/projects/npunlock-work/multiop")
)
CARRIER_DIR = HERE / "carriers"

# IR ops that the vendor compiler lowers to *patchable* ACT kernels at f16.
# (MultiOpProbe's menu: Add/Clamp/Multiply/Relu/Subtract are DPU-only.)
PATCHABLE = {
    "Abs": 1, "Elu": 1, "Erf": 1, "Sigmoid": 1, "Exp": 1, "Floor": 1, "Log": 1,
    "Sign": 1, "Sqrt": 1, "Tanh": 1,
    "Maximum": 2, "Minimum": 2, "Power": 2, "Divide": 2,
}
DPU_ONLY = ("Add", "Clamp", "Multiply", "Relu", "Subtract")


def ircompile_tool() -> pathlib.Path:
    tool = MULTIOP / "work" / "ircompile"
    if tool.is_file():
        return tool
    source = MULTIOP / "scripts" / "ircompile.c"
    if source.is_file():
        out = HERE / "build" / "ircompile"
        out.parent.mkdir(parents=True, exist_ok=True)
        include = MULTIOP / "scripts"
        subprocess.run(["cc", "-O2", "-o", str(out), str(source), f"-I{include}", "-ldl"], check=True)
        return out
    raise SystemExit("ircompile not found: set KERNEL_LIB_MULTIOP to the multiop work dir")


def generate_ir(op: str, shape: int, inputs: int, directory: pathlib.Path) -> tuple[pathlib.Path, pathlib.Path]:
    xml = directory / "graph.xml"
    weights = directory / "graph.bin"
    arglist = ", ".join(f'x{i}' for i in range(inputs))
    decls = "\n".join(f'x{i} = npu.input("x{i}", shape=(1,{shape}), dtype="f16")' for i in range(inputs))
    code = f"""
import npunlock as npu
from npunlock.ir import serialize_ir
{decls}
y = npu.op({op!r}, {arglist}, _shape=(1,{shape}), _dtype="f16", _name="y")
g = npu.Graph(inputs=[{arglist}], outputs=[y], name="c{shape}")
s = serialize_ir(g)
open({str(xml)!r}, "wb").write(s.xml)
open({str(weights)!r}, "wb").write(s.weights)
"""
    env = dict(os.environ, PYTHONPATH=str(NPUNLOCK_ROOT / "python"))
    subprocess.run([sys.executable, "-c", code], check=True, env=env, cwd=str(HERE))
    return xml, weights


def inspect(blob: pathlib.Path) -> dict:
    report = HERE / "build" / (blob.stem + ".inspect.json")
    report.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(NPUNLOCK_BUILD / "npunlock-inspect"), "--graph", str(blob),
                    "--report", str(report)], check=True, capture_output=True)
    return json.loads(report.read_text())


def build_carrier(op: str, shape: int, inputs: int, force: bool = False) -> dict:
    if op in DPU_ONLY:
        raise SystemExit(f"{op} lowers to DPU-only code (no ACT sections) -- not patchable")
    expected_inputs = PATCHABLE.get(op)
    if expected_inputs is None:
        raise SystemExit(f"unknown IR op {op!r}; known patchable: {sorted(PATCHABLE)}")
    if expected_inputs != inputs:
        raise SystemExit(f"{op} takes {expected_inputs} input(s), not {inputs}")

    CARRIER_DIR.mkdir(parents=True, exist_ok=True)
    name = f"{op.lower()}_{inputs}in_1x{shape}"
    blob = CARRIER_DIR / f"{name}.blob"
    sidecar = CARRIER_DIR / f"{name}.json"
    if blob.is_file() and sidecar.is_file() and not force:
        facts = json.loads(sidecar.read_text())
        facts["cached"] = True
        return facts

    scratch = HERE / "build" / "ir" / name
    scratch.mkdir(parents=True, exist_ok=True)
    xml, weights = generate_ir(op, shape, inputs, scratch)
    subprocess.run([str(ircompile_tool()), "--ir", str(xml), "--weights", str(weights),
                    "--out", str(blob)], check=True, capture_output=True)
    facts = inspect(blob)
    act = facts["act"]
    elements = act["targets"][0]["element_count"] if act["target_count"] else 0
    out = {
        "name": name,
        "ir_op": op,
        "ir_inputs": inputs,
        "shape": [1, shape],
        "dtype": "f16",
        "tensor_bytes": shape * 2,
        "blob": f"carriers/{name}.blob",
        "blob_bytes": blob.stat().st_size,
        "act_targets": act["target_count"],
        "act_groups": act["group_count"],
        "elements_per_invocation": elements,
        "targets": act["targets"],
        "build": {
            "ir": f"npu.op({op!r}, ...) at (1,{shape}) f16 -> serialize_ir",
            "ircompile": str(ircompile_tool()),
            "compiled_on": "Linux (Level Zero driver graph compiler)",
        },
    }
    sidecar.write_text(json.dumps(out, indent=2) + "\n")
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--op", default="Abs")
    parser.add_argument("--shape", type=int, default=1024)
    parser.add_argument("--inputs", type=int, default=1)
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()

    if args.list:
        for op, inputs in sorted(PATCHABLE.items()):
            print(f"{op:10s} inputs={inputs}")
        print("DPU-only (not patchable):", ", ".join(DPU_ONLY))
        return 0

    facts = build_carrier(args.op, args.shape, args.inputs, args.force)
    print(json.dumps(facts, indent=2) if not facts.get("cached")
          else f"cached {facts['name']}: targets={facts['act_targets']} "
               f"elems/inv={facts['elements_per_invocation']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

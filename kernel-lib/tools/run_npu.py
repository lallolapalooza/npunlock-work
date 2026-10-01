#!/usr/bin/env python3
"""kernel-lib NPU evidence runner.

Patches kernel-lib ELFs into a carrier and executes the patched graph on the
real NPU.

Two carrier families:
  * the prebuilt npunlock fixtures (1x16 ramp-add, 1x32 abs-abs), and
  * IR-built carriers (tools/make_carrier.py) with realistic tiles
    (512/8192 f16 per invocation), built on Linux through the driver's graph
    compiler.

Why not npunlock's own probe?  `npunlock-linux-probe` and the tmpdump
`dump_probe` are pinned to the fixed 1x16 / 32-byte porting-probe contract
(src/linux_probe/main.c rejects every other tensor size), so they cannot
execute a 1x1024 carrier.  tools/klib_probe.c reuses npunlock's own execution
engine (src/linux_probe/infer_engine.c) with an arbitrary-size command line.

Each case names the carrier, which ACT group is patched with which ELF, the raw
f16 input tensor(s), and a host reference over the values the carrier actually
delivers to its ACT (for the ramp carrier that model is itself verified by the
`copy16_ramp_model` case).

Usage:
  tools/run_npu.py --list
  tools/run_npu.py --all [--repeat N]
  tools/run_npu.py --case add_abs1024
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
import time

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import patch_ranges  # noqa: E402  (kernel-lib's own range re-pointer)

HERE = pathlib.Path(__file__).resolve().parent.parent
NPUNLOCK_ROOT = pathlib.Path(
    os.environ.get("NPUNLOCK_ROOT", "/home/asdf/projects/npunlock-work/npunlock")
)
NPUNLOCK_BUILD = pathlib.Path(
    os.environ.get("NPUNLOCK_SMOKE_BUILD_DIR", str(NPUNLOCK_ROOT / "build" / "fresh"))
)
WORK = pathlib.Path(os.environ.get("NPUNLOCK_SMOKE_WORK_DIR", "/tmp/kernellib"))
KLIB_PROBE = HERE / "build" / "klib_probe"
DUMP_PROBE = pathlib.Path(
    os.environ.get("KERNEL_LIB_DUMP_PROBE", "/home/asdf/projects/llvm-14/tmpdump/dump_probe")
)
NPUNLOCK_PROBE = NPUNLOCK_BUILD / "npunlock-linux-probe"
PREBUILT = pathlib.Path(
    "/home/asdf/projects/npunlock-work/llama.cpp/ggml/src/ggml-shave/kernels/prebuilt"
)
FIXTURES = NPUNLOCK_ROOT / "tests" / "fixtures" / "npu3720"

PROBE_INPUT = np.array(
    [-4.0, -2.0, -1.0, -0.5, 0.0, 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0],
    dtype=np.float16,
)


def carriers() -> dict[str, dict]:
    found = {
        "abs-add-1x16-tile1": {
            "blob": FIXTURES / "abs-add-1x16-tile1.blob",
            "inputs": 1,
            "elements_per_invocation": 8,
            "act_targets": 2,
            "act_groups": 1,
            "kind": "fixture-ramp",
        },
        "abs-abs-1x32": {
            "blob": FIXTURES / "abs-abs-1x32.blob",
            "inputs": 1,
            "elements_per_invocation": 16,
            "act_targets": 8,
            "act_groups": 2,
            "kind": "fixture-chained",
        },
    }
    for sidecar in sorted((HERE / "carriers").glob("*.json")):
        facts = json.loads(sidecar.read_text())
        found[facts["name"]] = {
            "blob": HERE / facts["blob"],
            "inputs": facts["ir_inputs"],
            "elements_per_invocation": facts["elements_per_invocation"],
            "act_targets": facts["act_targets"],
            "act_groups": facts["act_groups"],
            "shape": facts["shape"],
            "kind": f"ir-{facts['ir_op'].lower()}",
        }
    return found


CARRIERS = carriers()


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def native():
    if str(NPUNLOCK_ROOT / "python") not in sys.path:
        sys.path.insert(0, str(NPUNLOCK_ROOT / "python"))
    import npunlock as npu  # noqa: E402

    return npu.NativeLibraries(str(NPUNLOCK_BUILD))


def read_elf(name: str) -> bytes:
    if name.startswith("__prebuilt__"):
        path = PREBUILT / f"{name[len('__prebuilt__'):]}.elf"
    else:
        path = HERE / "elf" / f"{name}.elf"
    if not path.is_file():
        raise FileNotFoundError(f"kernel ELF missing: {path} (run build.sh first)")
    return path.read_bytes()


def patch_carrier(native_lib, carrier: bytes, plan: dict[int, bytes]) -> bytes:
    """Patch group_index -> ELF bytes; re-discover after each patch (the blob
    shifts), mirroring ggml-shave's tools/build_kernels.py."""
    blob = carrier
    for group_index, elf in sorted(plan.items()):
        groups = native_lib.discover_patch_targets(blob)
        if group_index >= len(groups):
            raise RuntimeError(f"carrier exposes {len(groups)} groups, need {group_index + 1}")
        blob = native_lib.patch_graph(blob, elf, groups[group_index]).graph_blob
    return blob


def case_inputs(case: dict) -> list[np.ndarray]:
    if "inputs" in case:
        return [np.asarray(v, dtype=np.float16) for v in case["inputs"]]
    return [case["input"]]


def execute(blob: bytes, inputs: list[np.ndarray], tag: str) -> dict:
    """Run the patched graph on the NPU with tools/klib_probe (any tensor size)."""
    if not KLIB_PROBE.is_file():
        raise FileNotFoundError(f"{KLIB_PROBE} missing (run build.sh)")
    WORK.mkdir(parents=True, exist_ok=True)
    blob_path = WORK / f"{tag}.blob"
    output_path = WORK / f"{tag}.out"
    report_path = WORK / f"{tag}.report.json"
    blob_path.write_bytes(blob)
    output_path.unlink(missing_ok=True)

    argv = [str(KLIB_PROBE), "--graph", str(blob_path)]
    input_paths = []
    for index, values in enumerate(inputs):
        path = WORK / f"{tag}.in{index}"
        path.write_bytes(values.tobytes())
        input_paths.append(path)
        argv += [f"--input-{index}", str(path)]
    argv += ["--output", str(output_path), "--report", str(report_path)]

    start = time.perf_counter()
    run = subprocess.run(argv, capture_output=True, text=True, timeout=300)
    seconds = time.perf_counter() - start
    report = json.loads(report_path.read_text()) if report_path.is_file() else {}
    actual = output_path.read_bytes() if output_path.is_file() else b""

    entry = {
        "executor": "tools/klib_probe (npunlock infer_engine.c)",
        "stages": report.get("stages", {}),
        "level_zero_result": report.get("result", {}).get("level_zero_result"),
        "actual_hex": actual.hex(),
        "actual_sha256": sha256(actual) if actual else None,
        "actual_f16": [float(v) for v in np.frombuffer(actual, dtype=np.float16)]
        if actual else [],
        "wall_seconds": round(seconds, 4),
        "probe_stdout": run.stdout[-600:],
        "probe_returncode": run.returncode,
    }
    if len(inputs[0]) * 2 == 32:
        bundle = WORK / f"bundle-{tag}"
        subprocess.run(["rm", "-rf", str(bundle)], check=False)
        write_bundle(bundle, blob, inputs[0].tobytes(), inputs[0].tobytes())
        dump = subprocess.run([str(DUMP_PROBE), "--bundle", str(bundle)],
                              capture_output=True, text=True, timeout=180)
        parsed = parse_dump(dump.stdout)
        entry["dump_probe"] = {
            "actual_hex": parsed.get("ACTUAL", ""),
            "agree_with_klib_probe": parsed.get("ACTUAL", "") == actual.hex(),
        }
    return entry


def write_bundle(directory: pathlib.Path, blob: bytes, input_data: bytes,
                 expected: bytes) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "graph.blob").write_bytes(blob)
    (directory / "input-0.bin").write_bytes(input_data)
    (directory / "expected-output-0.bin").write_bytes(expected)

    def record(name: str, data: bytes, argument_index: int) -> dict:
        return {"file": name, "size": len(data), "sha256": sha256(data),
                "argument_index": argument_index, "dtype": "f16",
                "shape": [1, len(data) // 2]}

    manifest = {
        "schema": "npunlock.porting.probe.v1",
        "purpose": "kernel-lib NPU evidence bundle",
        "producer": {"platform": "Linux x86_64 (kernel-lib tools/run_npu.py)",
                     "device": "Intel NPU (/dev/accel0)", "driver_version": "see report",
                     "graph_compiler_version": "vendor graph compiler (driver)"},
        "graph": {"file": "graph.blob", "size": len(blob), "sha256": sha256(blob)},
        "inputs": [record("input-0.bin", input_data, 0)],
        "expected_outputs": [record("expected-output-0.bin", expected, 1)],
        "oracle": {"name": "kernel-lib host reference", "input_argument_index": 0,
                   "output_argument_index": 1},
    }
    (directory / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")


def parse_dump(text: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for key in ("INPUT", "EXPECT", "ACTUAL"):
        match = re.search(rf"^{key}\s*:\s*(?:\(\d+\s+bytes\))?\s*([0-9a-f ]*)$", text, re.M)
        if match:
            out[key] = "".join(match.group(1).split())
    return out


def run_case(name: str, case: dict, repeat: int = 1) -> dict:
    native_lib = native()
    carrier_bytes = CARRIERS[case["carrier"]]["blob"].read_bytes()
    plan = {group: read_elf(elf) for group, elf in case["plan"].items()}
    blob = patch_carrier(native_lib, carrier_bytes, plan)
    range_changes = None
    if case.get("range_offsets"):
        # each range runs the entry at its offset; the extent follows the entry
        # size in the image (the device fetches [addend, addend + extent))
        table = patch_ranges.entry_table(read_elf(case["plan"][0]))
        extents = {}
        for group, offset in case["range_offsets"].items():
            if offset not in table:
                raise SystemExit(f"entry +{offset:#x} not found in the image: {sorted(table)}")
            extents[group] = table[offset][1]
        buffer = bytearray(blob)
        range_changes = patch_ranges.patch(buffer, case["range_offsets"], extents)
        blob = bytes(buffer)
    inputs = case_inputs(case)

    result: dict[str, object] = {
        "case": name,
        "description": case["description"],
        "carrier": case["carrier"],
        "carrier_facts": {k: v for k, v in CARRIERS[case["carrier"]].items() if k != "blob"},
        "carrier_sha256": sha256(carrier_bytes),
        "plan": {str(group): {"elf": elf, "sha256": sha256(read_elf(elf))}
                 for group, elf in case["plan"].items()},
        "graph_sha256": sha256(blob),
        "range_offsets": case.get("range_offsets"),
        "range_changes": range_changes,
        "input_f16": [float(v) for v in inputs[0][:16]],
        "inputs": [{"elements": int(v.size), "sha256": sha256(v.tobytes())} for v in inputs],
        "reference": case["reference_name"],
        "runs": [],
    }

    for attempt in range(repeat):
        expected = case["reference"](case)
        entry = execute(blob, inputs, f"{name}-{attempt}")
        actual = bytes.fromhex(entry["actual_hex"])
        entry["expected_hex"] = expected.hex()
        entry["expected_sha256"] = sha256(expected)
        entry["expected_f16"] = [float(v) for v in np.frombuffer(expected, dtype=np.float16)][:16]
        compare = case.get("compare")
        if actual and len(actual) == len(expected):
            a = np.frombuffer(actual, dtype=np.uint16)
            e = np.frombuffer(expected, dtype=np.uint16)
            sl = slice(*compare) if compare else slice(None)
            af = np.frombuffer(actual, dtype=np.float16).astype(np.float32)[sl]
            ef = np.frombuffer(expected, dtype=np.float16).astype(np.float32)[sl]
            finite = np.isfinite(af) & np.isfinite(ef)
            entry["compared_elements"] = int(finite.sum())
            entry["mismatched_elements"] = int(np.count_nonzero(a[sl] != e[sl]))
            entry["exact_match"] = bool(np.array_equal(a, e))
            entry["exact_match_compared_range"] = bool(np.array_equal(a[sl], e[sl]))
            entry["max_abs_error"] = float(np.max(np.abs(af[finite] - ef[finite]))) if finite.any() else None
            entry["compared_sha256"] = sha256(actual[2 * (sl.start or 0):2 * (sl.stop or len(actual) // 2)])
        else:
            entry["exact_match"] = False
            entry["exact_match_compared_range"] = False
        entry["attempt"] = attempt
        result["runs"].append(entry)

    runs = result["runs"]
    tolerance = case.get("tolerance")
    result["tolerance"] = tolerance
    result["verdict"] = {
        "executes": all(r["stages"].get("execution") == "passed" for r in runs),
        "exact_match": all(r.get("exact_match") for r in runs),
        "exact_match_compared_range": all(r.get("exact_match_compared_range") for r in runs),
        "max_abs_error": max((r.get("max_abs_error") or 0.0) for r in runs),
        "mismatched_elements": max(r.get("mismatched_elements", 0) for r in runs),
        "repeats": len(runs),
        "within_tolerance": all(
            (r.get("max_abs_error") is not None) and (r["max_abs_error"] <= tolerance)
            for r in runs
        ) if tolerance is not None else None,
        "consistent": len({r.get("compared_sha256", r["actual_sha256"]) for r in runs}) == 1,
        "mean_wall_seconds": round(float(np.mean([r["wall_seconds"] for r in runs])), 4),
    }
    return result


# --------------------------------------------------------------------------
# Host references (over the values the carrier delivers to its ACT)
# --------------------------------------------------------------------------


def act_input(case: dict) -> np.ndarray:
    """The exact fp16 values a carrier delivers to its ACT.

    Only abs-add-1x16-tile1 has a DPU pre-op: it adds the fixed fp16 ramp
    i/15-0.5 (npunlock/tools/porting_probe.py CARRIER_RAMP_VALUES).  The
    `copy16_ramp_model` case verifies that model element-by-element.  The IR
    carriers are verified byte-transparent by their `copy_*` cases.
    """
    values = case_inputs(case)[0]
    if case["carrier"] == "abs-add-1x16-tile1":
        ramp = np.array([np.float16(i / 15.0 - 0.5) for i in range(values.size)], dtype=np.float16)
        return (values + ramp).astype(np.float16)
    return values.astype(np.float16)


def ref_identity(case: dict) -> bytes:
    return act_input(case).tobytes()


def ref_plain_input(case: dict) -> bytes:
    return case_inputs(case)[0].tobytes()


def ref_add_packed_first_half(case: dict) -> bytes:
    values = act_input(case)
    half = values.size // 2
    out = values.copy()
    out[:half] = (values[:half].astype(np.float32) + values[half:].astype(np.float32)).astype(np.float16)
    return out.tobytes()


def ref_mul_then_copy(case: dict) -> bytes:
    """Multi-entry image: window 0 runs the packed multiply, window 1 the copy.

    A packed entry pairs lane i with lane i ^ count, so only a window that has a
    partner window *after* it can host one; here window 0 multiplies against
    window 1, and window 1 runs the elementwise copy entry.
    """
    values = act_input(case)
    half = values.size // 2
    first = values[:half].astype(np.float16) * values[half:].astype(np.float16)
    return np.concatenate([first, values[half:]]).astype(np.float16).tobytes()


def ref_mul_packed_first_half(case: dict) -> bytes:
    values = act_input(case)
    half = values.size // 2
    out = values.copy()
    out[:half] = (values[:half] * values[half:]).astype(np.float16)
    return out.tobytes()


def ref_binary(fn) -> "callable":
    def reference(case: dict) -> bytes:
        a, b = (v.astype(np.float32) for v in case_inputs(case)[:2])
        return fn(a, b).astype(np.float16).tobytes()

    return reference


def ref_f16_binary(fn) -> "callable":
    """fp16-arithmetic reference: each operation rounds to binary16, matching the
    kernel's own precision (numpy computes in fp32 and rounds once here)."""

    def reference(case: dict) -> bytes:
        a, b = case_inputs(case)[:2]
        return fn(a, b).tobytes()

    return reference


def ref_unary(fn, f16_math: bool = False) -> "callable":
    def reference(case: dict) -> bytes:
        x = act_input(case)
        if f16_math:
            return fn(x).astype(np.float16).tobytes()
        return fn(x.astype(np.float64)).astype(np.float32).astype(np.float16).tobytes()

    return reference


def silu(x: np.ndarray) -> np.ndarray:
    return (x / (1.0 + np.exp(-x.astype(np.float64)))).astype(np.float32)


def gelu_tanh(x: np.ndarray) -> np.ndarray:
    xd = x.astype(np.float64)
    w = np.sqrt(2.0 / np.pi) * (xd + 0.044715 * xd ** 3)
    return (0.5 * xd * (1.0 + np.tanh(w))).astype(np.float32)


def gelu_tanh_f16(x: np.ndarray) -> np.ndarray:
    """Same expression with every intermediate rounded to binary16 (the kernel's
    arithmetic; see gelu_f16.c)."""
    h = np.float16
    x = x.astype(h).astype(np.float32).astype(h)
    c1 = np.float16(0.044715)
    c2 = np.float16(0.7978845608)
    half = np.float16(0.5)
    one = np.float16(1.0)
    x3 = np.float16(np.float16(x * x) * x)
    w = np.float16(c2 * np.float16(x + np.float16(c1 * x3)))
    th = np.tanh(w.astype(np.float32)).astype(h)
    hx = np.float16(half * x)
    return np.float16(hx + np.float16(hx * th))


SILU_IDEAL = ref_unary(lambda x: silu(x))
GELU_IDEAL = ref_unary(gelu_tanh)
GELU_F16 = ref_unary(gelu_tanh_f16, f16_math=True)
ADD_F16 = ref_f16_binary(lambda a, b: (a.astype(np.float16) + b.astype(np.float16)))
MUL_F16 = ref_f16_binary(lambda a, b: (a.astype(np.float16) * b.astype(np.float16)))
SCALE_F16 = ref_binary(lambda a, b: a * b[0])


def gelu_inputs(n: int, dtype=np.float16) -> np.ndarray:
    """GELU needs x^3 to stay inside fp16 (the kernel evaluates x^3 in fp16)."""
    return np.linspace(-4.0, 4.0, n, dtype=np.float32).astype(dtype)


def tile_inputs(n: int, dtype=np.float16) -> list[np.ndarray]:
    """Deterministic, non-trivial operands for a 1xN tile pair."""
    x = (np.arange(1, n + 1, dtype=np.float32) / np.float32(7.0) - 3.0).astype(dtype)
    return [x]


def build_cases() -> dict[str, dict]:
    cases: dict[str, dict] = {}
    x16 = np.arange(1, 17, dtype=np.float16)
    x32 = np.arange(1, 33, dtype=np.float16)
    a16 = np.array([1., 2., 3., 4., 5., 6., 7., 8., 9., 10., 11., 12., 13., 14., 15., 16.],
                   dtype=np.float16)
    b16 = np.array([10., 20., 30., 40., 50., 60., 70., 80., 90., 100., 110., 120., 130., 140.,
                    150., 160.], dtype=np.float16)
    a8 = np.array([1., 2., 3., 4., 5., 6., 7., 8.], dtype=np.float16)
    b8 = np.array([10., 20., 30., 40., 50., 60., 70., 80.], dtype=np.float16)

    # --- prebuilt fixtures -------------------------------------------------
    cases["copy32"] = {
        "carrier": "abs-abs-1x32", "plan": {0: "copy_f16", 1: "copy_f16"},
        "input": x32, "reference": ref_plain_input,
        "reference_name": "the input tensor itself (byte-transparent copy)",
        "description": "1x32 carrier, copy_f16 in both groups: byte-transparent identity",
    }
    cases["add32_backend_prebuilt"] = {
        "carrier": "abs-abs-1x32",
        "plan": {0: "__prebuilt__shave_add", 1: "__prebuilt__shave_copy"},
        "input": np.concatenate([a16, b16]), "reference": ref_add_packed_first_half,
        "reference_name": "packed a+b in the first invocation window", "compare": (0, 16),
        "description": "ggml-shave pinned ELFs (bit-exact vs its CPU backend): packed f16 add",
    }
    cases["add32_lib"] = {
        "carrier": "abs-abs-1x32", "plan": {0: "add_f16_packed", 1: "copy_f16"},
        "input": np.concatenate([a16, b16]), "reference": ref_add_packed_first_half,
        "reference_name": "packed a+b in the first invocation window", "compare": (0, 16),
        "diff_against": "add32_backend_prebuilt",
        "description": "kernel-lib add_f16_packed + copy_f16 (16-element invocations)",
    }
    cases["copy16_ramp_model"] = {
        "carrier": "abs-add-1x16-tile1", "plan": {0: "copy_f16"},
        "input": PROBE_INPUT, "reference": ref_identity,
        "reference_name": "fp16(input + carrier ramp)",
        "description": "1x16 carrier copy_f16: verifies the ramp model act = fp16(input + i/15 - 0.5)",
    }
    cases["add16_packed_lib"] = {
        "carrier": "abs-add-1x16-tile1", "plan": {0: "add_f16_packed"},
        "input": np.concatenate([a8, b8]), "reference": ref_add_packed_first_half,
        "reference_name": "packed a+b over fp16(input + ramp), first 8-element window",
        "compare": (0, 8),
        "description": "the same add_f16_packed ELF on an 8-element invocation",
    }
    cases["multi4_pair32"] = {
        "carrier": "abs-abs-1x32", "plan": {0: "multi4"},
        "range_offsets": {0: 0x800, 2: 0x000},
        "input": np.concatenate([a16, b16]),
        "reference": ref_mul_then_copy,
        "reference_name": "packed product over lanes 0-15, echo of lanes 16-31",
        "description": "abs-abs-1x32: one image, range 0 = kb_mul_packed (+0x800), "
                       "range 2 = kb copy (+0x000)",
    }
    cases["silu16"] = {
        "carrier": "abs-add-1x16-tile1", "plan": {0: "silu_f16"},
        "input": PROBE_INPUT, "reference": SILU_IDEAL, "reference_name": SILU_IDEAL.__name__,
        "compare": (0, 8), "description": "silu_f16 on the 1x16 carrier",
    }
    cases["gelu16"] = {
        "carrier": "abs-add-1x16-tile1", "plan": {0: "gelu_f16"},
        "input": PROBE_INPUT, "reference": GELU_F16, "reference_name": GELU_F16.__name__,
        "compare": (0, 8), "tolerance": 5e-3,
        "description": "gelu_f16 (fp16 arithmetic) on the 1x16 carrier",
    }

    # --- IR-built carriers, one kernel per carrier -------------------------
    for n in (1024, 16384):
        elements = n
        x = tile_inputs(n)[0]
        suffix = f"{n}"
        cases[f"copy_abs{suffix}"] = {
            "carrier": f"abs_1in_1x{n}", "plan": {0: "copy_f16"},
            "input": x, "reference": ref_identity,
            "reference_name": "identity (byte-transparent copy)",
            "description": f"Abs(1,{n}) carrier (N={elements} elements): copy_f16",
        }
        half = n // 2
        # bounded operands: the packed multiply is evaluated in fp16
        base = np.linspace(-2.0, 2.0, half, dtype=np.float32).astype(np.float16)
        packed = np.concatenate([base, (base.astype(np.float32) * 1.5 + 0.25).astype(np.float16)])
        cases[f"add_packed_abs{suffix}"] = {
            "carrier": f"abs_1in_1x{n}", "plan": {0: "add_f16_packed"},
            # packed tile = [a | b]; the carrier's single input record holds both
            "input": packed,
            "reference": ref_add_packed_first_half,
            "reference_name": "packed a+b in the first invocation window",
            "compare": (0, n // 2),
            "description": f"Abs(1,{n}): packed f16 add over a {n}-element tile",
        }
        cases[f"mul_packed_abs{suffix}"] = {
            "carrier": f"abs_1in_1x{n}", "plan": {0: "mul_f16_packed"},
            "input": packed,
            "reference": ref_mul_packed_first_half,
            "reference_name": "packed product in the first invocation window",
            "compare": (0, n // 2),
            "description": f"Abs(1,{n}): packed f16 multiply over a {n}-element tile",
        }
        cases[f"multi4_abs{suffix}"] = {
            "carrier": f"abs_1in_1x{n}", "plan": {0: "multi4"},
            # ONE image; range 0 runs kb_mul_packed (+0x800), range 2 runs
            # kb_add_packed (+0x400) -- two different kernels per invocation
            "range_offsets": {0: 0x800, 2: 0x000},
            "input": packed,
            "reference": ref_mul_then_copy,
            "reference_name": "packed product over the first window, echo of the second",
            "description": f"Abs(1,{n}): one image, range 0 = kb_mul_packed (+0x800), "
                           f"range 2 = kb copy (+0x000)",
        }
        cases[f"silu_abs{suffix}"] = {
            "carrier": f"abs_1in_1x{n}", "plan": {0: "silu_f16"},
            "input": x, "reference": SILU_IDEAL, "reference_name": SILU_IDEAL.__name__,
            "description": f"Abs(1,{n}): silu_f16 over the whole tile",
        }
        cases[f"gelu_abs{suffix}"] = {
            "carrier": f"abs_1in_1x{n}", "plan": {0: "gelu_f16"},
            "input": gelu_inputs(n), "reference": GELU_F16, "reference_name": GELU_F16.__name__,
            "tolerance": 5e-3,   # fp16 arithmetic + mlibm tanhf vs the host reference
            "description": f"Abs(1,{n}): gelu_f16 (fp16 arithmetic) over the whole tile",
        }

    for n in (1024, 16384):
        suffix = f"{n}"
        pa = tile_inputs(n)[0]
        pb = (pa.astype(np.float32) * np.float32(-0.5) + np.float32(0.25)).astype(np.float16)
        cases[f"add_bin{suffix}"] = {
            "carrier": f"maximum_2in_1x{n}", "plan": {0: "add_f16"},
            "inputs": [pa, pb], "reference": ADD_F16, "reference_name": ADD_F16.__name__,
            "description": f"Maximum(1,{n}) 2-input carrier: add_f16 (a+b)",
        }
        cases[f"mul_bin{suffix}"] = {
            "carrier": f"maximum_2in_1x{n}", "plan": {0: "mul_f16"},
            "inputs": [pa, pb], "reference": MUL_F16, "reference_name": MUL_F16.__name__,
            "description": f"Maximum(1,{n}) 2-input carrier: mul_f16 (a*b)",
        }
        cases[f"scale_bin{suffix}"] = {
            "carrier": f"maximum_2in_1x{n}", "plan": {0: "scale_f16"},
            # the factor must be present in *every* invocation window (each
            # invocation sees record 1 at its own window base)
            "inputs": [pa, np.full(n, np.float16(1.5), dtype=np.float16)],
            "reference": SCALE_F16, "reference_name": SCALE_F16.__name__,
            "description": f"Maximum(1,{n}) 2-input carrier: scale_f16 (x * a[0])",
        }
    return cases


CASES = build_cases()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--repeat", type=int, default=1)
    args = parser.parse_args()

    if args.list:
        for name, case in CASES.items():
            print(f"{name:24s} {case['description']}")
        return 0

    selected = args.case or (list(CASES) if args.all else [])
    if not selected:
        parser.print_usage()
        return 2

    report: dict[str, object] = {"cases": {}}
    for name in selected:
        case = CASES[name]
        if not CARRIERS.get(case["carrier"], {}).get("blob", pathlib.Path("/nonexistent")).is_file():
            print(f"{name:24s} SKIP (carrier {case['carrier']} not built)")
            continue
        result = run_case(name, case, repeat=max(1, args.repeat))
        report["cases"][name] = result
        verdict = result["verdict"]
        print(f"{name:24s} executes={verdict['executes']} "
              f"exact={verdict['exact_match']} "
              f"range={verdict['exact_match_compared_range']} "
              f"tol={verdict.get('within_tolerance')} "
              f"maxerr={verdict['max_abs_error']:.3g} "
              f"wall={verdict['mean_wall_seconds']}s")

    for name, case in CASES.items():
        target = case.get("diff_against")
        if target and name in report["cases"] and target in report["cases"]:
            mine = report["cases"][name]["runs"][-1]["actual_hex"]
            theirs = report["cases"][target]["runs"][-1]["actual_hex"]
            report["cases"][name]["verdict"]["byte_identical_to"] = {
                "case": target, "identical": mine == theirs}
            print(f"{name:24s} byte_identical_to {target}: {mine == theirs}")

    out = HERE / "reports" / "npu-evidence.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    existing: dict = {"cases": {}}
    if out.is_file():
        existing = json.loads(out.read_text())
        existing.setdefault("cases", {})
    existing["cases"].update(report["cases"])
    existing["schema"] = "kernel-lib.npu-evidence.v1"
    existing["generated_by"] = "kernel-lib/tools/run_npu.py"
    out.write_text(json.dumps(existing, indent=2, sort_keys=True) + "\n")
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

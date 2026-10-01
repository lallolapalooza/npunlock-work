#!/usr/bin/env python3
"""kernel-lib manifest generator.

Merges three sources into kernel-lib/manifest.json:

  * static per-kernel contracts (op / dtype / ACT record layout / shape and
    element-count constraints / which carriers it patches),
  * the measured build status (build/status.tsv, written by build.sh),
  * the NPU execution evidence (reports/npu-evidence.json, written by
    tools/run_npu.py).

The `engine_ops` section mirrors the engine table ggml-shave's
tools/build_kernels.py consumes (name / class / lanes / stage0 / stage1), so the
backend can regenerate its table and its `kernels/prebuilt/` drop-in directory
from this manifest.

usage: make_manifest.py [KERNEL_LIB_ROOT]
"""

from __future__ import annotations

import hashlib
import json
import pathlib
import sys

ROOT = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parent.parent)

# --------------------------------------------------------------------------
# Carrier contracts (measured; see README.md "Carrier contract")
# --------------------------------------------------------------------------
def load_carriers() -> dict:
    """Carrier facts: the prebuilt npunlock fixtures plus every IR carrier built
    by tools/make_carrier.py (carriers/*.json), plus the shipped two-input
    carriers when npunlock has generated them."""
    import json as _json

    carriers = {
        "abs-abs-1x32": {
            "path": "npunlock/tests/fixtures/npu3720/abs-abs-1x32.blob",
            "sha256": "da8c205954f565c95845d9109558928d3c3e2c249c2c9942dc92b7ed60c4402c",
            "source": "prebuilt fixture (vendor graph compiler)",
            "ir_op": None, "shape": [1, 32], "dtype": "f16", "inputs": 1,
            "act_groups": 2, "act_targets": 8, "elements_per_invocation": 16,
            "load_bearing_ranges": [0, 2, 4, 6],
            "notes": "two chained ACT groups; each window pairs lane i with i ^ 16",
        },
        "abs-add-1x16-tile1": {
            "path": "npunlock/tests/fixtures/npu3720/abs-add-1x16-tile1.blob",
            "sha256": "d4abc3c09cabdb14fd5090fa5d2e4d33f4aa755aad2bcad4f078acc891d26e07",
            "source": "prebuilt fixture (vendor graph compiler)",
            "ir_op": None, "shape": [1, 16], "dtype": "f16", "inputs": 1,
            "act_groups": 1, "act_targets": 2, "elements_per_invocation": 8,
            "load_bearing_ranges": [0, 1],
            "notes": "DPU pre-op adds the fixed fp16 ramp i/15-0.5 before the ACT",
        },
    }
    for sidecar in sorted((ROOT / "carriers").glob("*.json")):
        facts = _json.loads(sidecar.read_text())
        carriers[facts["name"]] = {
            "path": facts["blob"],
            "sha256": hashlib.sha256((ROOT / facts["blob"]).read_bytes()).hexdigest(),
            "source": "IR-built on Linux (tools/make_carrier.py)",
            "ir_op": facts["ir_op"], "shape": facts["shape"], "dtype": facts["dtype"],
            "inputs": facts["ir_inputs"], "act_groups": facts["act_groups"],
            "act_targets": facts["act_targets"],
            "elements_per_invocation": facts["elements_per_invocation"],
            "load_bearing_ranges": [0, 2],
            "notes": "the ACT is the whole op; patching replaces it",
        }
    shipped = ROOT.parent / "npunlock" / "build" / "fresh" / "npu-carrier" / "two-input"
    if shipped.is_dir():
        for inspect_file in sorted(shipped.glob("*.inspect.json")):
            facts = _json.loads(inspect_file.read_text())
            act = facts["act"]
            name = inspect_file.name.replace(".inspect.json", "")
            blob = inspect_file.parent / f"{name}.blob"
            carriers[f"shipped:{name}"] = {
                "path": str(blob),
                "sha256": hashlib.sha256(blob.read_bytes()).hexdigest() if blob.is_file() else None,
                "source": "shipped npunlock path (PosixWorker npu_carrier_smoke.sh)",
                "ir_op": name.split("-")[0].capitalize(), "shape": None, "dtype": "f16",
                "inputs": 2, "act_groups": act["group_count"],
                "act_targets": act["target_count"],
                "elements_per_invocation": act["targets"][0]["element_count"],
                "load_bearing_ranges": [0, 2], "notes": "2 input records, output record 2",
            }
    return carriers


CARRIERS = load_carriers()

# --------------------------------------------------------------------------
# Per-kernel contracts
# --------------------------------------------------------------------------
# form:
#   unary          input 0 -> output 0
#   binary         input 0, input 1 -> output 2 (equal shapes)
#   unary_scalar   input 0 = vector, input 1 = tensor whose element 0 is the
#                  scalar factor, output 2
#   packed_pair    single input tile holding [a | b]; lane i pairs with i ^ count
#   reduce_rows    input 0 -> output 0, one reduction per dims32[0]-sized row
#   reduce_all     input 0, input 1 -> output 2 (single scalar element)
#   rope           input 0 = activations, input 1 = i32 positions, output 2
KERNELS = [
    dict(name="add_f16_packed", op="add", ggml=["GGML_OP_ADD"], dtype="f16",
         form="packed_pair", records=(1, 1), mlibm=False,
         constraints="dense, equal shapes; tile = [a | b], element_count = half tile",
         evidence=["add32_lib"],
         engine=dict(name="add", cls="binary", lanes=16, tile_elements=32,
                     stage0="add_f16_packed", stage1="copy_f16",
                     carrier="abs-abs-1x32")),
    dict(name="copy_f16", op="copy", ggml=["GGML_OP_DUP", "GGML_OP_CPY"], dtype="f16",
         form="unary", records=(1, 1), mlibm=False,
         constraints="dense; elementwise (second ACT stage of every engine blob)",
         evidence=["copy32"],
         engine=dict(name="copy", cls="unary", lanes=32, tile_elements=32,
                     stage0="copy_f16", stage1="copy_f16",
                     carrier="abs-abs-1x32")),
    dict(name="add_f16", op="add", ggml=["GGML_OP_ADD"], dtype="f16",
         form="binary", records=(2, 3), mlibm=False,
         constraints="dense, equal shapes (no broadcast)", evidence=[]),
    dict(name="add_f32", op="add", ggml=["GGML_OP_ADD"], dtype="f32",
         form="binary", records=(2, 3), mlibm=False,
         constraints="dense, equal shapes; FP32 *binary* carrier is not a validated "
                     "npunlock path (wiki/LIMITATIONS.md)", evidence=[]),
    dict(name="mul_f16", op="mul", ggml=["GGML_OP_MUL"], dtype="f16",
         form="binary", records=(2, 3), mlibm=False,
         constraints="dense, equal shapes; fp32 multiply register form needs the "
                     "asm fixup (see README)", evidence=[]),
    dict(name="mul_f32", op="mul", ggml=["GGML_OP_MUL"], dtype="f32",
         form="binary", records=(2, 3), mlibm=False,
         constraints="dense, equal shapes", evidence=[]),
    dict(name="scale_f16", op="scale", ggml=["GGML_OP_SCALE"], dtype="f16",
         form="unary_scalar", records=(2, 3), mlibm=False,
         constraints="input 1 must be a 1-element f16 tensor holding the factor",
         evidence=[]),
    dict(name="scale_f32", op="scale", ggml=["GGML_OP_SCALE"], dtype="f32",
         form="unary_scalar", records=(2, 3), mlibm=False,
         constraints="input 1 must be a 1-element f32 tensor holding the factor; "
                     "runtime scalar instead of ggml op_params", evidence=[]),
    dict(name="silu_f16", op="silu", ggml=["GGML_OP_SILU"], dtype="f16",
         form="unary", records=(1, 1), mlibm=True,
         constraints="dense; expf via mlibm (last bit may differ from host libm)",
         evidence=["silu16"]),
    dict(name="silu_f32", op="silu", ggml=["GGML_OP_SILU"], dtype="f32",
         form="unary", records=(1, 1), mlibm=True,
         constraints="dense; FP32 unary accuracy carrier is a validated path",
         evidence=[]),
    dict(name="gelu_f16", op="gelu", ggml=["GGML_OP_GELU"], dtype="f16",
         form="unary", records=(1, 1), mlibm=True,
         constraints="dense; tanh-approximation GELU", limitations="EXECUTED but numeric mismatch: GELU needs register-register fp32 multiplies (0.044715*x*x*x, 0.5*x*(1+w)), which the current toolchain does not compute (see measured_toolchain_rules D3).",
         evidence=["gelu16"]),
    dict(name="gelu_f32", op="gelu", ggml=["GGML_OP_GELU"], dtype="f32",
         form="unary", records=(1, 1), mlibm=True,
         constraints="dense; tanh-approximation GELU", limitations="Compiles; cannot be executed (no f32 carrier on this host) and is blocked by D3.",
         evidence=[]),
    dict(name="rms_norm_f16", op="rms_norm", ggml=["GGML_OP_RMS_NORM"], dtype="f16",
         form="reduce_rows", records=(1, 1), mlibm=True,
         constraints="rows of dims32[0]; eps pinned to 1e-5f", evidence=[]),
    dict(name="rms_norm_f32", op="rms_norm", ggml=["GGML_OP_RMS_NORM"], dtype="f32",
         form="reduce_rows", records=(1, 1), mlibm=True,
         constraints="rows of dims32[0]; eps pinned to 1e-5f", evidence=[]),
    dict(name="softmax_f16", op="softmax", ggml=["GGML_OP_SOFT_MAX"], dtype="f16",
         form="reduce_rows", records=(1, 1), mlibm=True,
         constraints="rows of dims32[0]; no logit scale / ALiBi (not carried by the ABI)",
         evidence=[]),
    dict(name="softmax_f32", op="softmax", ggml=["GGML_OP_SOFT_MAX"], dtype="f32",
         form="reduce_rows", records=(1, 1), mlibm=True,
         constraints="rows of dims32[0]; no logit scale / ALiBi", evidence=[]),
    dict(name="rope_f16", op="rope", ggml=["GGML_OP_ROPE"], dtype="f16",
         form="rope", records=(2, 3), mlibm=True,
         constraints="NEOX layout; n_dims = dims32[0]; pos in input record 1",
         evidence=[]),
    dict(name="rope_f32", op="rope", ggml=["GGML_OP_ROPE"], dtype="f32",
         form="rope", records=(2, 3), mlibm=True,
         constraints="NEOX layout; n_dims = dims32[0]; pos in input record 1",
         evidence=[]),
    dict(name="vec_dot_f16", op="vec_dot", ggml=["GGML_OP_MUL_MAT"], dtype="f16",
         form="reduce_all", records=(2, 3), mlibm=False,
         constraints="full reduction over the invocation chunk; output record holds 1 element",
         evidence=[]),
    dict(name="vec_dot_f32", op="vec_dot", ggml=["GGML_OP_MUL_MAT"], dtype="f32",
         form="reduce_all", records=(2, 3), mlibm=False,
         constraints="full reduction over the invocation chunk; output record holds 1 element",
         evidence=[]),
]


MEASURED_TOOLCHAIN_RULES = [
    {"id": "D1", "rule": "fp32 ADD/SUB with two *runtime* register operands is computed "
                         "correctly only with the '; fmt=2' register-form asm hint",
     "evidence": "add_f16_packed is byte-exact on the NPU with the hint and returns "
                 "in[i] + <hardware constant> without it (reports/npu-evidence.json "
                 "case add32_lib vs the unfixed build)"},
    {"id": "D2", "rule": "fp32 ADD/MUL whose operand is a hardware-selector literal "
                         "(0.5, 1.0, 2.0, 3.0, 1.41494, 0.70604, pi, e, 10.0) is computed "
                         "correctly only WITHOUT the hint (the compiler chooses the "
                         "constant-selector encoding deliberately)",
     "evidence": "silu_f16 (1.0f + expf(-x)) is bit-exact with FIXUP=off and wrong with "
                 "FIXUP=on; x + 1.0f returns x+1 unfixed and x fixed"},
    {"id": "D3", "rule": "fp32 MUL of two runtime registers does not compute the product in "
                         "either mode (returns one of the operands): x*x returns x, "
                         "in[i]*in[i^16] returns the partner",
     "evidence": "throwaway probes k_rrmul.c / k_mulrr.c, both fixup modes"},
    {"id": "D4", "rule": "mlibm CALLs work: expf and tanhf are bit-exact/1-ULP on the NPU",
     "evidence": "silu_f16 executes bit-exact (mismatched_elements = 0) and the tanhf/expf "
                 "probes return the correctly rounded values"},
    {"id": "D5", "rule": "the v1 porting-probe contract (npunlock-linux-probe and tmpdump "
                         "dump_probe) only accepts 32-byte tensors, i.e. a 1x16 fp16 carrier",
     "evidence": "src/linux_probe/main.c:248; tools/klib_probe.c exists to run any size"},
]


LATENCY = {
    "provenance": (
        "Per-invocation cost with a *persistent* runtime session, measured by the "
        "npu-runtime workstream (/home/asdf/projects/npunlock-work/npu-runtime/"
        "README.md, build/npu-runtime-bench --runs 200); kernel-lib's own probe "
        "(tools/klib_probe, fresh process, one graph create per call) measures "
        "26-76 ms per run, which is the graph-recreate class, not kernel time."
    ),
    "graph_execute_and_fence_wait_ms_per_invocation": 0.80,
    "steady_state_ms_per_call_reused_session": [1.00, 1.22],
    "graph_recreate_per_call_ms": 46,
    "kernel_lib_probe_wall_seconds": "see reports/npu-evidence.json verdict.mean_wall_seconds",
    "note": (
        "The numbers are per invocation of the whole patched graph, not per "
        "element: a 32-element tile costs the same as a 1-element tile, which is "
        "why larger carriers (graph compiler) are the lever, not kernel tuning."
    ),
}


# Evidence cases added after the first cut (IR carriers, multi-entry image).
EXTRA_EVIDENCE = {
    "add_f16_packed": ["add_packed_abs1024", "add_packed_abs16384", "multi4_abs1024"],
    "mul_f16_packed": ["mul_packed_abs1024", "mul_packed_abs16384", "multi4_abs1024"],
    "scale_f16_packed": [],
    "copy_f16": ["copy_abs1024", "copy_abs16384", "multi4_abs32", "multi4_pair32"],
    "silu_f16": ["silu_abs1024", "silu_abs16384"],
    "gelu_f16": ["gelu_abs1024", "gelu_abs16384"],
    "add_f16": ["add_bin1024", "add_bin16384"],
    "mul_f16": ["mul_bin1024", "mul_bin16384"],
    "scale_f16": ["scale_bin1024", "scale_bin16384"],
    "multi4": ["multi4_pair32", "multi4_abs1024", "multi4_abs16384"],
}

# Element counts each kernel has actually been executed at, per carrier.
EXTRA_CONTRACTS = [
    dict(name="mul_f16_packed", op="mul", ggml=["GGML_OP_MUL"], dtype="f16",
         form="packed_pair", records=(1, 1), mlibm=False,
         constraints="dense, equal shapes; tile = [a | b]; fp16 arithmetic",
         evidence=[]),
    dict(name="scale_f16_packed", op="scale", ggml=["GGML_OP_SCALE"], dtype="f16",
         form="packed_pair", records=(1, 1), mlibm=False,
         constraints="tile = [x | factor], the factor in lane `element_count`",
         evidence=[]),
    dict(name="multi4", op="multi", ggml=["GGML_OP_ADD", "GGML_OP_MUL", "GGML_OP_DUP",
                                          "GGML_OP_SILU"], dtype="f16",
         form="multi_entry", records=(1, 1), mlibm=True,
         constraints="one image, four entries 0x400 apart (copy/add/mul/silu); "
                     "one ACT range runs one entry via tools/patch_ranges.py",
         evidence=[]),
]

FORM_SUMMARY = {
    "unary": "y[i] = f(x[i]) over invocation.element_count elements",
    "binary": "y[i] = f(a[i], b[i]); both inputs must have identical static shapes",
    "unary_scalar": "y[i] = x[i] * a[0] (runtime factor travels in input record 1)",
    "packed_pair": "y[i] = in[i] + in[i ^ element_count]; host packs [a | b] into one tile",
    "reduce_rows": "one reduction per row of dims32[0] elements, rows walked to element_count",
    "reduce_all": "out[0] = reduce over the whole invocation chunk",
    "multi_entry": "one image, several named entries 0x400 apart; a range runs one entry",
    "rope": "NEOX rotary embedding, one row per dims32[0] elements",
}


KERNELS.extend(EXTRA_CONTRACTS)


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_status() -> dict[str, dict]:
    status_dir = ROOT / "build" / "status"
    rows: dict[str, dict] = {}
    if not status_dir.is_dir():
        return rows
    for path in sorted(status_dir.glob("*.tsv")):
        lines = path.read_text().splitlines()
        if len(lines) < 2:
            continue
        header = lines[0].split("\t")
        values = lines[1].split("\t")
        row = dict(zip(header, values))
        rows[row["name"]] = row
    return rows


def multi_entry_offsets(elf_path: pathlib.Path) -> list[dict]:
    """{name, offset, size, aligned} for each function symbol in .text."""
    import struct as _struct

    if not elf_path.is_file():
        return []
    data = elf_path.read_bytes()
    shoff = _struct.unpack_from("<I", data, 0x20)[0]
    shentsize = _struct.unpack_from("<H", data, 0x2E)[0]
    shnum = _struct.unpack_from("<H", data, 0x30)[0]
    shstrndx = _struct.unpack_from("<H", data, 0x32)[0]
    secs = []
    for index in range(shnum):
        off = shoff + index * shentsize
        name, kind, flags, addr, file_off, size, link, info, align, entsize = _struct.unpack_from(
            "<IIIIIIIIII", data, off)
        secs.append(dict(index=index, nameoff=name, type=kind, addr=addr, off=file_off,
                         size=size, link=link, entsize=entsize))
    strings = secs[shstrndx]
    for sec in secs:
        end = data.index(b"\0", strings["off"] + sec["nameoff"])
        sec["name"] = data[strings["off"] + sec["nameoff"]:end].decode()
    text = next((s for s in secs if s["name"] == ".text"), None)
    symtab = next((s for s in secs if s["type"] == 2), None)
    if text is None or symtab is None:
        return []
    symstr = secs[symtab["link"]]
    out = []
    for index in range(symtab["size"] // symtab["entsize"]):
        off = symtab["off"] + index * symtab["entsize"]
        st_name, st_value, st_size, st_info, st_other, st_shndx = _struct.unpack_from(
            "<IIIBBH", data, off)
        if st_shndx != text["index"] or (st_info & 0xF) != 2 or st_size == 0:
            continue
        end = data.index(b"\0", symstr["off"] + st_name)
        sname = data[symstr["off"] + st_name:end].decode() if st_name else ""
        if not sname or sname.startswith("__"):
            continue
        offset = st_value - text["addr"]
        out.append({"name": sname, "offset": offset, "offset_hex": hex(offset),
                    "size": st_size, "aligned_0x400": offset % 0x400 == 0})
    return out


def main() -> int:
    status = read_status()
    evidence_path = ROOT / "reports" / "npu-evidence.json"
    evidence = json.loads(evidence_path.read_text()) if evidence_path.is_file() else {"cases": {}}

    kernels = []
    for contract in KERNELS:
        name = contract["name"]
        row = status.get(name, {})
        elf_path = ROOT / "elf" / f"{name}.elf"
        validate = row.get("validate", "SKIP")
        compile_state = row.get("compile", "MISSING")
        text_bytes = row.get("text_bytes", "-")

        entry: dict = {
            "name": name,
            "op": contract["op"],
            "ggml_ops": contract["ggml"],
            "dtype": contract["dtype"],
            "abi_form": contract["form"],
            "abi_summary": FORM_SUMMARY.get(contract["form"], ""),
            "record_indices": {"inputs": list(range(contract["records"][0])),
                               "output": contract["records"][1] - 1},
            "links_mlibm": contract["mlibm"],
            "constraints": contract["constraints"],
            "source": f"src/{name}.c",
            "source_sha256": sha256(ROOT / "src" / f"{name}.c"),
            "elf": f"elf/{name}.elf" if elf_path.is_file() else None,
            "elf_sha256": row.get("sha256") if row.get("sha256") != "-" else None,
            "text_bytes": int(text_bytes) if text_bytes.isdigit() else None,
            "build": {
                "compile": compile_state,
                "assemble": row.get("asm", "-"),
                "link": row.get("link", "-"),
                "validate": validate,
                "message": row.get("first_message", ""),
            },
            "status": (
                "COMPILE_FAIL" if compile_state != "OK"
                else "VALIDATED" if validate == "OK"
                else "COMPILED_UNVALIDATED"
            ),
        }
        if contract.get("engine"):
            entry["engine"] = contract["engine"]
        runs = []
        for case_name in list(contract.get("evidence", [])) + EXTRA_EVIDENCE.get(name, []):
            case = evidence["cases"].get(case_name)
            if case is None:
                continue
            last = case["runs"][-1]
            runs.append({
                "case": case_name,
                "carrier": case["carrier"],
                "plan": case["plan"],
                "executes": last["stages"].get("execution") == "passed",
                "exact_match": last.get("exact_match"),
                "within_tolerance": case["verdict"].get("within_tolerance"),
                "tolerance": case.get("tolerance"),
                "exact_match_compared_range": last.get("exact_match_compared_range"),
                "compared_elements": last.get("compared_elements"),
                "mismatched_elements": last.get("mismatched_elements"),
                "max_abs_error": last.get("max_abs_error"),
                "mean_wall_seconds": case["verdict"]["mean_wall_seconds"],
                "byte_identical_to": case["verdict"].get("byte_identical_to"),
                "evidence": "reports/npu-evidence.json",
            })
        if runs:
            entry["execution"] = runs
            first = runs[0]
            if not first["executes"]:
                entry["status"] = "EXECUTION_FAILED"
            elif first["exact_match"] or first.get("exact_match_compared_range"):
                entry["status"] = "EXECUTED_VERIFIED"
            elif case["verdict"].get("within_tolerance"):
                entry["status"] = "EXECUTED_VERIFIED_TOLERANT"
            else:
                entry["status"] = "EXECUTED_MISMATCH"
        kernels.append(entry)

    engine_ops = [k["engine"] for k in kernels if "engine" in k]

    manifest = {
        "schema": "kernel-lib.manifest.v1",
        "generated_by": "kernel-lib/build.sh + kernel-lib/tools/make_manifest.py",
        "abi": {
            "entry": "controlled_act",
            "signature": "void controlled_act(unsigned layerParams)",
            "header": "src/npunlock/npu3720_kernel.h",
            "header_sha256": sha256(ROOT / "src" / "npunlock" / "npu3720_kernel.h"),
            "linker_script": "src/shave_kernel.ld",
            "linker_script_sha256": sha256(ROOT / "src" / "shave_kernel.ld"),
            "invocation_fields": [
                "params32 + 0x00 = input record 0 data pointer",
                "params32 + 0x0c = rank", "params32 + 0x10 = dims pointer",
                "params32 + 0x28 * record = record data pointer (0x28-byte records)",
                "element_count = product(dims[0..rank))",
            ],
            "target_triple": "shave / 3720xx",
        },
        "toolchain": (ROOT / "build" / "toolchain-stamp.txt").read_text().splitlines()
        if (ROOT / "build" / "toolchain-stamp.txt").is_file() else [],
        "carriers": CARRIERS,
        "latency": LATENCY,
        "measured_toolchain_rules": MEASURED_TOOLCHAIN_RULES,
        "engine_ops": engine_ops,
        "kernels": kernels,
        "summary": {
            "kernels": len(kernels),
            "compiled_and_validated": sum(1 for k in kernels if k["status"] == "VALIDATED"),
            "executed_verified_tolerant": sum(
                1 for k in kernels if k["status"] == "EXECUTED_VERIFIED_TOLERANT"),
            "executed_verified": sum(1 for k in kernels if k["status"] == "EXECUTED_VERIFIED"),
            "executed_mismatch": sum(1 for k in kernels if k["status"] == "EXECUTED_MISMATCH"),
            "compile_fail": sum(1 for k in kernels if k["status"] == "COMPILE_FAIL"),
        },
    }
    (ROOT / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=False) + "\n")

    # Engine table in the exact shape ggml-shave's tools/build_kernels.py consumes
    # (ENGINE_OPS): drop-in replacement for that tuple.
    lines = ["# generated by kernel-lib/tools/make_manifest.py from manifest.json",
             "# ggml-shave: replace ENGINE_OPS in tools/build_kernels.py with this tuple,",
             "# then build with --kernels <kernel-lib>/src --prebuilt <kernel-lib>/elf.",
             "ENGINE_OPS = ("]
    for op in engine_ops:
        lines.append("    {")
        lines.append(f'        "name": "{op["name"]}",')
        lines.append(f'        "class": "{op["cls"]}",')
        lines.append(f'        "lanes": {op["lanes"]},')
        lines.append(f'        "stage0": "{op["stage0"]}.c",')
        lines.append(f'        "stage1": "{op["stage1"]}.c",')
        lines.append(f'        # carrier: {op["carrier"]} (patched group 0 = stage0, group 1 = stage1)')
        lines.append("    },")
    lines.append(")")
    (ROOT / "engine-ops.py").write_text("\n".join(lines) + "\n")
    print(f"manifest.json: {manifest['summary']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

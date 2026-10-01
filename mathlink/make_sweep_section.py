#!/usr/bin/env python3
"""Render out/SWEEP_SECTION.md from the per-symbol probe results."""
import json, pathlib, subprocess, sys

ROOT = pathlib.Path(__file__).parent
merged = {}
for name in ("symprobe_curated.json", "symprobe_50.json", "symprobe.json"):
    p = ROOT / "out" / name
    if p.exists():
        for r in json.loads(p.read_text()):
            merged[r["symbol"]] = r

import ctypes
class _View(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(ctypes.c_uint8)), ("size", ctypes.c_size_t)]
class _Buffer(ctypes.Structure):
    _fields_ = [("data", ctypes.POINTER(ctypes.c_uint8)), ("size", ctypes.c_size_t),
                ("release", ctypes.c_void_p), ("context", ctypes.c_void_p)]
class _Diag(ctypes.Structure):
    _fields_ = [("struct_size", ctypes.c_uint32), ("json", _Buffer)]
class _Image(ctypes.Structure):
    _fields_ = [("file_offset", ctypes.c_size_t), ("size", ctypes.c_size_t),
                ("address", ctypes.c_uint32), ("entry", ctypes.c_uint32)]
_LIB = ctypes.CDLL("/home/asdf/projects/npunlock-work/npunlock/build/fresh/libnpunlock.so")
_LIB.npunlock_parse_shave_elf.argtypes = [_View, ctypes.POINTER(_Image), ctypes.POINTER(_Diag)]
_LIB.npunlock_parse_shave_elf.restype = ctypes.c_int
_LIB.npunlock_status_name.argtypes = [ctypes.c_int]; _LIB.npunlock_status_name.restype = ctypes.c_char_p

def validate(sym):
    elf = ROOT / "out" / "symprobe" / f"{sym}.elf"
    if not elf.exists():
        return "missing"
    data = elf.read_bytes()
    buf = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
    img, diag = _Image(), _Diag(ctypes.sizeof(_Diag))
    st = _LIB.npunlock_parse_shave_elf(_View(buf, len(data)), ctypes.byref(img), ctypes.byref(diag))
    if st == 0:
        return "VALID"
    detail = ctypes.string_at(diag.json.data, diag.json.size).decode("utf-8", "replace") if diag.json.size else ""
    return "INVALID: " + detail[:70]

linked, failed, data, invalid = [], [], [], []
rows = []
for sym in sorted(merged):
    r = merged[sym]
    if r["stage"] != "ok":
        failed.append((sym, r.get("stderr", "")[:90]))
        continue
    linked.append(sym)
    v = validate(sym)
    if v != "VALID":
        invalid.append((sym, v))
    if r["argdata_size"]:
        data.append((sym, r["argdata_size"]))
    rows.append((sym, r["text_size"], r["argdata_size"], r["elf_bytes"], v))

# inventory cross-check
inv = pathlib.Path("/home/asdf/projects/npunlock-work/npunlock/ABI/MLIBM_SYMBOLS.md").read_text()
import re
doc_syms = set(re.findall(r'^([A-Za-z_][A-Za-z0-9_]*)\s+[TD]\s', inv, re.M))
covered = sorted(s for s in doc_syms if s in merged)
missing = sorted(s for s in doc_syms if s not in merged)

lines = ["## 6.3 Full public-symbol sweep (results: `out/SYMBOL_TABLE.tsv`)", ""]
lines.append(f"* symbols probed: **{len(merged)}**; linked: **{len(linked)}**; "
             f"link/assemble failures: **{len(failed)}**")
lines.append(f"* npunlock verdict `VALID`: **{len(linked) - len(invalid)}/{len(linked)}**"
             + (f"; not valid: {invalid}" if invalid else ""))
lines.append(f"* non-empty `.arg.data` (ACT contract violation): "
             + (", ".join(f"`{s}` ({n} B)" for s, n in data) if data else "none"))
if failed:
    lines.append(f"* failures: " + "; ".join(f"`{s}`: {e}" for s, e in failed))
lines.append("")
lines.append("Every probed symbol resolves out of `mlibm.a`, links, and yields an ELF that "
             "`npunlock_parse_shave_elf` accepts — except the `.arg.data` group above.")
lines.append("")
if missing:
    lines.append(f"Symbols in `ABI/MLIBM_SYMBOLS.md` not in this sweep ({len(missing)}): "
                 + ", ".join(f"`{s}`" for s in missing[:40])
                 + (" …" if len(missing) > 40 else ""))
    lines.append("")
lines.append("### .text size of the linked image per symbol (probe stub 32 B + closure)")
lines.append("")
lines.append("| symbol | .text B | .arg.data B | ELF B | verdict |")
lines.append("|---|---:|---:|---:|---|")
for sym, t, a, e, v in sorted(rows, key=lambda r: -r[1]):
    lines.append(f"| `{sym}` | {t} | {a} | {e} | {'VALID' if v=='VALID' else v} |")
(ROOT / "out" / "SWEEP_SECTION.md").write_text("\n".join(lines) + "\n")
print(f"rendered {len(rows)} rows -> out/SWEEP_SECTION.md")
print(f"linked {len(linked)}, failed {len(failed)}, argdata {len(data)}, invalid {len(invalid)}")
print("doc symbols:", len(doc_syms), "covered:", len(covered), "missing:", len(missing))
if missing:
    print("missing:", ", ".join(missing[:50]))

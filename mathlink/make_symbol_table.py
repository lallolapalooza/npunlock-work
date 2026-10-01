#!/usr/bin/env python3
"""Fold the per-symbol probe JSON files into out/SYMBOL_TABLE.tsv (symbol, .text, .arg.data,
elf bytes, verdict)."""
import json, pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).parent))
import subprocess

ROOT = pathlib.Path(__file__).parent
merged = {}
for name in ("symprobe_curated.json", "symprobe_50.json", "symprobe.json"):
    p = ROOT / "out" / name
    if not p.exists():
        continue
    for r in json.loads(p.read_text()):
        merged[r["symbol"]] = r
rows = ["symbol\ttext_bytes\targdata_bytes\telf_bytes\tstage\tvalidator"]
for sym in sorted(merged):
    r = merged[sym]
    if r["stage"] != "ok":
        rows.append(f"{sym}\t-\t-\t-\t{r['stage']}\t{r.get('stderr','').replace(chr(9),' ')[:120]}")
        continue
    elf = ROOT / "out" / "symprobe" / f"{sym}.elf"
    v = subprocess.run(["python3", str(ROOT / "validate_elf.py"), str(elf)],
                       capture_output=True, text=True).stdout.strip()
    verdict = "VALID" if v.startswith("VALID") else v.split(": ", 1)[-1][:80]
    rows.append(f"{sym}\t{r['text_size']}\t{r['argdata_size']}\t{r['elf_bytes']}\tok\t{verdict}")
(ROOT / "out" / "SYMBOL_TABLE.tsv").write_text("\n".join(rows) + "\n")
print(f"{len(merged)} symbols -> out/SYMBOL_TABLE.tsv")

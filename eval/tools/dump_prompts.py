#!/usr/bin/env python3
"""Materialise the frozen prompt set as individual prompt files.

Writes ``<out-dir>/<id>.txt`` with the exact prompt bytes (no added newline, so
llama-cli sees precisely the frozen prompt) and prints one TSV row per prompt on
stdout: ``id<TAB>kind<TAB>n_predict``.

    dump_prompts.py --promptset eval/prompts.json --out-dir /tmp/run/prompts
"""

from __future__ import annotations

import argparse
import json
import pathlib


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--promptset", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--only", default="",
                        help="comma-separated prompt ids to keep")
    args = parser.parse_args()

    promptset = json.loads(pathlib.Path(args.promptset).read_text())
    defaults = promptset.get("defaults", {})
    keep = {item for item in args.only.split(",") if item}

    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    known = set()
    for spec in promptset["prompts"]:
        if keep and spec["id"] not in keep:
            continue
        known.add(spec["id"])
        n_predict = int(spec.get("n_predict", defaults.get("n_predict", 32)))
        (out_dir / f"{spec['id']}.txt").write_bytes(spec["prompt"].encode("utf-8"))
        print(f"{spec['id']}\t{spec['kind']}\t{n_predict}")

    unknown = keep - known
    if unknown:
        raise SystemExit(f"unknown prompt ids: {sorted(unknown)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Repeat one prompt K times under each of several `-dev` settings and report
which runs produce byte-identical text.

This is the experiment behind the "outputs must match exactly" claim:

  * repeats within one device prove (or refute) that the mode is deterministic,
  * equality across devices tells whether *selecting* an extra backend changes
    the result at all, even when it offloads nothing.

Usage:
  ab_probe.py --model M --prompt-file F --n 96 --repeats 2 \
              --devices none,SHAVE0 --out ab.json
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cli_run  # noqa: E402


def run_once(opts, device, index, work: pathlib.Path) -> dict:
    namespace = argparse.Namespace(
        prompt_file=opts.prompt_file,
        prompt=opts.prompt,
        binary=opts.binary,
        model=opts.model,
        n=opts.n,
        ctx=opts.ctx,
        threads=opts.threads,
        seed=opts.seed,
        device=device,
        batch=opts.batch,
        ubatch=opts.ubatch,
        extra=[],
        timeout=opts.timeout,
        cwd=opts.cwd,
        out=str(work / f"{device}.{index}.json"),
        id=f"{device}-{index}",
    )
    return cli_run.run(namespace)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", required=True)
    parser.add_argument("--prompt-file", required=True)
    parser.add_argument("--devices", default="none,SHAVE0")
    parser.add_argument("--n", type=int, default=96)
    parser.add_argument("--ctx", type=int, default=4096)
    parser.add_argument("--threads", type=int, default=8)
    parser.add_argument("--batch", type=int, default=2048)
    parser.add_argument("--ubatch", type=int, default=512)
    parser.add_argument("--seed", type=int, default=1234)
    parser.add_argument("--repeats", type=int, default=2)
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--binary", default="./build/bin/llama-cli")
    parser.add_argument("--cwd", default=None)
    parser.add_argument("--work-dir", default="/tmp/eval-ab-probe")
    parser.add_argument("--out")
    opts = parser.parse_args()

    work = pathlib.Path(opts.work_dir)
    work.mkdir(parents=True, exist_ok=True)
    devices = [item for item in opts.devices.split(",") if item]

    results = collections.OrderedDict()
    for device in devices:
        entries = []
        for index in range(opts.repeats):
            record = run_once(opts, device, index, work)
            text = record.get("generated_text") or ""
            entry = {
                "index": index,
                "md5": hashlib.md5(text.encode()).hexdigest(),
                "chars": len(text),
                "prompt_tps": record.get("prompt_tps"),
                "gen_tps": record.get("gen_tps"),
                "ok": record.get("ok"),
                "error_lines": record.get("error_lines", []),
                "text": text,
            }
            entries.append(entry)
            print(f"  {device} #{index}: {entry['chars']} chars md5={entry['md5'][:12]} "
                  f"gen={entry['gen_tps']} ok={entry['ok']}", flush=True)
        results[device] = entries

    report = {"schema": "npunlock.eval.ab-probe.v1", "prompt_file": opts.prompt_file,
              "n_predict": opts.n, "ctx": opts.ctx, "threads": opts.threads,
              "devices": results}
    report["analysis"] = {}
    for device, entries in results.items():
        digests = {entry["md5"] for entry in entries}
        report["analysis"][device] = {
            "self_deterministic": len(digests) == 1,
            "distinct_outputs": len(digests),
        }
    digests_per_device = {device: {e["md5"] for e in entries}
                          for device, entries in results.items()}
    report["analysis"]["all_modes_agree"] = (
        len({frozenset(d) for d in digests_per_device.values()}) == 1)

    print("\nper-device determinism :",
          {d: v["self_deterministic"] for d, v in report["analysis"].items()
           if isinstance(v, dict)})
    print("all modes agree        :", report["analysis"]["all_modes_agree"])

    if opts.out:
        pathlib.Path(opts.out).write_text(json.dumps(report, indent=2) + "\n")
        print(f"wrote {opts.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

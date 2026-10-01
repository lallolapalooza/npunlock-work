#!/usr/bin/env python3
"""Run one llama-cli prompt deterministically and record a machine-readable result.

This is the single-prompt primitive used by eval/run_compare.sh.  It owns the
whole "how do we bound and parse llama-cli" contract, so the harness never has
to rely on exit codes or fragile shell parsing:

  * the process is bounded by --timeout seconds *and* by -n (the chat build
    enters an interactive loop otherwise; stdin is /dev/null so EOF also ends it),
  * timing comes from llama-cli's own ``[ Prompt: P t/s | Generation: G t/s ]``
    line (printed on stdout, not stderr),
  * the generated text is the span between the ``> <prompt>`` echo and that line,
  * GGML_SHAVE_STATS=1 makes the NPU backend print
    ``ggml-shave-stats {...}`` on stderr at exit; the parsed object is recorded.

Usage:
  cli_run.py --model M --prompt TEXT --n 32 --device none  --id arith --out r.json
  cli_run.py --model M --prompt-file F --n 24 --device SHAVE0 --id longctx --out r.json
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
TIMING = re.compile(r"\[ Prompt:\s*([0-9.]+)\s*t/s \| Generation:\s*([0-9.]+)\s*t/s \]")
STATS = re.compile(r"ggml-shave-stats (\{.*\})")
# llama-cli prints its banner and the "available commands:" help before the first
# turn; the generation is everything between that block and the timing line.
BANNER_END = "/glob <pattern>     add text files using globbing pattern"
ERROR_LINE = re.compile(r"^\s*(?:error|Error|ERROR|llama-cli: error|failed|Failed)[: ].*$",
                        re.MULTILINE)


def strip_ansi(text: str) -> str:
    return ANSI.sub("", text)


def extract_generated(stdout: str, prompt: str) -> str:
    """Return the completion text of a llama-cli chat-mode run.

    Preferred anchor: llama-cli echoes the prompt as ``> <prompt>``; everything
    up to the timing line (or ``Exiting...``) is the completion.  llama-cli
    truncates the echo for long prompts, so when the anchor is absent fall back
    to the banner boundary, which is where the first turn starts.
    """
    anchor_marker = "> " + prompt
    anchor = stdout.rfind(anchor_marker)
    used_anchor = anchor >= 0
    if used_anchor:
        start = anchor + len(anchor_marker)
    else:
        banner = stdout.rfind(BANNER_END)
        start = banner + len(BANNER_END) if banner >= 0 else 0

    # End of the completion: the blank line in front of the timing line, else
    # the blank line in front of "Exiting...".
    text = None
    for end_marker in ("\n\n[ Prompt:", "\n\nExiting"):
        index = stdout.find(end_marker, start)
        if index >= 0:
            text = stdout[start:index]
            break
    if text is None:
        text = stdout[start:]

    # When the echo was too long llama-cli cut it and marked the cut, so the
    # block still opens with the tail of the prompt: drop through the marker.
    if not used_anchor and "(truncated)" in text:
        cut = text.rfind("(truncated)")
        newline = text.find("\n", cut)
        text = text[newline:] if newline >= 0 else ""
    return text.strip("\n")


def run(args) -> dict:
    if args.prompt_file:
        prompt = open(args.prompt_file, "r", encoding="utf-8").read()
    else:
        prompt = args.prompt
    if prompt is None:
        raise SystemExit("either --prompt or --prompt-file is required")

    argv = [
        args.binary,
        "-m", args.model,
        "-p", prompt,
        "-n", str(args.n),
        "-c", str(args.ctx),
        "-t", str(args.threads),
        "--temp", "0",
        "-s", str(args.seed),
        "-co", "off",
        "-st",
        "--no-warmup",
        # `--fit` (the memory auto-fit) is disabled for BOTH modes: the
        # ggml-shave device reports 0 MiB, and llama.cpp's fit arithmetic divides
        # by that and raises SIGFPE in the model load (reported to the llama.cpp
        # owner).  Fit is a placement heuristic; disabling it keeps the CPU and
        # NPU invocations identical apart from -dev.
        "-fit", "off",
    ]
    if args.device:
        argv += ["-dev", args.device]
    if args.batch:
        argv += ["-b", str(args.batch)]
    if args.ubatch:
        argv += ["-ub", str(args.ubatch)]
    argv += args.extra

    env = dict(os.environ)
    # The backend hooks the atexit stats printer when the device registers, so
    # this works for CPU-only runs too (and then reports 0 NPU invocations).
    env["GGML_SHAVE_STATS"] = "1"

    started = time.time()
    timed_out = False
    try:
        proc = subprocess.run(
            argv,
            input=b"",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=args.timeout,
            env=env,
            cwd=args.cwd,
        )
        rc = proc.returncode
        out = strip_ansi(proc.stdout.decode("utf-8", "replace"))
        err = strip_ansi(proc.stderr.decode("utf-8", "replace"))
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        rc = None
        out = strip_ansi((exc.stdout or b"").decode("utf-8", "replace"))
        err = strip_ansi((exc.stderr or b"").decode("utf-8", "replace"))
    wall_s = time.time() - started

    timing = TIMING.search(out)
    stats = None
    for match in STATS.finditer(err + "\n" + out):
        try:
            stats = json.loads(match.group(1))
        except json.JSONDecodeError:
            pass

    record = {
        "id": args.id,
        "device": args.device or "default",
        "n_predict": args.n,
        "exit_code": rc,
        "timed_out": timed_out,
        "wall_s": round(wall_s, 3),
        "prompt_tps": float(timing.group(1)) if timing else None,
        "gen_tps": float(timing.group(2)) if timing else None,
        "generated_text": extract_generated(out, prompt),
        "error_lines": [line.strip() for line in ERROR_LINE.findall(out + "\n" + err)],
        "shave_stats": stats,
        "argv": argv,
    }
    record["ok"] = (rc == 0 and not timed_out and not record["error_lines"])

    if args.out:
        out_path = os.path.abspath(args.out)
        os.makedirs(os.path.dirname(out_path), exist_ok=True)
        with open(out_path, "w", encoding="utf-8") as handle:
            json.dump(record, handle, indent=2)
            handle.write("\n")
        # Keep the raw streams next to the record: the harness must be auditable
        # without re-running the model, and llama-cli's failure text only exists
        # in its stdout.
        for suffix, text in ((".raw.out", out), (".raw.err", err)):
            with open(out_path + suffix, "w", encoding="utf-8") as handle:
                handle.write(text)

    return record


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--model", required=True)
    parser.add_argument("--prompt")
    parser.add_argument("--prompt-file")
    parser.add_argument("--id", default="prompt")
    parser.add_argument("--n", type=int, default=32)
    parser.add_argument("--ctx", type=int, default=2048)
    parser.add_argument("--threads", type=int, default=8)
    parser.add_argument("--batch", type=int, default=2048)
    parser.add_argument("--ubatch", type=int, default=512)
    parser.add_argument("--seed", type=int, default=1234)
    parser.add_argument("--device", default=None,
                        help="value for -dev (e.g. 'none' for CPU-only, 'SHAVE0')")
    parser.add_argument("--binary", default="./build/bin/llama-cli")
    parser.add_argument("--cwd", default=None)
    parser.add_argument("--timeout", type=float, default=240.0)
    parser.add_argument("--out")
    parser.add_argument("--extra", nargs=argparse.REMAINDER, default=[])
    args = parser.parse_args()

    record = run(args)
    json.dump(record, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

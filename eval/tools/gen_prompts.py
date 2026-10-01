#!/usr/bin/env python3
"""Deterministic generator for eval/prompts.json.

The frozen artifact is ``eval/prompts.json``; this script exists only to make
its provenance auditable (in particular the long-context filler, which is
materialised line by line so the file stays self-contained and cannot drift).

    python3 eval/tools/gen_prompts.py            # writes eval/prompts.json
    python3 eval/tools/gen_prompts.py --check     # fail if it would change

The prompt set covers the kinds required for a comparable evaluation:
arithmetic, code, explanation, short answer, repeat-after-me, instruction
following, counting, and long-context tail retrieval.
"""

from __future__ import annotations

import argparse
import json
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE.parent / "prompts.json"

# The long-context prompt: 96 filler lines of ~10 tokens each (~1k tokens),
# plus a question that can only be answered from the tail of the context.
FILLER_LINES = 96
PASSPHRASE = "MAGENTA-7741"


def long_context_prompt() -> str:
    lines = []
    for i in range(FILLER_LINES):
        lines.append(
            f"RECORD-{i:04d} | shelf A{i % 40:02d} | {i % 97 + 3} crates | inspector: ok"
        )
    lines.append(f"FINAL ENTRY | vault passphrase is {PASSPHRASE} | do not disclose")
    lines.append(
        "Question: what is the vault passphrase written in the final entry? "
        "Reply with only the passphrase."
    )
    return "\n".join(lines)


def prompt_set() -> dict:
    return {
        "schema": "npunlock.eval.promptset.v1",
        "generated_by": "eval/tools/gen_prompts.py",
        "model": "../models/gemma-4-E4B-it-Q4_0.gguf",
        # Fixed decoding contract: greedy, fixed seed, fixed context/threads.
        "decoding": {
            "temperature": 0.0,
            "top_k": 1,
            "seed": 1234,
            "ctx": 4096,
            "threads": 8,
            "batch": 2048,
            "ubatch": 512,
            "warmup": False,
        },
        "defaults": {"n_predict": 32, "timeout_s": 240},
        "prompts": [
            {
                "id": "arith",
                "kind": "arithmetic",
                "n_predict": 24,
                "prompt": "Compute 47 * 89 exactly. Reply with only the number.",
                "checks": [
                    {"type": "regex", "pattern": r"\b4183\b",
                     "desc": "exact product 47*89 = 4183"},
                ],
            },
            {
                "id": "short",
                "kind": "short_answer",
                "n_predict": 16,
                "prompt": "What is the capital of France? Answer with only the city name.",
                "checks": [
                    {"type": "regex", "pattern": r"(?i:paris)",
                     "desc": "answers 'Paris'"},
                ],
            },
            {
                "id": "repeat",
                "kind": "repeat_after_me",
                "n_predict": 32,
                "prompt": ("Repeat exactly, including punctuation: "
                           "The quick brown fox jumps over the lazy dog."),
                "checks": [
                    {"type": "equals_normalized",
                     "value": "The quick brown fox jumps over the lazy dog.",
                     "desc": "verbatim echo of the sentence"},
                ],
            },
            {
                "id": "list3",
                "kind": "instruction_following",
                "n_predict": 32,
                "prompt": ("List exactly three primary colours, one per line, "
                           "with no other text."),
                "checks": [
                    {"type": "each_line_matches",
                     "pattern": r"(?i:\b(red|blue|yellow)\b)", "count": 3,
                     "desc": "three lines, each naming a primary colour"},
                ],
            },
            {
                "id": "count10",
                "kind": "counting",
                "n_predict": 32,
                "prompt": "Count from 1 to 10. Separate the numbers with single spaces.",
                "checks": [
                    {"type": "regex", "pattern": r"1 2 3 4 5 6 7 8 9 10",
                     "desc": "the sequence 1..10 as space-separated digits"},
                ],
            },
            {
                "id": "code",
                "kind": "code",
                "n_predict": 96,
                "prompt": ("Write a Python function add(a, b) that returns the sum of "
                           "its two arguments. Reply with code only, no explanation."),
                "checks": [
                    {"type": "regex", "pattern": r"def\s+add\s*\(",
                     "desc": "defines add(...)"},
                    {"type": "regex", "pattern": r"return\s+a\s*\+\s*b",
                     "desc": "returns a + b"},
                ],
            },
            {
                "id": "explain",
                "kind": "explanation",
                "n_predict": 96,
                "prompt": ("In two sentences, explain why the sky appears blue. "
                           "Mention light scattering."),
                "checks": [
                    {"type": "regex", "pattern": r"(?i:scatter)",
                     "desc": "mentions scattering"},
                    {"type": "min_words", "count": 25,
                     "desc": "produces an actual explanation, not a fragment"},
                ],
            },
            {
                "id": "longctx",
                "kind": "long_context_tail",
                "n_predict": 24,
                "prompt": long_context_prompt(),
                "checks": [
                    {"type": "regex", "pattern": r"MAGENTA-7741",
                     "desc": "retrieves the passphrase from the context tail"},
                ],
            },
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="exit nonzero if the on-disk prompts.json differs")
    args = parser.parse_args()

    text = json.dumps(prompt_set(), indent=2, ensure_ascii=False) + "\n"
    if args.check:
        current = OUT.read_text() if OUT.exists() else ""
        if current != text:
            print(f"{OUT} is stale; re-run without --check", flush=True)
            return 1
        print(f"{OUT} is up to date")
        return 0

    OUT.write_text(text)
    data = json.loads(text)
    print(f"wrote {OUT}: {len(data['prompts'])} prompts")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

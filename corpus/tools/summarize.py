#!/usr/bin/env python3
"""Collect per-kernel diagnostics from corpus/logs into corpus/DIAGNOSTICS.md."""
import pathlib
import re

CORPUS = pathlib.Path(__file__).resolve().parent.parent
LOGS = CORPUS / "logs"
PAT = re.compile(r"(fatal error|error:|UNREACHABLE|warning: SHAVE|Cannot select|legalize:|Permission denied)")


def rows(prefix):
    path = CORPUS / ("results_%s.tsv" % prefix)
    if not path.exists():
        return []
    out = []
    for line in path.read_text().splitlines()[1:]:
        if line.strip():
            out.append(line.split("\t"))
    return out


def diagnostics(prefix, name):
    lines = []
    for stage in ("compile", "asm", "link"):
        log = LOGS / ("%s.%s.%s.err" % (prefix, name, stage))
        if not log.exists() or not log.stat().st_size:
            continue
        for raw in log.read_text(errors="replace").splitlines():
            if PAT.search(raw) and "note:" not in raw:
                if raw not in lines:
                    lines.append(raw.strip())
    return lines


def main():
    out = ["# Corpus diagnostics (verbatim toolchain messages)", ""]
    for prefix in ("kernels", "repros"):
        out.append("## %s" % prefix)
        out.append("")
        for row in rows(prefix):
            name, cstat, crc, astat, arc, lstat, lrc, vstat, text, cls = row[:10]
            status = " ".join("%s=%s" % (k, v) for k, v in
                              [("compile", cstat), ("asm", astat), ("link", lstat), ("validate", vstat)])
            out.append("### %s — %s (text=%s)" % (name, status, text))
            if cstat == "OK" and astat == "OK" and lstat == "OK" and vstat == "OK":
                out.append("")
                continue
            for line in diagnostics(prefix, name)[:4]:
                out.append("- `%s`" % line.replace("`", "'"))
            out.append("")
    (CORPUS / "DIAGNOSTICS.md").write_text("\n".join(out) + "\n")
    print("wrote", CORPUS / "DIAGNOSTICS.md")


if __name__ == "__main__":
    main()

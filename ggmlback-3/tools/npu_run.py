#!/usr/bin/env python3
"""Patch a carrier with a kernel ELF, execute it on the NPU, return the output.

usage: npu_run.py --carrier B --elf E [--input F] [--repeat N] [--dump P] [--json]
The input/output are raw f16 little-endian buffers.  Prints mprobe's timing line.
"""
import argparse, os, subprocess, sys, tempfile

ROOT = "/home/asdf/projects/npunlock-work"
PATCHBLOB = f"{ROOT}/npunlock/build/fresh/npunlock-patchblob"
MPROBE = f"{ROOT}/ggmlback-3/work/mprobe"

def patch(carrier, elf, out):
    p = subprocess.run([PATCHBLOB, "--graph", carrier, "--elf", elf, "--output", out],
                       capture_output=True, text=True)
    if p.returncode:
        raise SystemExit("patchblob failed: %s%s" % (p.stdout, p.stderr))
    return out

def run(patched, input_path=None, repeat=1, dump=None):
    cmd = [MPROBE, "--graph", patched, "--repeat", str(repeat)]
    if input_path:
        cmd += ["--input", input_path]
    if dump:
        cmd += ["--dump", dump]
    p = subprocess.run(cmd, capture_output=True, text=True)
    if p.returncode:
        raise SystemExit("mprobe failed (%d): %s" % (p.returncode, p.stderr))
    return p.stdout

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--carrier", required=True)
    ap.add_argument("--elf", required=True)
    ap.add_argument("--input")
    ap.add_argument("--repeat", type=int, default=1)
    ap.add_argument("--out")
    ap.add_argument("--work", default=None)
    a = ap.parse_args()
    work = a.work or tempfile.mkdtemp(prefix="/tmp/ggmlshave-")
    patched = os.path.join(work, "patched.blob")
    patch(a.carrier, a.elf, patched)
    text = run(patched, a.input, a.repeat)
    for line in text.splitlines():
        if not line.startswith("OUTPUT"):
            print(line)
    if a.out:
        import binascii
        for line in text.splitlines():
            if line.startswith("OUTPUT"):
                hexv = line.split(":", 1)[1].split()
                open(a.out, "wb").write(binascii.unhexlify("".join(hexv).replace("0x", "")))
    print("patched=%s" % patched)

if __name__ == "__main__":
    main()

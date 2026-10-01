#!/usr/bin/env python3
"""Compile SHAVE kernels through npunlock's own shavecc path (header expansion +
compile + assemble + link with lib/mlibm.a + npunlock_parse_shave_elf validation)."""
import sys, os, pathlib
sys.path.insert(0, "/home/asdf/projects/npunlock-work/npunlock/python")
from npunlock._native import NativeLibraries, NativeError

NATIVE = "/home/asdf/projects/npunlock-work/npunlock/build/fresh"
TOOLCHAIN = "/home/asdf/projects/npunlock-work/shave-toolchain-root"

def main():
    src_path = sys.argv[1]
    out_path = sys.argv[2]
    src = pathlib.Path(src_path).read_bytes()
    lib = NativeLibraries(NATIVE)
    try:
        elf = lib.compile_shave(src, toolchain_dir=TOOLCHAIN)
    except NativeError as exc:
        print(f"FAIL {src_path}: {exc}")
        if exc.stdout_log:
            print("--- stdout ---"); print(exc.stdout_log.decode("utf-8", "replace"))
        if exc.stderr_log:
            print("--- stderr ---"); print(exc.stderr_log.decode("utf-8", "replace"))
        return 1
    pathlib.Path(out_path).write_bytes(elf)
    print(f"OK   {src_path} -> {out_path} ({len(elf)} bytes)")
    return 0

sys.exit(main())

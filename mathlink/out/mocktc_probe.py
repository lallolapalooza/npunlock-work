#!/usr/bin/env python3
"""Drive npunlock's real shavecc driver against a mock toolchain to capture the exact
stage argv it builds (in particular whether lib/mlibm.a is passed to the linker)."""
import pathlib, sys
sys.path.insert(0, "/home/asdf/projects/npunlock-work/npunlock/python")
from npunlock._native import NativeLibraries, NativeError

src = b'#define MLIBM_DEFINE_LINK_COMPAT 1\n#include <npunlock/npu3720_kernel.h>\nvoid controlled_act(unsigned p){out_v: (void)p;}\n'
lib = NativeLibraries("/home/asdf/projects/npunlock-work/npunlock/build/fresh")
try:
    elf = lib.compile_shave(src, toolchain_dir="/home/asdf/projects/npunlock-work/mathlink/out/mocktc")
    print("unexpected success", len(elf))
except NativeError as exc:
    print("driver stopped at:", exc.stage, "|", str(exc)[:160])
print(open("/home/asdf/projects/npunlock-work/mathlink/out/mocktc/argv.log").read())

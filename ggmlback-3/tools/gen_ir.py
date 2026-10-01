#!/usr/bin/env python3
"""Generate OpenVINO IR carriers (xml+bin) for the ggml-shave engine.

usage: gen_ir.py OP N OUTDIR
"""
import sys, pathlib
import npunlock as npu
from npunlock.ir import serialize_ir

op, n, outdir = sys.argv[1], int(sys.argv[2]), pathlib.Path(sys.argv[3])
outdir.mkdir(parents=True, exist_ok=True)
x = npu.input("x", shape=(1, n), dtype="f16")
y = npu.op(op, x, _shape=(1, n), _dtype="f16", _name="y")
g = npu.Graph(inputs=[x], outputs=[y], name="c%d" % n)
s = serialize_ir(g)
(outdir / ("%s%d.xml" % (op.lower(), n))).write_bytes(s.xml)
(outdir / ("%s%d.bin" % (op.lower(), n))).write_bytes(s.weights)
print("wrote %s/%s%d.xml" % (outdir, op.lower(), n))

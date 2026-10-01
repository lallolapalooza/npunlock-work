#!/usr/bin/env python3
"""Generate a 2-input OpenVINO IR carrier (binary elementwise ACT).
usage: gen_ir2.py OP N DTYPE OUTDIR
"""
import sys, pathlib
import npunlock as npu
from npunlock.ir import serialize_ir
op, n, dtype, outdir = sys.argv[1], int(sys.argv[2]), sys.argv[3], pathlib.Path(sys.argv[4])
outdir.mkdir(parents=True, exist_ok=True)
x = npu.input("a", shape=(1, n), dtype=dtype)
y = npu.input("b", shape=(1, n), dtype=dtype)
z = npu.op(op, x, y, _shape=(1, n), _dtype=dtype, _name="out")
g = npu.Graph(inputs=[x, y], outputs=[z], name="%s%d" % (op.lower(), n))
s = serialize_ir(g)
stem = "%s%d_%s" % (op.lower(), n, dtype)
(outdir / (stem + ".xml")).write_bytes(s.xml)
(outdir / (stem + ".bin")).write_bytes(s.weights)
print("wrote", outdir / (stem + ".xml"))

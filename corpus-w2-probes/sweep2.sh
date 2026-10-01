#!/bin/bash
# rename the pure-ALU-carried loop counter i21 (not an LSU operand) to X
for r in i11 i10 i12 i15 i16 i31 i24 i29; do
  python3 /tmp/cw2/rename_test.py i21 "$r" /tmp/cw2/s2_$r.s
  out=$(/home/asdf/projects/llvm-14/tmpdump/runp3.sh /tmp/cw2/s2_$r.s --tag "s2$r" 2>&1 | grep -E "^ACTUAL|FAIL")
  echo "counter i21->$r : $out"
done

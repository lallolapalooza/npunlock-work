#!/bin/bash
for r in i7 i8 i9 i10 i11 i12 i13 i14 i15 i16 i23 i24 i25 i26 i27 i28 i29 i31; do
  python3 /tmp/cw2/rename_test.py i21 "$r" /tmp/cw2/s3_$r.s
  out=$(/home/asdf/projects/llvm-14/tmpdump/runp3.sh /tmp/cw2/s3_$r.s --tag "s3$r" 2>&1 | grep -E "^ACTUAL")
  if [ -z "$out" ]; then echo "counter->$r : FAULT/NO-OUTPUT"; 
  elif echo "$out" | grep -q "d3 f9 d7 bb"; then echo "counter->$r : SURVIVES (correct silu)";
  else echo "counter->$r : CLOBBERED -> $out"; fi
done

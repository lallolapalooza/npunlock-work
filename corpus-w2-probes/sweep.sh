#!/bin/bash
for r in i11 i12 i13 i14 i15 i16 i30_notused i23 i24 i25 i31; do
  [ "$r" = "i30_notused" ] && continue
  python3 /tmp/cw2/rename_test.py i17 "$r" /tmp/cw2/sw_$r.s
  out=$(/home/asdf/projects/llvm-14/tmpdump/runp3.sh /tmp/cw2/sw_$r.s --tag "sw$r" 2>&1 | grep -E "^ACTUAL|COMPILE FAIL|ASM FAIL|LINK FAIL")
  echo "i17->$r : $out"
done

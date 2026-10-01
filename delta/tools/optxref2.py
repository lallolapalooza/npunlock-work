#!/usr/bin/env python3
"""Find every function that touches a SHAVE cl::opt object (any field in
[base, base+0x180)), which reveals the option *value readers* -> implementing code.

Writes raw/opt_xrefs.json and prints a summary table.
"""
import re, json, collections

DEC = '/home/asdf/projects/r2gvp04w-re/re_decompiled/movicomp/decompiled_all.c'
OUT = '/home/asdf/projects/npunlock-work/delta/raw/opt_xrefs.json'

BASE = {
 'shave-aliased-store-distance':0x183c703c8,'shave-align-all-branch-targets':0x183c704a8,
 'shave-align-function-targets':0x183c702e8,'shave-align-jump-targets':0x183c6de18,
 'shave-align-loop-targets':0x183c6f860,'shave-align-stack':0x183c6d660,
 'shave-average-instruction-size':0x183c6e5f8,'shave-branch-delays':0x183c6e0b8,
 'shave-disable-constant-pools':0x183c6e958,'shave-no-replace-jmp-with-bra':0x183c6e278,
 'shave-disable-nop-compression':0x183c6d4a0,'shave-disable-NaN-checking':0x183c70800,
 'shave-no-peephole':0x183c6d900,'shave-disable-slot-optimiser':0x183c6dd38,
 'shave-enable-address-simplifier':0x183c6f5b8,'shave-enable-anti-dependency-breaker':0x183c70048,
 'shave-generate-fp-acc-mac':0x183c6fcc8,'shave-generate-int-acc-mac':0x183c6e198,
 'shave-inplace-mutation':0x183c6d580,'shave-enable-interrupt-friendly-scheduling':0x183c6d9e0,
 'shave-enable-ldx-and-stx-instructions':0x183c6d3c0,'shave-enable-loop-load-hoisting':0x183c6fda8,
 'shave-enable-low-impact-preemption':0x183c6ea38,'shave-prera-scheduler':0x183c70b58,
 'shave-pre-sched2-peepholes':0x183c6fe88,'shave-enable-preemption-checks':0x183c70588,
 'shave-stash-retrieve':0x183c6e518,'shave-has-floating-point-unit':0x183c6ff68,
 'shave-has-LSU1-fu':0x183c6d820,'shave-has-SAU-fu':0x183c6ebf8,'shave-has-VAU-fu':0x183c6f940,
 'shave-inline-assembly-warning-threshold':0x183c6dfd8,
 'shave-postra-scheduler-inner-loop-block-weight-modifier':0x183c6fbe0,
 'shave-lsu-flush-on-enter':0x183c6fa20,'shave-lsu-load-policy':0x183c6efb0,
 'shave-lsu-store-policy':0x183c708e0,'shave-lsu-volatile-load-store-policy':0x183c6e6e0,
 'shave-load-latency':0x183c6eb18,'shave-max-inline-assembly-lines':0x183c6d740,
 'shave-postra-scheduler-max-trace-blocks':0x183c6def8,
 'shave-scheduler-max-tracked-mem-ops':0x183c6e438,
 'shave-postra-scheduler-functional-unit-mutation-order':0x183c6edb8,
 'shave-postra-scheduler-outer-loop-block-weight-modifier':0x183c6f778,
 'shave-prera-scheduler-peepholes':0x183c70c38,'shave-preemption-max-loop-depth':0x183c70208,
 'shave-standard-section-prefix':0x183c6f228,'shave-stack-overflow-checking':0x183c6e358,
 'shave-postra-scheduler-trace-generation-direction':0x183c6f340,
 'shave-track-stack-usage':0x183c6eed0,'shave-enable-ldi':0x183c70128,
 'shave-nop-sync-after-inline-asm':0x183c6ecd8,'shave-enable-vilv':0x183c6fb00,
 'shave-vpu4-load-store-distance':0x183c6f698,'shave-widen-three-element-vectors':0x183c6dac0,
}
SPAN = 0x180
SHIFT = 6
bucket = collections.defaultdict(list)
for name, b in BASE.items():
    for k in range(b >> SHIFT, (b + SPAN) >> SHIFT):
        bucket[k].append((name, b))

hdr = re.compile(r'/\* ===== Function (\d+): (\S+) @ (\S+) size=(\d+)')
tok = re.compile(r'18[0-9a-f]{7}')
hits = collections.defaultdict(set)

with open(DEC, encoding='latin1') as f:
    cur = None
    for line in f:
        m = hdr.match(line)
        if m:
            cur = m.group(3)
            continue
        if cur is None:
            continue
        for t in tok.findall(line):
            v = int(t, 16)
            for name, b in bucket.get(v >> SHIFT, ()):
                if b <= v < b + SPAN:
                    hits[name].add(cur)

json.dump({k: sorted(v) for k, v in hits.items()}, open(OUT, 'w'), indent=1)
print('functions referencing each SHAVE option object:')
for name in BASE:
    fs = sorted(hits.get(name, ()))
    print('%-58s %2d  %s' % (name, len(fs), ' '.join(fs[:14]) + (' ...' if len(fs) > 14 else '')))

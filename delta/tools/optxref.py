#!/usr/bin/env python3
"""Map SHAVE cl::opt globals -> functions that read them (xref map).

Reads decompiled_all.c once, tracks function headers, and for a given list of
global symbols (option storage in .data) reports which functions reference them.
"""
import re, sys, json, collections

DEC = '/home/asdf/projects/r2gvp04w-re/re_decompiled/movicomp/decompiled_all.c'

OPTS = {
 'shave-aliased-store-distance':'183c703c8','shave-align-all-branch-targets':'183c704a8',
 'shave-align-function-targets':'183c702e8','shave-align-jump-targets':'183c6de18',
 'shave-align-loop-targets':'183c6f860','shave-align-stack':'183c6d660',
 'shave-average-instruction-size':'183c6e5f8','shave-branch-delays':'183c6e0b8',
 'shave-disable-constant-pools':'183c6e958','shave-no-replace-jmp-with-bra':'183c6e278',
 'shave-disable-nop-compression':'183c6d4a0','shave-disable-NaN-checking':'183c70800',
 'shave-no-peephole':'183c6d900','shave-disable-slot-optimiser':'183c6dd38',
 'shave-enable-address-simplifier':'183c6f5b8','shave-enable-anti-dependency-breaker':'183c70048',
 'shave-generate-fp-acc-mac':'183c6fcc8','shave-generate-int-acc-mac':'183c6e198',
 'shave-inplace-mutation':'183c6d580','shave-enable-interrupt-friendly-scheduling':'183c6d9e0',
 'shave-enable-ldx-and-stx-instructions':'183c6d3c0','shave-enable-loop-load-hoisting':'183c6fda8',
 'shave-enable-low-impact-preemption':'183c6ea38','shave-prera-scheduler':'183c70b58',
 'shave-pre-sched2-peepholes':'183c6fe88','shave-enable-preemption-checks':'183c70588',
 'shave-stash-retrieve':'183c6e518','shave-has-floating-point-unit':'183c6ff68',
 'shave-has-LSU1-fu':'183c6d820','shave-has-SAU-fu':'183c6ebf8','shave-has-VAU-fu':'183c6f940',
 'shave-inline-assembly-warning-threshold':'183c6dfd8',
 'shave-postra-scheduler-inner-loop-block-weight-modifier':'183c6fbe0',
 'shave-lsu-flush-on-enter':'183c6fa20','shave-lsu-load-policy':'183c6efb0',
 'shave-lsu-store-policy':'183c708e0','shave-lsu-volatile-load-store-policy':'183c6e6e0',
 'shave-load-latency':'183c6eb18','shave-max-inline-assembly-lines':'183c6d740',
 'shave-postra-scheduler-max-trace-blocks':'183c6def8',
 'shave-scheduler-max-tracked-mem-ops':'183c6e438',
 'shave-postra-scheduler-functional-unit-mutation-order':'183c6edb8',
 'shave-postra-scheduler-outer-loop-block-weight-modifier':'183c6f778',
 'shave-prera-scheduler-peepholes':'183c70c38','shave-preemption-max-loop-depth':'183c70208',
 'shave-standard-section-prefix':'183c6f228','shave-stack-overflow-checking':'183c6e358',
 'shave-postra-scheduler-trace-generation-direction':'183c6f340',
 'shave-track-stack-usage':'183c6eed0','shave-enable-ldi':'183c70128',
 'shave-nop-sync-after-inline-asm':'183c6ecd8','shave-enable-vilv':'183c6fb00',
 'shave-vpu4-load-store-distance':'183c6f698','shave-widen-three-element-vectors':'183c6dac0',
}
# registration functions to exclude
REG = set('''180003600 180003740 180003850 180003970 180003a80 180003b90 180003cb0 180003df0
180003f30 180004040 180004150 180004260 180004370 180004480 180004590 1800046a0 1800047b0
1800048c0 1800049d0 180004ae0 180004bf0 180004d00 180004e10 180004f20 180005030 180005140
180005480 180005590 1800056a0 1800057b0 1800058c0 1800059d0 180005af0 180005c10 180005d20
180006100 1800064e0 1800067d0 180006910 180006a30 180006b50 180006c70 180006ce0 180006e00
180006f10 180007030 1800070a0 1800071b0 1800074f0 180007600 180007710 180007820 180007930
180007a40'''.split())

addr2opt = {v: k for k, v in OPTS.items()}
pat = re.compile('|'.join(addr2opt))
hdr = re.compile(r'/\* ===== Function (\d+): (\S+) @ (\S+) size=(\d+)')

def main():
    cur = None
    hits = collections.defaultdict(set)
    with open(DEC, encoding='latin1') as f:
        for line in f:
            m = hdr.match(line)
            if m:
                cur = m.group(3)
                continue
            if cur in REG or cur is None:
                continue
            for a in pat.findall(line):
                hits[a].add(cur)
    out = []
    for a, funcs in sorted(hits.items()):
        out.append({'option': addr2opt[a], 'global': a, 'functions': sorted(funcs)})
    json.dump(out, open('/home/asdf/projects/npunlock-work/delta/raw/opt_xrefs.json', 'w'), indent=1)
    for r in out:
        print('%-58s %s' % (r['option'], ' '.join(r['functions'][:12]) + (' ...' if len(r['functions']) > 12 else '')))

main()

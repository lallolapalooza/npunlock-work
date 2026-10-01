#!/usr/bin/env python3
"""patch_encodings.py — one-shot repairs to encodings.json (run after gen_encodings.py).

Why: gen_encodings.py packs definition records FIRST-RECORD-AT-THE-BOTTOM
(bitpos grows from 0).  The model validated against real binaries
(ENCODING_NOTES.md §3: CMU.CMII.U32 = `81 90 10 00`, LSU0.LDO.64 = `c2 62 27
e1 00`, LSU1.LDIH pair `a9 4b 00 00`) is FIRST-RECORD-AT-TOP.  Extraction
yielded record lists in varying order per builder function, so the packed json
is right for some forms and byte-reversed for others.  This script:

  1. appends a first-at-top repack of every schema-16 record list (deduped),
  2. repairs the hand-added BRU.BRA fmt=102 entry (b5-branch: [b5][rel16][pred8]),
  3. drops two corpus-absent forms that poison `asm` form selection
     (LSU0.LDIL fmt=1: 0 matches in 7,394 corpus bytes; BRU.BRA fmt=0: 0 matches),
  4. sets `encode_priority: 0` on the forms --cv 3720xx output is anchored to
     by reloc/DWARF evidence (resolve_form prefers these when encoding).

Idempotent-ish: repacked forms are deduped by (fixed, mask, operand positions).
"""
import json
import pickle
import re
import sys
from collections import defaultdict

HERE = __import__('os').path.dirname(__import__('os').path.abspath(__file__))
JSON = __import__('os').path.join(HERE, 'encodings.json')
PKL = __import__('os').path.join(HERE, 'defs_extracted.pkl')

LIT = {'D', 'X', 'BS'}
PRED = {'P'}


def classify(cls):
    if cls in LIT:
        return 'lit'
    if cls in PRED:
        return 'pred'
    if re.match(r'^0[Xx](\d*)$', cls):
        return 'imm'
    if cls.startswith('0'):
        return 'imm'
    if cls[:1] in ('I', 'V') and len(cls) <= 3:
        return 'reg'
    if cls in ('Av', 'As', 'F', 'T', 'E'):
        return 'reg'
    return 'imm'


def pack_top(recs, unit=''):
    """Pack records first-at-top (vendor model), pad to byte multiple at top."""
    n = sum(int(b) for _, _, b in recs)
    nbytes = (n + 7) // 8
    total = nbytes * 8
    pad = total - n
    pos = total - pad - 1  # start of record region (top of N-bit word)
    fixed = mask = 0
    ops = {}
    for cls, a, b in recs:
        w = int(b)
        hi, lo = pos, pos - w + 1
        k = classify(cls)
        if k == 'lit':
            v = int(a)
            if v >= (1 << w):
                return None
            for i in range(w):
                if (v >> i) & 1:
                    fixed |= 1 << (lo + i)
                mask |= 1 << (lo + i)
        else:
            idx = int(a)
            o = ops.setdefault(idx, {'kind': k, 'operand': idx, 'bitfields': [],
                                     'scale': 1})
            prio = {'reg': 3, 'pred': 2, 'imm': 1}
            if prio[k] > prio[o['kind']]:
                o['kind'] = k
                o['scale'] = re.match(r'^0[Xx](\d*)$', cls)
                o['scale'] = (int(o['scale'].group(1)) if o['scale'] and
                              o['scale'].group(1) else 1)
            o['bitfields'].append([hi, lo])
        pos = lo - 1
    oplist = []
    for idx in sorted(ops):
        o = ops[idx]
        o['bitfields'].sort(key=lambda x: -x[0])
        if unit == 'BRU' and o['kind'] == 'imm':
            o['kind'] = 'rel'
            o['pc_rel_base'] = 'instr'
            o['signed'] = True
            o['scale'] = 1
        oplist.append(o)
    return fixed, mask, nbytes * 8, nbytes, oplist


def ops_key(e):
    return tuple(sorted(
        (o['kind'], o['operand'], tuple(map(tuple, o['bitfields'])))
        for o in e['operands']))


def main():
    raw = json.load(open(JSON))
    insns = raw['instructions']

    defs = pickle.load(open(PKL, 'rb'))
    by_name = defaultdict(set)
    for d in defs:
        if d.get('schema') == 16 and d.get('ok') and d.get('name') and d.get('recs'):
            by_name[d['name']].add(tuple(map(tuple, d['recs'])))

    have = defaultdict(set)  # mnemonic -> {(fixed, mask, ops_key)}
    for e in insns:
        have[e['mnemonic']].add((e['fixed_bits'], e['mask'], ops_key(e)))

    # 1) first-at-top repacks
    added = skipped = bad = 0
    fmt_next = defaultdict(int)
    for e in insns:
        fmt_next[e['mnemonic']] = max(fmt_next[e['mnemonic']], int(e['format']) + 1)
    new_entries = []
    for name in sorted(by_name):
        for recs in sorted(by_name[name], key=str):
            packed = pack_top(list(recs), name.split('.')[0])
            if packed is None:
                bad += 1
                continue
            fixed, mask, nbits, nbytes, oplist = packed
            if mask == 0:
                continue
            key = (fixed, mask, tuple(sorted(
                (o['kind'], o['operand'], tuple(map(tuple, o['bitfields'])))
                for o in oplist)))
            if key in have[name]:
                skipped += 1
                continue
            have[name].add(key)
            unit = name.split('.')[0]
            new_entries.append({
                'mnemonic': name, 'unit': unit, 'format': fmt_next[name],
                'fixed_bits': fixed, 'mask': mask,
                'length_bits': nbits, 'length_bytes': nbytes,
                'operands': oplist, 'slots_allowed': [0, 1, 2],
                'confidence': 'first-at-top-repacked',
            })
            fmt_next[name] += 1
            added += 1
    insns.extend(new_entries)
    print(f'added {added} first-at-top forms (deduped {skipped}, bad {bad})')

    # 2) repair BRU.BRA fmt=102: [b5][rel16][pred8]  (gold x4, ef x8, r_offset=start+1)
    fixed_102 = 0
    for e in insns:
        if e['mnemonic'] == 'BRU.BRA' and e['format'] == 102:
            e['fixed_bits'] = 0x000000b5          # opcode byte at byte0
            e['mask'] = 0x000000ff                 # fixed bits only (byte0)
            e['operands'] = [
                {'kind': 'reg', 'operand': 0, 'bitfields': [[31, 24]],
                 'scale': 1},
                {'kind': 'rel', 'operand': 1, 'bitfields': [[23, 8]],
                 'scale': 1, 'signed': True},
            ]
            e['length_bits'] = 32
            e['length_bytes'] = 4
            e['confidence'] = 'confirmed-by-roundtrip'
            fixed_102 += 1
    print(f'repaired BRU.BRA fmt=102 entries: {fixed_102}')

    # 2b) normalize BRU operands: gen emitted 0X immediates for branches in
    #     gen-built entries; a branch operand must be kind 'rel' so encode
    #     resolves labels/absolute targets as displacements.
    conv = 0
    for e in insns:
        if e['unit'] != 'BRU':
            continue
        for o in e['operands']:
            if o['kind'] == 'imm':
                o['kind'] = 'rel'
                o['pc_rel_base'] = 'instr'
                o['signed'] = True
                o['scale'] = 1
                conv += 1
    # drop entries that became exact duplicates after normalization
    seen = set()
    deduped = []
    for e in insns:
        key = (e['mnemonic'], e['fixed_bits'], e['mask'], ops_key(e))
        if key in seen:
            continue
        seen.add(key)
        deduped.append(e)
    if len(deduped) != len(insns):
        print(f'deduped {len(insns) - len(deduped)} BRU-normalized duplicates')
    insns[:] = deduped
    print(f'normalized {conv} BRU operands imm->rel')

    # 3) drop corpus-absent encode-poison forms
    drop = [('LSU0.LDIL', 1), ('BRU.BRA', 0)]
    before = len(insns)
    insns = [e for e in insns
             if not any(e['mnemonic'] == m and e['format'] == f for m, f in drop)]
    print(f'dropped {before - len(insns)} poison forms')

    # 4) encode priorities (anchored by rela/DWARF evidence in --cv 3720xx output)
    #    + corpus hit counts (mlibm .text + gold .text) so resolve_form can
    #      prefer forms that actually occur in vendor-assembled output.
    corpus = []
    try:
        import glob
        import shave_asm as sa
        for p in sorted(glob.glob('/tmp/shave_objs/mlibm/*.o')):
            try:
                corpus.append(sa.extract_section(p, '.text'))
            except SystemExit:
                pass
        corpus.append(sa.extract_section(
            '/home/asdf/projects/npunlock-work/npunlock/tests/fixtures/npu3720/add1-fp16.elf',
            '.text'))
    except Exception as exc:  # corpus optional: scores stay 0
        print(f'corpus unavailable ({exc}); corpus_hits = 0 everywhere')
    wins = {}
    for data in corpus:
        for L in (1, 2, 3, 4, 5, 6):
            wins.setdefault(L, set()).update(
                int.from_bytes(data[p:p + L], 'little')
                for p in range(len(data) - L + 1))
    for e in insns:
        n = e['length_bytes']
        hits = 0
        for v in wins.get(n, ()):
            if (v & e['mask']) == e['fixed_bits']:
                hits += 1
        e['corpus_hits'] = hits

    prio = {
        # LO16 pair insn in ef_sqrt.o rela: `4a 00 00` (imm at bytes1-2)
        ('LSU0.LDIL', 100): 0,
        # HI16 pair insn in ef_sqrt.o rela: `a9 4b 00 00` (imm at bytes2-3)
        ('LSU1.LDIH', 100): 0,
        # conditional branch [b5][rel16][pred8]: gold 4 sites + ef 8 rela sites
        ('BRU.BRA', 102): 0,
        # IAU shift family: the gen_encodings "table-derived" twin of these
        # records is wrong (its literal record bits are packed bottom-up, so
        # e.g. `IAU.SHR.U32 d,a,imm` got fixed 0x10000EC6 -- bit 28 -- instead
        # of the vendor's 0x80000EC6), and unlike IAU.SHL the SHR forms have no
        # corpus hits, so resolve_form preferred the broken twin.  Measured on
        # the MTL NPU 2026-09-28: 0x10000EC6 does not shift at all (returns
        # a>>2 for imm 16, a for imm 4/1) while the repacked form returns
        # exactly a>>16 / a>>8; `IAU.SHR.I32` without the pin shifts logically
        # (0x0000c0de for a=0xc0dec480) instead of arithmetically
        # (0xffffc0de).  Anchor the first-at-top forms.
        ('IAU.SHL', 3): 0,
        ('IAU.SHR.U32', 3): 0,
        ('IAU.SHR.I32', 2): 0,
        ('IAU.SHR.I32', 3): 0,
    }
    # BRU.BRA unconditional: prefer the [c7][rel16][01] form (ef_sqrt.o+0xb5 rela-anchored)
    for e in insns:
        if (e['mnemonic'], e['format']) in prio:
            e['encode_priority'] = prio[(e['mnemonic'], e['format'])]
        if e['mnemonic'] == 'BRU.BRA' and e['fixed_bits'] == 0x010000c7:
            e['encode_priority'] = 0
        if e['mnemonic'] == 'BRU.BRA' and e['mask'] == 0xff0000ff and \
                e['fixed_bits'] == 0xc7000001:
            # gen-bottom twin of the c7-form: drop its encode claim by demoting
            e['encode_priority'] = 3

    raw['instructions'] = insns
    raw['generator'] = 'gen_encodings.py + patch_encodings.py (first-at-top repack, BRU fixes)'
    json.dump(raw, open(JSON, 'w'), indent=1)
    print(f'written: {len(insns)} instructions')


if __name__ == '__main__':
    sys.exit(main())

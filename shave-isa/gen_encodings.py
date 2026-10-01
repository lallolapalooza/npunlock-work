#!/usr/bin/env python3
"""Generate encodings.json from defs_extracted.pkl (schema-16 encoding records)."""
import pickle, json, re, sys
from collections import Counter

defs = pickle.load(open('/home/asdf/projects/npunlock-work/shave-isa/defs_extracted.pkl', 'rb'))
mn, _, _ = pickle.load(open('/home/asdf/projects/npunlock-work/shave-isa/mn.pkl', 'rb'))
canon = set(mn)

LIT_CLASSES = {'D', 'X', 'BS'}
PRED_CLASSES = {'P'}


def classify(cls):
    if cls in LIT_CLASSES:
        return ('lit', 1)
    if cls in PRED_CLASSES:
        return ('pred', 1)
    m = re.match(r'^0[Xx](\d*)$', cls)
    if m:
        return ('imm', int(m.group(1)) if m.group(1) else 1)
    if cls.startswith('0'):
        return ('imm', 1)
    if cls[:1] in ('I', 'V') and len(cls) <= 3:
        return ('reg', 1)
    if cls in ('Av', 'As', 'F', 'T', 'E'):
        return ('reg', 1)
    return ('imm', 1)


s16 = [x for x in defs if x['schema'] == 16 and x['ok'] and x['name'] and x['name'] in canon]
forms = {}
for x in s16:
    recs = tuple(map(str, x['recs']))
    forms.setdefault(x['name'], {}).setdefault(recs, x['recs'])

instructions = []
allcls = Counter()
for name in sorted(forms):
    unit = name.split('.')[0]
    for fi, (key, recs) in enumerate(sorted(forms[name].items())):
        bitpos = 0
        fixed_bits = 0
        fixed_mask = 0
        ops = {}
        ok = True
        for cls, a, b in recs:
            allcls[cls] += 1
            w = int(b)
            k, scale = classify(cls)
            hi = bitpos + w - 1
            lo = bitpos
            if k == 'lit':
                v = int(a)
                if v >= (1 << w):
                    ok = False
                    break
                fixed_bits |= v << lo
                fixed_mask |= ((1 << w) - 1) << lo
            else:
                idx = int(a)
                o = ops.setdefault(idx, {'kind': k, 'operand': idx,
                                         'bitfields': [], 'scale': scale})
                prio = {'reg': 3, 'pred': 2, 'imm': 1}
                if prio[k] > prio[o['kind']]:
                    o['kind'] = k
                    o['scale'] = scale
                o['bitfields'].append([hi, lo])
            bitpos += w
        if not ok or fixed_mask == 0:
            continue
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
        instructions.append({
            'mnemonic': name, 'unit': unit, 'format': fi,
            'fixed_bits': fixed_bits, 'mask': fixed_mask,
            'length_bits': bitpos, 'length_bytes': (bitpos + 7) // 8,
            'operands': oplist, 'slots_allowed': [0, 1, 2],
            'confidence': 'table-derived',
        })

print("instructions:", len(instructions))
print("classes:", allcls.most_common(40))
kc = Counter(o['kind'] for i in instructions for o in i['operands'])
print(kc)
json.dump({'instructions': instructions,
           'bundles': {'slots': 3, 'max_bits': 128,
                       'note': 'instructions concatenate; <=3 per packet, packet <=128 bits'}},
          open('/home/asdf/projects/npunlock-work/shave-isa/encodings.json', 'w'), indent=1)
print("written")

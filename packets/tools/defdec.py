#!/usr/bin/env python3
"""defdec.py - decode a SHAVE byte stream against the vendor instruction-definition
records extracted from moviAsm64.dll (shave-isa/defs_*.pkl).

Record format (see shave-isa/extract_defs.py):
    {'func': builder fn, 'name': mnemonic, 'recs': [ (cls, val, width), ... ]}
'recs' is listed MSB-field-first: the LAST record occupies the LOWEST bits of the
instruction word (verified against LSU0.LDO.32/LDIL whose emitted bytes we know).

Field classes:
    'D'  + numeric val  -> fixed opcode bits (must equal val)
    other classes       -> variable operand field (register/immediate)
A numeric-looking class (e.g. '0x', '0X4') is an immediate operand slot: variable.

Usage:
    python3 defdec.py gold.bin              # greedy longest/most-specific decode
    python3 defdec.py gold.bin --probe HEX  # show every def matching HEX at offset 0
"""
import pickle
import re
import sys

HERE = '/home/asdf/projects/npunlock-work/shave-isa/'

FIXED_CLS = ('D',)


def load_defs(files=('defs_platform.pkl', 'defs_extracted.pkl')):
    out = []
    for f in files:
        try:
            d = pickle.load(open(HERE + f, 'rb'))
        except Exception:
            continue
        for r in d:
            recs = r.get('recs') or []
            if not r.get('ok') or not recs or any(x is None for x in recs):
                continue
            if not all(len(x) == 3 for x in recs):
                continue
            out.append({'name': r.get('name'), 'func': r.get('func'),
                        'recs': recs, 'src': f})
    return out


def def_bitspec(recs):
    """Return (total_bits, fixed_value, fixed_mask, field_map).

    field_map: list of (name, lo_bit, width) in increasing bit order.
    """
    total = sum(w for _, _, w in recs)
    fields = []
    pos = 0
    for cls, val, w in reversed(recs):      # last rec = lowest bits
        fields.append((cls, val, pos, w))
        pos += w
    fx = 0
    msk = 0
    for cls, val, pos, w in fields:
        if cls in FIXED_CLS:
            fx |= (val & ((1 << w) - 1)) << pos
            msk |= ((1 << w) - 1) << pos
    return total, fx, msk, fields


def int_of(data, bitlen):
    v = int.from_bytes(data[:((bitlen + 7) // 8)], 'little')
    return v & ((1 << bitlen) - 1)


class DB:
    def __init__(self, defs):
        self.by_len = {}
        for d in defs:
            total, fx, msk, fields = def_bitspec(d['recs'])
            if total % 8:
                continue          # bit-granular forms cannot start byte-aligned
            rec = dict(d, bits=total, fx=fx, msk=msk, fields=fields, nbytes=total // 8)
            self.by_len.setdefault(total, []).append(rec)

    def match(self, data, off=0, want=None):
        hits = []
        for bits, recs in self.by_len.items():
            if off + bits // 8 > len(data):
                continue
            v = int_of(data[off:], bits)
            for r in recs:
                if want and r['name'] != want:
                    continue
                if v & r['msk'] == r['fx']:
                    hits.append(r)
        return hits

    def operands(self, r, v):
        out = []
        for cls, val, pos, w in r['fields']:
            raw = (v >> pos) & ((1 << w) - 1)
            if cls in FIXED_CLS:
                out.append(('op', 'D', raw))
            else:
                out.append(('op', cls, raw))
        return out


def main():
    args = sys.argv[1:]
    db = DB(load_defs())
    if '--probe' in args:
        i = args.index('--probe')
        hexs = args[i + 1].replace(' ', '')
        data = bytes.fromhex(hexs)
        hits = db.match(data, 0)
        print(f'{len(hits)} defs match {hexs}')
        for r in sorted(hits, key=lambda r: (r['nbytes'], r['name'] or '')):
            print(f"  {r['name']:34s} {r['nbytes']}B fx={r['fx']:011x} msk={r['msk']:011x} {r['recs']} [{r['src']}/{r['func']}]")
        return
    path = args[0]
    data = open(path, 'rb').read()
    off = 0
    while off < len(data):
        hits = db.match(data, off)
        if not hits:
            print(f'0x{off:04x}: .byte ' + ' '.join(f'{b:02x}' for b in data[off:off + 1]) + '   ; UNDECODED')
            off += 1
            continue
        # prefer the def with most fixed bits (most specific), then longest
        hits.sort(key=lambda r: (-bin(r['msk']).count('1'), -r['nbytes']))
        r = hits[0]
        v = int_of(data[off:], r['bits'])
        print(f"0x{off:04x}: {data[off:off + r['nbytes']].hex():<12s} {r['name']:34s} {r['nbytes']}B raw={data[off:off+r['nbytes']].hex()}")
        off += r['nbytes']


if __name__ == '__main__':
    main()

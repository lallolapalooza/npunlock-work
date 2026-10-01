#!/usr/bin/env python3
"""cal_fix.py — final canonical-layout fixes for calibrated/inferred forms.

MUST run last in the generation pipeline:

    python3 gen_encodings.py
    python3 patch_encodings.py     # stamps corpus_hits/encode_priority, BRU dedupe
    python3 cal_fix.py             # canonical layouts below

Covers:
  * LSU0.LDIL fmt100/101 (3B) and LSU1.LDIH fmt100 (4B) canonical record order
    [imm16][reg5][subop3][op8] as consumed by the msb remap in shave_asm.Insn
    (mask-run remap + operand bitpos = w - lo - width).
      - LDIH fmt100 fixed must be 0x75200000 so the remapped value-space fixed
        part equals 0x03a9 (0xa9 byte0 + subop 3 in byte1 bits0-2).
      - LDIL fmt100 fixed 0x400000 / fmt101 0xc00000 with mask 0xe00000
        (run(21,3) -> value bits0-2 = subop).
  * BRU.BRA fmt1 (unconditional, value bytes [0xc7][rel16][0x01]):
    mask 0xff0000ff — the low byte MUST be a full run(0,8) so the remap puts
    0x01 at value bits24-31 (a 1-bit low mask remaps to bit31 and emits 0x80).
    fixed 0xc7000001, operand rel canonical [[23,8]].
  * BRU.BRA fmt102 (conditional, value bytes [0xb5][rel16][iN]):
    fixed 0xb5<<24, mask 0xff<<24, reg canonical [[7,0]], rel [[23,8]].
  * encode_priority for the above (corpus-anchored forms rank first).
"""
import json
import re
import subprocess
from collections import Counter

DB_PATH = 'encodings.json'

RECOUNT_ELFS = (
    ['/home/asdf/projects/npunlock-work/npunlock/tests/fixtures/npu3720/'
     'add1-fp16.elf']
    + sorted(__import__('glob').glob('/tmp/mlibm_x/*.o'))
)


def recount_corpus_hits(db):
    """Recompute corpus_hits for mnemonics seen in our disasm listings.

    patch_encodings.py's stamps predate decode-side form ranking; encode must
    anchor to the form the DECODER actually selects per site, so counts are
    rebuilt from `; fmt=N` annotations of shave_asm disasm over the real
    binaries.  Mnemonics absent from the listings keep their old stamps.
    """
    tally = Counter()
    rx = re.compile(r';\s*fmt=(\d+).*?([A-Za-z0-9_.]+\.[A-Za-z0-9_.]+|[A-Z][A-Z0-9_.]*)\b')
    rxmn = re.compile(
        r'^0x[0-9a-f]+:\s+[0-9a-f]+\s+([A-Za-z0-9_.]+)\b.*;\s*fmt=(\d+)')
    for elf in RECOUNT_ELFS:
        try:
            out = subprocess.run(['python3', 'shave_asm.py', 'disasm',
                                  '--elf', elf], capture_output=True,
                                 text=True, timeout=120)
        except (OSError, subprocess.TimeoutExpired):
            continue
        for ln in out.stdout.splitlines():
            m = rxmn.search(ln)
            if m:
                tally[(m.group(1).upper(), int(m.group(2)))] += 1
    touched = {k[0] for k in tally}
    n = 0
    for i in db['instructions']:
        key = (i['mnemonic'], i['format'])
        if i['mnemonic'] in touched:
            i['corpus_hits'] = tally.get(key, 0)
            n += 1
    print(f'recounted corpus_hits for {len(touched)} mnemonics '
          f'({n} forms)')


def setform(db, name, fmt, **kw):
    for i in db['instructions']:
        if i['mnemonic'] == name and i['format'] == fmt:
            i.update(kw)
            print(f"fixed {name} fmt={fmt}")
            return True
    print(f"MISSING {name} fmt={fmt} (skipped)")
    return False


def main():
    db = json.load(open(DB_PATH))
    def ensure(name, fmt, **spec):
        for i in db['instructions']:
            if i['mnemonic'] == name and i['format'] == fmt:
                i.update(spec)
                print(f"ensured {name} fmt={fmt}")
                return
        e = {'mnemonic': name, 'format': fmt}
        e.update(spec)
        db['instructions'].append(e)
        print(f"created {name} fmt={fmt}")

    # hand-recovered forms that gen_encodings.py does not regenerate
    ensure('LSU1.LDIH', 100,
           unit='LSU1', fixed_bits=0x75200000, mask=0xffe00000,
           length_bits=32, length_bytes=4,
           confidence='confirmed-by-roundtrip', encode_priority=0,
           operands=[{'kind': 'reg', 'operand': 0,
                      'bitfields': [[20, 16]], 'scale': 1},
                     {'kind': 'imm', 'operand': 1,
                      'bitfields': [[15, 0]], 'scale': 1}])
    ensure('BRU.BRA', 102,
           unit='BRU', fixed_bits=0xb5000000, mask=0xff000000,
           length_bits=32, length_bytes=4,
           confidence='inferred', encode_priority=-1,
           operands=[{'kind': 'reg', 'operand': 0,
                      'bitfields': [[7, 0]], 'scale': 1},
                     {'kind': 'rel', 'operand': 1,
                      'bitfields': [[23, 8]], 'signed': True,
                      'pc_rel_base': 'instr', 'scale': 1}])

    setform(db, 'LSU0.LDIL', 101,
            fixed_bits=0xc00000, mask=0xe00000, encode_priority=0,
            operands=[{'kind': 'reg', 'operand': 0,
                       'bitfields': [[20, 16]], 'scale': 1},
                      {'kind': 'imm', 'operand': 1,
                       'bitfields': [[15, 0]], 'scale': 1}])
    setform(db, 'LSU1.LDIH', 100,
            fixed_bits=0x75200000, mask=0xffe00000, encode_priority=0,
            operands=[{'kind': 'reg', 'operand': 0,
                       'bitfields': [[20, 16]], 'scale': 1},
                      {'kind': 'imm', 'operand': 1,
                       'bitfields': [[15, 0]], 'scale': 1}])
    setform(db, 'BRU.BRA', 1,
            fixed_bits=0xc7000001, mask=0xff0000ff, encode_priority=-1,
            operands=[{'kind': 'rel', 'operand': 0,
                       'bitfields': [[23, 8]], 'signed': True,
                       'pc_rel_base': 'instr', 'scale': 1}])
    setform(db, 'BRU.BRA', 102,
            fixed_bits=0xb5000000, mask=0xff000000, encode_priority=-1,
            operands=[{'kind': 'reg', 'operand': 0,
                       'bitfields': [[7, 0]], 'scale': 1},
                      {'kind': 'rel', 'operand': 1,
                       'bitfields': [[23, 8]], 'signed': True,
                       'pc_rel_base': 'instr', 'scale': 1}])
    for i in db['instructions']:
        if i['mnemonic'] == 'BRU.BRA' and i['format'] not in (1, 102):
            i['encode_priority'] = 2
    # gold epilogue: 47 f1 01 at .text+0xb7 (BRU.JMP i30); the other fmt
    # encodes 01fe28 and must never be default-picked.  The gold line is
    # currently stolen by a wider CP.128 decode match one byte earlier, so the
    # recount cannot see fmt=0 -- pin it explicitly.
    setform(db, 'BRU.JMP', 0, encode_priority=-1)
    # gold convert sites: .text+0x59 = 81 9c 0c 08 (CMU.CP.32 i6, i9),
    # ef_j0 81 dc 25 08 (CMU.CP.32 i18, i29); pin the plain name's default
    # gold's convert = the repacked fmt15 (21 decode hits, byte-exact);
    # gen fmt1 encodes 819c8c00 (different fixed) - demote it
    setform(db, 'CMU.CP.32', 15, encode_priority=-1)
    setform(db, 'CMU.CP.32', 1, encode_priority=2)
    setform(db, 'LSU0.LDO.32', 0, encode_priority=0)
    # gold f32-add site .text+0xaa = 3c e0 9c 30 matches the repacked fmt3
    # (subop nibble 3); fmt2 (nibble 2) is the other repack variant
    setform(db, 'SAU.ADD.F32', 3, encode_priority=-1)
    setform(db, 'SAU.ADD.F32', 2, encode_priority=1)
    setform(db, 'BRU.JMP', 1, encode_priority=1)
    # --- descriptor-read family (ACT cross-binary scan, sec 15) -------
    # value-space entries (confidence first-at-top-repacked => no remap):
    #   [op8][unit4@8][tag@16,size@22-23][dst4@12-15][base5@17-21][imm15@25-39]
    # length 40 bits / 5 bytes; observed op/unit combos instantiated below.
    def add_ldo_family(fmt, op, unit, tag_byte2):
        # tag bits: byte2 = (base<<1) | (tag0@bit0) | tag_hi@bits6-7
        # value bit16 = byte2 bit0, value bits22-23 = byte2 bits6-7
        # imm15 occupies value bits25-39; bit24 is the fixed '1' parity of
        # the (imm<<1)|1 encoding
        fixed = (op | (unit << 8) | ((tag_byte2 & 1) << 16)
                 | ((tag_byte2 >> 6) << 22) | (1 << 24))
        mask = 0xff | 0xf00 | (1 << 16) | (3 << 22) | (1 << 24)
        entry = None
        for i in db['instructions']:
            if i['mnemonic'] == 'LSU0.LDO.32' and i['format'] == fmt:
                entry = i
                break
        if entry is None:
            entry = {}
            db['instructions'].append(entry)
        entry.update({
            'mnemonic': 'LSU0.LDO.32', 'unit': 'LSU0', 'format': fmt,
            'fixed_bits': fixed, 'mask': mask,
            'length_bits': 40, 'length_bytes': 5,
            'confidence': 'first-at-top-repacked',
            'operands': [
                {'kind': 'reg', 'operand': 0, 'bitfields': [[15, 12]], 'scale': 1},
                {'kind': 'reg', 'operand': 1, 'bitfields': [[21, 17]], 'scale': 1},
                {'kind': 'imm', 'operand': 2, 'bitfields': [[39, 25]], 'scale': 1},
            ],
        })
        print(f'set LDO family fmt{fmt} op={op:#x} unit={unit} tag={tag_byte2:#x}')
    add_ldo_family(110, 0x01, 0x2, 0x40)   # gold .text+0x8d out-read
    add_ldo_family(111, 0xfa, 0x2, 0x40)   # ACT in/out reads
    add_ldo_family(112, 0xfa, 0x6, 0x40)
    add_ldo_family(113, 0xf9, 0x2, 0x40)
    add_ldo_family(114, 0xf0, 0x6, 0x40)
    # --- vendor field-map fix for ST/STI.16.F32.F16 (TRACE_REPORT Q1) --
    # vendor record {D,0x40,9}{IR,1,5}{IR,0,5}{D,0x442,13} + 0..16 register
    # sweep (TRACE_REPORT.md Q1): in VALUE/byte space,
    #        value = LE bits[16:12], base = LE bits[21:17], STI = LE bit27.
    # Entries are normalized to 'first-at-top-repacked' (value space, no
    # remap at load) so fixed/mask/fields are all written in byte space.
    FLD = 0x3ff000          # LE bits12..21
    STI_BIT = 1 << 27
    MASK_V = 0xFFC00FFF     # every bit except the two register fields
    patched = 0

    def remap_to_value(f0, m):
        # canonical-LSB -> value space, same math as shave_asm.Insn
        w = 32
        new_f = 0
        mm = m
        p = 0
        while mm:
            if mm & 1:
                s = p
                while mm & 1:
                    mm >>= 1
                    p += 1
                r = p - s
                pos = w - s - r
                new_f |= ((f0 >> s) & ((1 << r) - 1)) << pos
            else:
                mm >>= 1
                p += 1
        return new_f

    for i in db['instructions']:
        nm = i['mnemonic']
        if nm not in ('LSU0.ST.16.F32.F16', 'LSU1.ST.16.F32.F16',
                      'LSU0.STI.16.F32.F16', 'LSU1.STI.16.F32.F16'):
            continue
        regops = [o for o in i['operands'] if o['kind'] == 'reg']
        if len(regops) != 2:
            continue                      # >2-operand variants: unverified, skip
        if i.get('confidence') == 'first-at-top-repacked':
            v_fixed = i['fixed_bits']      # already value space
        else:
            v_fixed = remap_to_value(i['fixed_bits'], i['mask'])
        v_fixed &= ~FLD
        if 'STI.' in nm:
            v_fixed |= STI_BIT
        else:
            v_fixed &= ~STI_BIT
        i['fixed_bits'] = v_fixed
        i['mask'] = MASK_V
        i['operands'] = [
            {'kind': 'reg', 'operand': 0, 'bitfields': [[16, 12]], 'scale': 1},
            {'kind': 'reg', 'operand': 1, 'bitfields': [[21, 17]], 'scale': 1},
        ]
        i['confidence'] = 'first-at-top-repacked'
        i['length_bits'] = 32
        i['length_bytes'] = 4
        patched += 1
    print(f'field-map patched (value space) ST/STI.16.F32.F16 plain forms: {patched}')
    recount_corpus_hits(db)
    # HW LDI pins applied LAST (after ensure() creates above)
    for nm, fmt in (('LSU0.LDIL', 0), ('LSU1.LDIL', 0),
                    ('LSU0.LDIH', 0), ('LSU1.LDIH', 100)):
        setform(db, nm, fmt, encode_priority=-1)
    for i in db['instructions']:
        if i['mnemonic'] in ('LSU0.LDIL', 'LSU1.LDIL',
                             'LSU0.LDIH', 'LSU1.LDIH'):
            if i['length_bytes'] == 3:
                i['encode_priority'] = 4
            elif i['format'] not in (0, 100):
                i['encode_priority'] = 2
    # --- LSU1 store forms on the hintless path (LSU unit policy) --------
    # `LSU1.ST.16.F32.F16` hintless picked the mask-poor fmt0 (0x920 ->
    # bytes `20 69 0a 00`, operand fields landing in the wrong places).  fmt1
    # (0x00000449 -> `49 34 00 08` for i3,i0) is the structurally closest
    # form: the exact +7 unit delta of the HW-anchored LSU0 form 42640a00
    # (fmt1 0x00000442).  fmt2/fmt3 carry the same fixed pattern, so pin fmt1
    # and demote fmt0.
    for fmt, prio in ((0, 2), (1, -1), (2, 1), (3, 1)):
        setform(db, 'LSU1.ST.16.F32.F16', fmt, encode_priority=prio)
    # `LSU1.ST.32` fmt18 (`89 09 00 01`) is the form verified on hardware by
    # the u5/k1 store probe; keep it ahead of the structurally-equal fmt5.
    setform(db, 'LSU1.ST.32', 18, encode_priority=-1)
    json.dump(db, open(DB_PATH, 'w'), indent=1)
    print('saved', DB_PATH)


if __name__ == '__main__':
    main()

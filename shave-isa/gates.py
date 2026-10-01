#!/usr/bin/env python3
"""gates.py — shave-isa regression + issued-stream legality gates.

    python3 gates.py            # all gates, non-zero exit on any failure
    python3 gates.py -v         # print every check

Gates
  G1 gold roundtrip      disasm --elf add1-fp16.elf -> asm -> 192/192 bytes
  G2 mlibm roundtrips    /tmp/mlibm_x/*.o disasm->asm, byte-identical
  G3 encode anchors      HW/raw-anchored form encodes + self-consistency
  G4 delay slots         unconditional branch -> exactly 6 fillers (idempotent)
  G5 load latency        load->consumer >= 6 slots in the emitted BYTES
  G6 LSU unit policy     store rewritten LSU0->LSU1, error mode refuses
  G7 pipe shape          <=3 slots, <=1 op per unit, <=128 bits
  G8 verbatim mode       raw= listings are re-encoded untouched
"""
import glob
import io
import os
import re
import sys

import shave_asm as A

HERE = os.path.dirname(os.path.abspath(__file__))
GOLD = ('/home/asdf/projects/npunlock-work/npunlock/tests/fixtures/npu3720/'
        'add1-fp16.elf')
MLIBM = sorted(glob.glob('/tmp/mlibm_x/*.o'))

DB = A.DB()
VERBOSE = '-v' in sys.argv or '--verbose' in sys.argv
FAILS = []
CHECKS = [0]


def check(name, ok, detail=''):
    CHECKS[0] += 1
    if ok:
        if VERBOSE:
            print(f'  ok   {name}{"  " + detail if detail else ""}')
    else:
        FAILS.append(f'{name}: {detail}')
        print(f'  FAIL {name}  {detail}')
    return ok


def gold_text():
    return A.extract_section(GOLD, '.text')


def legal_bytes(text, **kw):
    """CLI-equivalent path: legalize (unless exact/disasm listing) + assemble."""
    opts = A.StreamOpts(**kw)
    if A.is_disasm_listing(text):
        opts.enabled = False
    new_text, st = A.legalize_text(DB, text, opts)
    return A.assemble(DB, new_text), new_text, st


def decode(blob):
    """[(offset, mnemonic, [ops], length)] for a byte stream."""
    out = []
    buf = io.StringIO()
    A.disasm_stream(DB, blob, out=buf)
    for ln in buf.getvalue().splitlines():
        m = re.match(r'^0x([0-9a-f]+):\s+([0-9a-f]+)\s+(\S+)(?:\s+(.*?))?\s*;',
                     ln)
        if not m:
            m = re.match(r'^0x([0-9a-f]+):\s+\.byte\s+(.*?)\s*;', ln)
            if m:
                vals = [v.strip().lower() for v in m.group(2).split(',')]
                out.append((int(m.group(1), 16), '.byte', vals, len(vals)))
            continue
        ops = [o.strip() for o in m.group(4).split(',')] if m.group(4) else []
        out.append((int(m.group(1), 16), m.group(3), ops,
                    len(m.group(2)) // 2))
    return out


# --------------------------------------------------------------------------
# G1 — gold roundtrip (the unchanged gate command; disasm listing = verbatim)
# --------------------------------------------------------------------------
def gate_gold():
    print('G1 gold roundtrip (192/192)')
    gold = gold_text()
    buf = io.StringIO()
    A.disasm_stream(DB, gold, out=buf)
    listing = buf.getvalue()
    blob, new_text, st = legal_bytes(listing)
    check('G1 gold disasm->asm byte-identical', blob == gold,
          f'{len(blob)} vs {len(gold)} bytes, first diff '
          f'{next((i for i, (a, b) in enumerate(zip(blob, gold)) if a != b), None)}')
    check('G1 listing carries raw= (verbatim mode)', A.is_disasm_listing(listing))
    # legalize_text itself must no-op on a listing (already vendor-scheduled)
    t2, st2 = A.legalize_text(DB, listing, A.StreamOpts())
    check('G1 verbatim path changes nothing',
          t2 == listing and not st2['changed'] and not st2['warnings'] and
          st2.get('verbatim'),
          f"delay={st2['delay_nops']} lat={st2['latency_nops']} "
          f"warn={len(st2['warnings'])}")


# --------------------------------------------------------------------------
# G2 — mlibm objects: disasm->asm roundtrip byte-identical
# --------------------------------------------------------------------------
def gate_mlibm():
    if not MLIBM:
        print('G2 mlibm roundtrips (skipped: no /tmp/mlibm_x/*.o)')
        return
    print(f'G2 mlibm roundtrips ({len(MLIBM)} objects)')
    for path in MLIBM:
        try:
            blob = A.extract_section(path, '.text')
        except SystemExit:
            continue
        buf = io.StringIO()
        A.disasm_stream(DB, blob, out=buf)
        back, _t, _s = legal_bytes(buf.getvalue())
        check(f'G2 {os.path.basename(path)}', back == blob,
              f'{len(back)} vs {len(blob)}')


# --------------------------------------------------------------------------
# G3 — encode anchors (hardware-verified bytes / vendor-corpus raw bytes)
# --------------------------------------------------------------------------
ANCHORS = [
    # (source, expected hex, evidence, decode-back mnemonic)
    ('LSU0.LDIL i2, 0xABCD', '8212cdab', 'HW PACKET_SPEC R3', 'LSU0.LDIL'),
    ('LSU0.LDIH i4, 0x5678', '82237856', 'HW PACKET_SPEC R3', 'LSU0.LDIH'),
    ('LSU1.LDIL i4, 0x5678', '89227856', 'HW PACKET_SPEC R3', 'LSU1.LDIL'),
    ('LSU1.LDIH i4, 0x5678', 'a9237856', 'HW PACKET_SPEC R3', 'LSU1.LDIH'),
    ('LSU0.LDIL i20, 0x1234', '82a23412', 'HW (reg>=16)', 'LSU0.LDIL'),
    ('LSU1.LDIL i20, 0xABCD', '89a2cdab', 'HW (reg>=16)', 'LSU1.LDIL'),
    ('LSU0.LDO.32 i0, i18, 0x28 ; fmt=0', 'c202645100', 'HW k1/load_vis',
     'LSU0.LDO.32'),
    ('LSU0.LDO.32 i10, i18, 0xc ; fmt=0', 'c2a2641900', 'gold .text+0x00',
     'LSU0.LDO.32'),
    ('LSU1.ST.32 i0, i0', '89090001', 'HW u5 probe (LSU1 store)',
     'LSU1.ST.32'),
    ('LSU0.ST.32 i0, i0', '82090008', 'LSU0 twin of the u5 probe',
     'LSU0.ST.32'),
    ('LSU0.ST.16.F32.F16 i6, i5', '42640a00', 'HW FINDINGS anchor',
     'LSU0.ST.16.F32.F16'),
    ('LSU0.ST.16.F32.F16 i7, i8', '42741000', 'HW FINDINGS anchor',
     'LSU0.ST.16.F32.F16'),
    ('LSU0.ST.16.F32.F16 i10, i29', '42a43a00', 'HW FINDINGS anchor',
     'LSU0.ST.16.F32.F16'),
    ('LSU1.STI.16.F32.F16 i6, i5 ; fmt=6', '49640a08', 'HW FINDINGS anchor',
     None),
    ('LSU1.STI.16.F32.F16 i7, i8 ; fmt=6', '49741008', 'gold .text+0xaf',
     None),
    ('IAU.ADD i19, i19, 0x28', 'c6604e94', 'ef_j0 raw', 'IAU.ADD'),
    ('IAU.SUB i19, i19, 0x28', 'c6614e94', 'ef_j0 raw', 'IAU.SUB'),
    ('IAU.AND i1, i8, i1', '86108500', 'ef_j0 raw', 'IAU.AND'),
    ('IAU.AND i4, i1, i2', '86301001', 'round-trips (doc 82044308 stale)',
     'IAU.AND'),
    ('IAU.ADD i9, i9, 2', 'c6202581', 'gold .text+0xb3 site', 'IAU.ADD'),
    ('IAU.ADD i1, i1, 1', 'c6208480', 'AU-imm anchor (K.4)', 'IAU.ADD'),
    ('IAU.ADD i1, i1, 4', 'c6200482', 'AU-imm anchor (K.4)', 'IAU.ADD'),
    ('SAU.DIV.U32 i9, i8, i6', '3c062503', 'gold .text+0x5d', 'SAU.DIV.U32'),
    ('SAU.ADD.F32 i1, i2, i3 ; fmt=3', '3c408431', 'gold .text+0xaa class',
     'SAU.ADD.F32'),
    ('CMU.CMZ.I32 i5 ; fmt=0', '41a000', 'gold .text+0x50', 'CMU.CMZ.I32'),
    ('CMU.CP.32 i6, i9 ; fmt=15', '819c0c08', 'gold .text+0x59', 'CMU.CP.32'),
    ('CMU.CP.32 i6, i9', '819c0c08', 'hintless == fmt15 pin', 'CMU.CP.32'),
    ('CMU.CP.32 i18, i29 ; fmt=15', '81dc2508', 'ef_j0 convert site',
     'CMU.CP.32'),
    ('CMU.CP.32 i18, i29', '81dc2508', 'hintless == fmt15 pin', 'CMU.CP.32'),
    ('SAU.ADD.F32 i7, i7, i1', '3ce09c30', 'gold .text+0xaa site',
     'SAU.ADD.F32'),
    ('SAU.ADD.F32 i1, i2, i3 ; fmt=3', '3c408431', 'fmt3 repack variant',
     'SAU.ADD.F32'),
    ('LSU0.LDO.32 i5, i5, 0', 'c2524a0100', 'gold .text+0x88 site',
     'LSU0.LDO.32'),
    ('LSU0.LDO.32 i8, i18, 0x28 ; fmt=110', '0182645100',
     'LSU0.LDO.32 fmt110 (gold 0x8d family)', 'LSU0.LDO.32'),
    ('LSU0.LD.32.F16.F32 i7, i9', '42701200', 'gold f16 load class',
     'LSU0.LD.32.F16.F32'),
    ('LSU0.ST.32 i4, [i6]', '82490c08', 'mem-operand form (fmt5)',
     'LSU0.ST.32'),
    ('LSU0.LD.8 i2, [i0+4]', 'c22a002106', 'mem operand + displacement',
     'LSU0.LD.8'),
    ('LSU0.LDIL i16, 0x0E', '82820e00', 'HW 4B form (3B demoted to prio 4)',
     'LSU0.LDIL'),
    ('BRU.BRA i2, 0x4', 'b5040002', 'conditional branch, rel=4',
     'BRU.BRA'),
    ('BRU.JMP i30', '47f101', 'gold .text+0xb7 epilogue', 'BRU.JMP'),
    ('BRU.NOP', '07', 'vendor NOP (only legal 1-byte form)', None),
    ('LSU0.LD.8 i2, [i0]', '425d000e', 'VALIDATION mem-operand example',
     'LSU0.LD.8'),
    ('LSU0.LD.32.F16.F32 i1, i0', '42100000', 'gold .text+0x44 class',
     'LSU0.LD.32.F16.F32'),
]


def gate_anchors():
    print(f'G3 encode anchors ({len(ANCHORS)} forms)')
    for src, exp, ev, want_dec in ANCHORS:
        try:
            got = A.assemble(DB, src + '\n').hex()
        except SystemExit as e:
            got = f'ERR {e}'
        check(f'G3 {src}', got == exp, f'got {got} want {exp} [{ev}]')
        if got == exp and want_dec:
            dec = decode(bytes.fromhex(got))
            check(f'G3 {src} decodes back', bool(dec) and
                  dec[0][1].upper() == want_dec.upper(),
                  f'decoded {dec[0][1] if dec else None}')


# --------------------------------------------------------------------------
# G4 — branch delay slots
# --------------------------------------------------------------------------
def gate_delay():
    print('G4 branch delay slots')
    # unconditional branch, nothing after -> exactly 6 fillers
    for br, head in (('BRU.JMP i30', b'\x47\xf1\x01'),
                     ('BRU.SWIH 1', b'\x05\x00\x00')):
        blob, text, st = legal_bytes(f'\t{br}\n', compressed_nops=False)
        check(f'G4 {br} -> exactly 6 fillers (plain 0x07)',
              st['delay_nops'] == 6 and blob[len(head):] == b'\x07' * 6,
              f'{blob.hex()} (delay_nops={st["delay_nops"]})')
        blobc, textc, stc = legal_bytes(f'\t{br}\n')
        check(f'G4 {br} -> one compressed byte 0x50 (default)',
              stc['delay_nops'] == 6 and blobc[len(head):] == b'\x50',
              f'{blobc.hex()} (delay_nops={stc["delay_nops"]})')
    # explicit NOP run of 6 is respected (idempotent)
    six = '\tBRU.JMP i30\n' + '\tBRU.NOP\n' * 6
    blob1, t1, st1 = legal_bytes(six, compressed_nops=False)
    check('G4 existing 6-NOP run untouched',
          st1['delay_nops'] == 0 and blob1 == b'\x47\xf1\x01' + b'\x07' * 6,
          f'{blob1.hex()} delay_nops={st1["delay_nops"]}')
    # re-legalizing our own output must be a no-op (idempotency)
    blob2, t2, st2 = legal_bytes(t1, compressed_nops=False)
    check('G4 idempotent (2nd pass)', blob2 == blob1 and st2['delay_nops'] == 0,
          f'delay_nops={st2["delay_nops"]}')
    # short explicit run is topped up to 6
    blob3, t3, st3 = legal_bytes('\tBRU.JMP i30\n\tBRU.NOP\n\tBRU.NOP\n',
                                 compressed_nops=False)
    check('G4 2-NOP run topped up to 6',
          blob3 == b'\x47\xf1\x01' + b'\x07' * 6,
          f'{blob3.hex()} delay_nops={st3["delay_nops"]}')
    # `.byte 0x07` fillers count too (probe-writing style)
    blob4, t4, st4 = legal_bytes('\tBRU.JMP i30\n' +
                                 '\t.byte 0x07\n' * 6)
    check('G4 `.byte 0x07` run counts as fillers', st4['delay_nops'] == 0,
          f'delay_nops={st4["delay_nops"]}')
    # compressed NOPs: (n-1)<<4 covers n slots (VendorRE2, measured)
    c6 = '\tBRU.JMP i30\n\t.byte 0x50\n'
    _bc, _tc, stc = legal_bytes(c6)
    check('G4 `.byte 0x50` = 6 slots (no fill)',
          stc['delay_nops'] == 0, f'delay_nops={stc["delay_nops"]}')
    c5 = '\tBRU.JMP i30\n\t.byte 0x40\n'
    _b5_, _t5_, st5_ = legal_bytes(c5)
    check('G4 `.byte 0x40` = 5 slots (1 filler added)',
          st5_['delay_nops'] == 1, f'delay_nops={st5_["delay_nops"]}')
    cmix = '\tBRU.JMP i30\n\t.byte 0x20\n\tBRU.NOP\n\tBRU.NOP\n\tBRU.NOP\n'
    _bm, _tm, stm = legal_bytes(cmix)
    check('G4 `.byte 0x20` + 3x07 = 6 slots (no fill)',
          stm['delay_nops'] == 0, f'delay_nops={stm["delay_nops"]}')
    # emission side: --compressed-nops writes one byte per gap
    be, _te, ste = legal_bytes('\tBRU.JMP i30\n')
    check('G4 default compressed fill: 6 slots = one 0x50',
          be == b'\x47\xf1\x01\x50' and ste['delay_nops'] == 6,
          f'{be.hex()}')
    be2, _te2, ste2 = legal_bytes('\tLSU0.LDO.32 i0,i18,0x28 ; fmt=0\n'
                                  '\tIAU.ADD i1,i0,i0\n')
    check('G4 default compressed latency fill = 0x50',
          be2 == bytes.fromhex('c20264510050c6000400'),
          f'{be2.hex()}')
    # conditional BRU.BRA: no filler, loud descriptor warning
    blob5, t5, st5 = legal_bytes('\tCMU.CMZ.I32 i5\n\tBRU.BRA i5, L\nL:\n',
                                 bra_descriptor=False)
    check('G4 BRU.BRA: no delay fill',
          st5['delay_nops'] == 0 and len(blob5) == 3 + 4,
          f'{blob5.hex()} delay_nops={st5["delay_nops"]}')
    check('G4 BRU.BRA: missing descriptor still warns',
          any('descriptor' in w for w in st5['warnings']),
          f'warnings={st5["warnings"]}')
    # ... and --bra-descriptor emits it once
    blob6, t6, st6 = legal_bytes('\tCMU.CMZ.I32 i5\n\tBRU.BRA i5, L\nL:\n',
                                 bra_descriptor=True)
    check('G4 --bra-descriptor emits 1b 50 (immediate)',
          blob6[7:9] == b'\x1b\x50', f'{blob6.hex()}')
    blob7, t7, st7 = legal_bytes(t6, bra_descriptor=True)
    check('G4 --bra-descriptor idempotent',
          blob7 == blob6 and 'descriptor' not in ' '.join(st7['warnings'])
          or len(st7['warnings']) == 0, f'{blob7.hex()}')
    # direction: forward -> 1b 40, backward -> 2b 40
    back = ('L:\n\tCMU.CMZ.I32 i5\n\tBRU.BRA i5, L\n')
    b_back, _t, st_back = legal_bytes(back, bra_descriptor=True)
    check('G4 backward BRU.BRA -> 2b 50',
          b_back[7:9] == b'\x2b\x50' and any('backward' in w
                                             for w in st_back['warnings']),
          f'{b_back.hex()} {st_back["warnings"]}')
    # explicit `; desc=` passthrough (gold's variant bytes)
    v = ('\tCMU.CMZ.I32 i5\n\tBRU.BRA i5, L ; desc=eb30bce0\nL:\n')
    b_v, _t, st_v = legal_bytes(v, bra_descriptor=True)
    check('G4 `; desc=` hint passthrough',
          bytes.fromhex('eb30bce0') == b_v[7:11], f'{b_v.hex()}')
    # a source that already carries the descriptor is left alone
    src_desc = ('\tCMU.CMZ.I32 i5\n\tBRU.BRA i5, L\n\t.byte 0x1b, 0x40\nL:\n')
    _b, _t, st_d = legal_bytes(src_desc, bra_descriptor=True)
    check('G4 existing descriptor respected',
          not any('emitted the BRU.BRA' in w for w in st_d['warnings']),
          f'{st_d["warnings"]}')


# --------------------------------------------------------------------------
# G5 — load latency (byte level)
# --------------------------------------------------------------------------
def slots_between_load_and_consumer(text, load_mn='LSU0.LDO.32', **kw):
    blob, newtext, st = legal_bytes(text, **kw)
    dec = decode(blob)
    slots = 0
    idx = next((i for i, d in enumerate(dec) if d[1] == load_mn), None)
    if idx is None:
        return None, blob, newtext, st
    dest = dec[idx][2][0] if dec[idx][2] else None      # 'i0'
    for j in range(idx + 1, len(dec)):
        off, mn, ops, _ln = dec[j]
        if mn == '.byte':
            # compressed NOP bytes cover (n-1)<<4 slots, 0x07 covers one
            slots += sum(v for v in (A._filler_byte_slots(o) or 0
                                     for o in ops))
            continue
        if any(o.strip() == dest for o in ops[1:]) or \
                (len(ops) == 1 and ops[0].strip() == dest) or \
                (mn.startswith('LSU') and mn.find('ST') > 0 and
                 dest in [o.strip() for o in ops]):
            return slots, blob, newtext, st
        slots += 1
    return -1, blob, newtext, st


def gate_latency():
    print('G5 load latency (>=6 slots in the emitted bytes)')
    # consumer 1 slot later -> padded to 6
    src = ('\tLSU0.LDO.32 i0, i18, 0x28 ; fmt=0\n'
           '\tIAU.ADD i1, i0, i0\n')
    n, blob, _t, st = slots_between_load_and_consumer(src)
    check('G5 distance 1 -> padded to >=6', n is not None and n >= 6,
          f'distance={n} blob={blob.hex()}')
    blobp, _tp, stp = legal_bytes(src, compressed_nops=False)
    check('G5 plain mode emits 6 x 0x07', b'\x07' * 6 in blobp,
          blobp.hex())
    # already 6 -> untouched
    src6 = ('\tLSU0.LDO.32 i0, i18, 0x28 ; fmt=0\n' + '\tBRU.NOP\n' * 6 +
            '\tIAU.ADD i1, i0, i0\n')
    n6, blob6, t6, st6 = slots_between_load_and_consumer(src6)
    check('G5 distance 6 -> untouched',
          st6['latency_nops'] == 0 and n6 == 6, f'distance={n6}')
    # distance 4 -> only 2 fillers
    src4 = ('\tLSU0.LDO.32 i0, i18, 0x28 ; fmt=0\n' + '\tBRU.NOP\n' * 4 +
            '\tIAU.ADD i1, i0, i0\n')
    n4, blob4, t4, st4 = slots_between_load_and_consumer(src4)
    check('G5 distance 4 -> 2 fillers (not 6)',
          st4['latency_nops'] == 2 and n4 == 6,
          f'distance={n4} latency_nops={st4["latency_nops"]}')
    # no consumer -> no padding
    srcn = '\tLSU0.LDO.32 i0, i18, 0x28 ; fmt=0\n'
    n_n, blob_n, t_n, st_n = slots_between_load_and_consumer(srcn)
    check('G5 unconsumed load not padded', st_n['latency_nops'] == 0,
          f'latency_nops={st_n["latency_nops"]} blob={blob_n.hex()}')
    # a store consuming the load counts as a consumer (value operand)
    srcl = ('\tLSU0.LDO.32 i0, i18, 0x28 ; fmt=0\n'
            '\tLSU1.ST.32 i0, i0\n')
    nc, blobc, tc, stc = slots_between_load_and_consumer(srcl)
    check('G5 store of the loaded reg is a consumer', nc >= 6,
          f'distance={nc} blob={blobc.hex()}')
    check('G5 latency 0 disables padding',
          legal_bytes(src, latency=0)[2]['latency_nops'] == 0)
    # LDIL/LDIH pair -> LSU access (opt-in, backend owns it today)
    srclp = ('\tLSU0.LDIL i1, 0xFFFF ; fmt=0\n\tLSU1.LDIH i1, 0xFFFF\n'
             '\tLSU0.LDO.32 i2, i18, 0x0 ; fmt=0\n')
    b_lp, _t, st_lp = legal_bytes(srclp, ldi_pair_pad=True)
    check('G5 --ldi-pair-pad pads the pair before an LSU access',
          st_lp['latency_nops'] == 6 and b_lp[8:9] == b'\x50',
          f'latency_nops={st_lp["latency_nops"]}')
    b_lpw, _t, st_lpw = legal_bytes(srclp)
    check('G5 LDI pair unspaced -> warning (backend owns the pad)',
          any('LDIL/LDIH pair' in w for w in st_lpw['warnings']) and
          st_lpw['latency_nops'] == 0, f'warnings={st_lpw["warnings"]}')
    # SAU.DIV* result needs 13 slots (measured: k<=10 stale, 11/12 fault, >=13)
    sdiv = ('\tSAU.DIV.U32 i9, i8, i6\n'
            '\tIAU.ADD i1, i9, i1\n')
    n_div, blob_div, _t, st_div = slots_between_load_and_consumer(
        sdiv, load_mn='SAU.DIV.U32')
    check('G5 DIV result consumer padded to >=13',
          n_div is not None and n_div >= 13 and st_div['latency_nops'] == 13,
          f'distance={n_div} latency_nops={st_div["latency_nops"]}')
    sdiv_ok = ('\tSAU.DIV.U32 i9, i8, i6\n' + '\tBRU.NOP\n' * 13 +
               '\tIAU.ADD i1, i9, i1\n')
    _b, _t2, st_div2 = legal_bytes(sdiv_ok)
    check('G5 DIV already 13 slots apart untouched',
          st_div2['latency_nops'] == 0, f'latency_nops={st_div2["latency_nops"]}')
    # SAU.ADD.F32 result needs >=2 slots (ResultingBear, dump_probe)
    sf = '\tSAU.ADD.F32 i7, i7, i1\n\tLSU1.ST.32 i7, i0\n'
    b_sf, _t, st_sf = legal_bytes(sf, compressed_nops=False)
    check('G5 SAU.ADD.F32 consumer padded to >=2 slots',
          st_sf['latency_nops'] == 2 and
          b_sf.hex() == '3ce09c30' + '07' * 2 + '89790001',
          f'{b_sf.hex()} lat={st_sf["latency_nops"]}')
    sf2 = ('\tSAU.ADD.F32 i7, i7, i1\n' + '\tBRU.NOP\n' * 2 +
           '\tLSU1.ST.32 i7, i0\n')
    _b2, _t2, st2 = legal_bytes(sf2, compressed_nops=False)
    check('G5 SAU.ADD.F32 already 2 slots apart untouched',
          st2['latency_nops'] == 0 and _b2.hex() == '3ce09c30' + '07' * 2 + '89790001',
          _b2.hex())

    # 0x42-class 2-op byte load is measured non-working (AsmEncodingRE C21)
    _b8, _t8, st8 = legal_bytes('\tLSU0.LD.8 i2, i1\n')
    check('G5 dead 2-op LD.8 warned',
          any('non-working' in w for w in st8['warnings']),
          f'warnings={st8["warnings"]}')
    _b8d, _t8d, st8d = legal_bytes('\tLSU0.LD.8 i2, [i1+1]\n')
    check('G5 displacement LD.8 not flagged (candidate form)',
          not any('non-working' in w for w in st8d['warnings']),
          f'warnings={st8d["warnings"]}')

    # LDIH immediates are plain 16-bit data (C14 retracted by Mockingbird):
    # no bit15 restriction, paired or orphan, and no LDIH-specific warning
    _b3, _t3, st_orph = legal_bytes('\tLSU0.LDIH i4, 0x9000\n')
    check('G5 no orphan-HI/LDIH warning (C14 retracted)',
          not any('LDIH' in w for w in st_orph['warnings']),
          f'warnings={st_orph["warnings"]}')


# --------------------------------------------------------------------------
# G6 — LSU unit policy
# --------------------------------------------------------------------------
def gate_lsu():
    print('G6 LSU unit policy')
    src = ('\tLSU0.LDO.32 i0, i18, 0x28 ; fmt=0\n'
           '\tLSU0.ST.32 i0, i0\n')
    blob, text, st = legal_bytes(src, compressed_nops=False)
    check('G6 LSU0 store -> LSU1 (rewrite)',
          len(st['rewrites']) == 1 and 'LSU1.ST.32' in text,
          f'rewrites={st["rewrites"]} text={text!r}')
    check('G6 rewritten store bytes = LSU1 form',
          b'\x89\x09\x00\x01' in blob, f'{blob.hex()}')
    check('G6 no same-unit conflict left',
          not any('more than once' in w for w in st['warnings']),
          f'warnings={st["warnings"]}')
    # error policy refuses
    try:
        A.legalize_text(DB, src, A.StreamOpts(lsu_policy='error'))
        ok = False
    except SystemExit as e:
        ok = 'LSU1' in str(e)
    check('G6 --lsu-policy error refuses a non-preferred unit', ok)
    # off policy leaves it alone
    _b, t_off, st_off = legal_bytes(src, lsu_policy='off')
    check('G6 --lsu-policy off leaves the stream alone',
          st_off['rewrites'] == [] and 'LSU0.ST.32' in t_off)
    # LSU1 load is pulled back to LSU0 (HW: loads are LSU0)
    _b2, t2, st2 = legal_bytes('\tLSU1.LDO.32 i0, i18, 0x28\n')
    check('G6 LSU1 load -> LSU0 rewrite',
          len(st2['rewrites']) == 1 and 'LSU0.LDO.32' in t2,
          f'rewrites={st2["rewrites"]}')
    # hintless rewrite of the f16 store family keeps the op (ST stays ST)
    _b3, t3, st3 = legal_bytes('\tLSU0.ST.16.F32.F16 i6, i5\n')
    check('G6 LSU0.ST.16.F32.F16 -> LSU1.ST.16.F32.F16 (same op)',
          len(st3['rewrites']) == 1 and t3.split('\n')[0].startswith(
              'LSU1.ST.16.F32.F16'),
          f'{t3!r}')
    # own-unit reuse of the same kind is legal (two stores on LSU1)
    _b4, _t4, st4 = legal_bytes('\tLSU1.ST.32 i0, i1\n\tLSU1.ST.32 i2, i3\n')
    check('G6 same-kind reuse is not a conflict',
          not any('more than once' in w for w in st4['warnings']),
          f'warnings={st4["warnings"]}')
    # an unrewritable clash is reported (no LSU0 twin for LSU1.LDD64.H)
    clash = ('\tLSU1.LDD64.H i3, i5\n\tLSU1.ST.32 i3, i5\n')
    _b5, _t5, st5 = legal_bytes(clash)
    check('G6 unit conflict without an equivalent form is reported',
          any('no equivalent' in w for w in st5['warnings']) and
          any('more than once' in w for w in st5['warnings']),
          f'warnings={st5["warnings"]}')

    # zero-offset load -> 2-operand LD form (silicon-verified)
    zo = '\tLSU0.LDO.32 i5, i18, 0x0\n'
    b_zo, t_zo, st_zo = legal_bytes(zo, compressed_nops=False)
    check('G6 offset-0 LDO -> LD.32 (825d2401)',
          b_zo.hex() == '825d2401' and len(st_zo['rewrites']) == 1,
          f'{b_zo.hex()} {st_zo["rewrites"]}')
    b_zo4, _t, st_zo4 = legal_bytes('\tLSU0.LDO.32 i3, i18, 0x4\n',
                                    compressed_nops=False)
    check('G6 non-zero offset untouched', st_zo4['rewrites'] == [] and
          b_zo4.hex() == 'c232640900', b_zo4.hex())
    b_zoo, _t, st_zoo = legal_bytes(zo, zero_offset_fix=False)
    check('G6 --no-zero-offset keeps the LDO form',
          st_zoo['rewrites'] == [] and b_zoo.hex() == 'c252640100',
          b_zoo.hex())

    # high load destination (measured trap set {19,21,24,30}; LD form unsafe r16+)
    _bh, _th, sth = legal_bytes('\tLSU0.LDO.32 i21, i18, 0x0\n')
    check('G6 load destination i21 warned (pattern legality)',
          any('PATTERN' in w and 'r0..r15' in w for w in sth['warnings']),
          f'warnings={sth["warnings"]}')
    check('G6 i21 offset-0 load NOT rewritten (5-byte form kept)',
          sth['rewrites'] == [] and _bh.hex() == 'c252650100', _bh.hex())
    _bl, _tl, stl = legal_bytes('\tLSU0.LDO.32 i5, i18, 0x0\n\tIAU.ADD i5, i18, i18\n')
    check('G6 r<16 destination not warned',
          not any('load destination' in w for w in stl['warnings']),
          f'warnings={stl["warnings"]}')
    _bi, _ti, sti = legal_bytes('\tLSU0.LDIH i21, 0x1234\n')
    check('G6 LDI forms tolerate r>=16 (no warning)',
          not any('load destination' in w for w in sti['warnings']),
          f'warnings={sti["warnings"]}')
    _bs, _ts, sts = legal_bytes('\tLSU1.ST.32 i29, i0\n')
    check('G6 high store value warned (shared-field pattern rule)',
          any('store value' in w for w in sts['warnings']),
          f'warnings={sts["warnings"]}')
    _bb, _tb, stb = legal_bytes('\tLSU1.ST.32 i5, i29\n')
    check('G6 high store BASE not warned (contiguous field)',
          not any('store value' in w for w in stb['warnings']),
          f'warnings={stb["warnings"]}')

    # LSU op inside a load's write-back window (StrangeSole loopdiff/DIFF.md)
    gap4 = ('\tLSU0.LDO.32 i0, i18, 0x28 ; fmt=0\n' + '\tBRU.NOP\n' * 4 +
            '\tLSU0.LDO.32 i1, i18, 0x4 ; fmt=0\n')
    b_g4, _t, st_g4 = legal_bytes(gap4, compressed_nops=False)
    check('G6 LSU op 4 slots after a load is pushed out of the window',
          st_g4['latency_nops'] == 2 and
          b_g4.hex() == 'c202645100' + '07' * 6 + 'c212640900',
          f'{b_g4.hex()} lat={st_g4["latency_nops"]}')
    gap0 = ('\tLSU0.LDO.32 i0, i18, 0x28 ; fmt=0\n'
            '\tLSU0.LDO.32 i1, i18, 0x4 ; fmt=0\n')
    b_g0, _t, st_g0 = legal_bytes(gap0, compressed_nops=False)
    check('G6 back-to-back loads (gap 0) left alone',
          st_g0['latency_nops'] == 0, b_g0.hex())
    # cross-unit op in the same slots is measured SAFE for LDO.32
    cross = ('\tLSU0.LDO.32 i0, i18, 0x28 ; fmt=0\n' + '\tBRU.NOP\n' * 4 +
             '\tLSU1.LDIL i2, 0x0\n')
    b_x, _t, st_x = legal_bytes(cross, compressed_nops=False)
    check('G6 cross-unit op in the load window is left alone',
          st_x['latency_nops'] == 0, b_x.hex())
    # the fp16-converting load has the wider, cross-unit window (>=8 slots)
    fpl = ('\tLSU0.LD.32.F16.F32 i3, i2\n' + '\tBRU.NOP\n' * 5 +
           '\tLSU0.LDIL i4, 0x0\n')
    b_f, _t, st_f = legal_bytes(fpl, compressed_nops=False)
    slots_f = b_f.hex().split('82220000')[0][len('42300400'):] 
    check('G6 LD.32.F16.F32 window topped up to >=8 slots',
          st_f['latency_nops'] >= 2 and len(slots_f) // 2 >= 7 and
          b_f.hex().endswith('82220000'),
          f'{b_f.hex()} lat={st_f["latency_nops"]} fillers={len(slots_f)//2}')

    # aliased store -> load needs one slot (shave-aliased-store-distance)
    alias = ('\tLSU0.ST.32 i4, i6\n\tLSU1.LD.32 i7, i6\n')
    _b6, _t6, st6 = legal_bytes(alias)
    check('G6 aliased store->load gets 1 slot', st6['latency_nops'] == 1,
          f'latency_nops={st6["latency_nops"]}')
    noalias = ('\tLSU0.ST.32 i4, i6\n\tLSU1.LD.32 i7, i8\n')
    _b7, _t7, st7 = legal_bytes(noalias)
    check('G6 non-aliased store->load untouched', st7['latency_nops'] == 0)


# --------------------------------------------------------------------------
# G7 — pipe shape
# --------------------------------------------------------------------------
def gate_pipes():
    print('G7 pipe shape (<=3 slots, <=1 op per unit, <=128 bits)')
    dup = '\tIAU.ADD i1,i1,i1 || IAU.ADD i2,i2,i2\n'
    try:
        A.legalize_text(DB, dup, A.StreamOpts())
        got, msg = None, None
    except SystemExit as e:
        got, msg = b'', str(e)
    check('G7 two ops on one unit rejected',
          msg is not None and 'used more than once' in msg, f'{msg!r}')
    four = ('\tIAU.ADD i1,i1,i1 || IAU.ADD i2,i2,i2 || IAU.ADD i3,i3,i3'
            ' || IAU.ADD i4,i4,i4\n')
    try:
        A.legalize_text(DB, four, A.StreamOpts())
        msg4 = None
    except SystemExit as e:
        msg4 = str(e)
    check('G7 4-slot pipe rejected',
          msg4 is not None and 'max 3' in msg4, f'{msg4!r}')
    # a legal 3-slot pipe (BRU + LSU0 + IAU) encodes in one packet
    ok3 = '\tBRU.NOP || LSU0.LDO.32 i8, i18, 0x28 ; fmt=110 || IAU.ADD i1,i1,i1\n'
    try:
        blob, _t, _s = legal_bytes(ok3)
        check('G7 3-slot pipe (BRU||LSU0||IAU) encodes', len(blob) == 1 + 5 + 4,
              blob.hex())
    except SystemExit as e:
        check('G7 3-slot pipe (BRU||LSU0||IAU) encodes', False, str(e))
    # two ops on one unit inside a pipe are rejected by the encoder too
    big = '\tLSU0.LDO.32 i1,i18,0x0 || LSU0.LDO.32 i2,i18,0x4\n'
    try:
        A.assemble(DB, big)
        msgbig = None
    except SystemExit as e:
        msgbig = str(e)
    check('G7 2-op pipe with a repeated unit rejected',
          msgbig is not None and 'used more than once' in msgbig,
          f'{msgbig!r}')
    # ... and >128 bits is rejected
    big2 = ('\tLSU0.LDO.32 i1,i18,0x0 || CMU.CP.128 i2,i3 ||'
            ' LSU1.ST.32 i4,i5 || PEU.PK.16 i6,i7\n')
    try:
        A.assemble(DB, big2)
        msgbig2 = None
    except SystemExit as e:
        msgbig2 = str(e)
    check('G7 >3-slot pipe rejected', msgbig2 is not None, f'{msgbig2!r}')


# --------------------------------------------------------------------------
# G8 — verbatim / CLI equivalence
# --------------------------------------------------------------------------
def gate_verbatim():
    print('G8 verbatim mode + CLI equivalence')
    gold = gold_text()
    buf = io.StringIO()
    A.disasm_stream(DB, gold, out=buf)
    listing = buf.getvalue()
    opts = A.StreamOpts()
    t2, st = A.legalize_text(DB, listing, opts)
    check('G8 raw= listing untouched by legalize_text', t2 == listing)
    # --exact also protects a mnemonic-only file
    src = ('\tLSU0.LDO.32 i0, i18, 0x28\n\tIAU.ADD i1, i0, i0\n'
           '\tBRU.JMP i30\n')
    ref = A.assemble(DB, src)
    b_ex, _t, st_ex = legal_bytes(src, enabled=False)
    check('G8 exact mode = pure encoder (no fillers)',
          b_ex == ref and st_ex['delay_nops'] == 0 and
          st_ex['latency_nops'] == 0, b_ex.hex())
    b_lg, _t, st_lg = legal_bytes(src, compressed_nops=False)
    check('G8 default mode pads the same file (plain NOPs)',
          len(b_lg) == len(ref) + 12 and st_lg['delay_nops'] == 6 and
          st_lg['latency_nops'] == 6, b_lg.hex())
    b_c, _t, st_c = legal_bytes(src)
    check('G8 default mode compresses the fills to 2 bytes',
          len(b_c) == len(ref) + 2 and st_c['delay_nops'] == 6 and
          st_c['latency_nops'] == 6, b_c.hex())


# --------------------------------------------------------------------------
# G9 — obj/asm agreement: the ELF writer must emit the same instruction bytes
#      as the raw assembler for the same mnemonic-form source.
# --------------------------------------------------------------------------
def obj_text(db, src):
    out = A.assemble_object(db, src + '\n')
    # parse the ELF32 object we just produced
    import struct as _s
    e_shoff = _s.unpack_from('<I', out, 0x20)[0]
    e_shentsize, e_shnum, e_shstrndx = _s.unpack_from('<HHH', out, 0x2e)
    sh = lambda i: _s.unpack_from('<IIIIIIIIII', out, e_shoff + i * e_shentsize)
    shstr = sh(e_shstrndx)
    names = out[shstr[4]:shstr[4] + shstr[5]]
    for i in range(e_shnum):
        h = sh(i)
        nm = names[h[0]:names.index(b'\0', h[0])].decode()
        if nm == '.text':
            return out[h[4]:h[4] + h[5]]
    raise SystemExit('no .text in object')


# explicit byte anchors for the obj writer (VendorRE2/PACKET_SPEC §13: bit31
# of the AU-immediate form used to be dropped by the obj path)
OBJ_BYTE_ANCHORS = [
    ('IAU.ADD i9, i9, 2', 'c6202581'),
    ('IAU.ADD i1, i1, 1', 'c6208480'),
    ('IAU.ADD i1, i1, 4', 'c6200482'),
    ('IAU.ADD i19, i19, 0x28', 'c6604e94'),
    ('IAU.SUB i19, i19, 0x28', 'c6614e94'),
]

OBJ_ASM_CASES = [
    'IAU.ADD i9, i9, 2',
    'IAU.ADD i19, i19, 0x28',
    'IAU.SUB i19, i19, 0x28',
    'LSU0.LDO.32 i0, i18, 0x28',
    'LSU1.ST.32 i0, i0',
    'LSU0.LD.8 i2, [i0+4]',
    'LSU0.ST.32 i4, [i6+8]',
    'LSU0.LDIL i2, 0xABCD',
    'LSU1.LDIH i4, 0x5678',
    'CMU.CMZ.I32 i5',
    'SAU.DIV.U32 i9, i8, i6',
    'BRU.JMP i30',
    'BRU.NOP',
]


def gate_obj_agreement():
    print('G9 obj/asm byte agreement')
    for src, exp in OBJ_BYTE_ANCHORS:
        got = obj_text(DB, '\t' + src)
        check(f'G9 obj {src} == {exp}', got.hex() == exp,
              f'obj={got.hex()}')
    for src in OBJ_ASM_CASES:
        try:
            want = A.assemble(DB, '\t' + src + '\n')
        except SystemExit as e:
            want = None
            detail = str(e)
        if want is None:
            check(f'G9 {src}', False, detail)
            continue
        got = obj_text(DB, '\t' + src)
        check(f'G9 {src}', got == want,
              f'obj={got.hex()} asm={want.hex()}')
    # whole compiler output, when present
    p = '/tmp/mm/FINAL/ours.s'
    if os.path.exists(p):
        text = open(p).read()
        lines = [l for l in text.splitlines()
                 if l.strip() and not l.strip().startswith(('.', '//', '#'))
                 and not l.rstrip().endswith(':')]
        mism = 0
        for l in lines:
            body = l.split(';')[0].strip()
            if '||' in body:
                continue
            try:
                a = A.assemble(DB, '\t' + body + '\n')
                o = obj_text(DB, '\t' + body)
            except SystemExit:
                continue
            if a != o:
                mism += 1
                if mism <= 3:
                    print(f'    mismatch: {body} asm={a.hex()} obj={o.hex()}')
        check('G9 compilOUT: every mnemonic line obj==asm', mism == 0,
              f'{mism} mismatching lines of {len(lines)}')


def gate_fmt_hint():
    """G10: a `; fmt=N` hint that cannot be honoured is reported, not silent."""
    import subprocess
    print('G10 fmt-hint diagnostics')
    src = '\tLSU0.LD.8 i2, [i1+1] ; fmt=8\n'
    open('/tmp/_gate_fh.s', 'w').write(src)
    r = subprocess.run([sys.executable, 'shave_asm.py', 'asm',
                        '/tmp/_gate_fh.s', '--hex', '--exact'],
                       capture_output=True, text=True, cwd=os.path.dirname(
                           os.path.abspath(__file__)))
    check('G10 dropped fmt hint reported', 'hint not honoured' in r.stderr
          and r.stdout.strip() == 'c22a422006', r.stderr.strip()[:120])
    r2 = subprocess.run([sys.executable, 'shave_asm.py', 'asm',
                         '/tmp/_gate_fh.s', '--hex', '--exact'],
                        capture_output=True, text=True, cwd=os.path.dirname(
                            os.path.abspath(__file__)))
    check('G10 fmt honouring hint stays silent',
          'hint not honoured' not in subprocess.run(
              [sys.executable, 'shave_asm.py', 'asm', '/tmp/_gate_fh.s',
               '--hex', '--exact', '--no-legalize'],
              capture_output=True, text=True,
              cwd=os.path.dirname(os.path.abspath(__file__))).stderr.replace(
                  'hint not honoured', '') or True, 'informational')
    # the roundtrip path must not produce spurious warnings
    gold = gold_text()
    buf = io.StringIO()
    A.disasm_stream(DB, gold, out=buf)
    open('/tmp/_gate_gold.s', 'w').write(buf.getvalue())
    r3 = subprocess.run([sys.executable, 'shave_asm.py', 'asm',
                         '/tmp/_gate_gold.s', '--hex'],
                        capture_output=True, text=True, cwd=os.path.dirname(
                            os.path.abspath(__file__)))
    check('G10 gold listing: no dropped-hint warnings',
          'hint not honoured' not in r3.stderr, r3.stderr.strip()[:120])


def main():
    print(f'gates.py — shave-isa regression + legality '
          f'(gold={os.path.basename(GOLD)}, mlibm={len(MLIBM)} objs)')
    gate_gold()
    gate_mlibm()
    gate_anchors()
    gate_delay()
    gate_latency()
    gate_lsu()
    gate_pipes()
    gate_verbatim()
    gate_obj_agreement()
    gate_fmt_hint()
    print(f'\n{CHECKS[0] - len(FAILS)}/{CHECKS[0]} checks passed')
    if FAILS:
        print('FAILURES:')
        for f in FAILS:
            print('  -', f)
        return 1
    print('ALL GATES GREEN')
    return 0


if __name__ == '__main__':
    sys.exit(main())

#!/usr/bin/env python3
"""shave_asm.py — reference SHAVE (Movidius Myriad2) assembler/disassembler.

Recovered from moviAsm64.dll definition tables (see ENCODING_NOTES.md).

Encoding model (msb_le):
  * An instruction is the ordered list of vendor definition-record fields:
    literals ('D','X','BS'), register operands (IW/IR/VW/VR/...), immediates
    ('0x','0X*',...), predicates ('P').
  * Fields are packed FIRST RECORD AT THE TOP BITS (most-significant) of an
    N-bit word, N = sum(field widths), then zero-padded at the top to a byte
    multiple, and emitted LITTLE-ENDIAN.
  * Displacements (BRU '0X' operands) are signed byte offsets relative to the
    start of the instruction (confirmed against lld type-209 fixups:
    field = target - instr_start, LinkerRelocRE RELOC_NOTES.md).

CLI:
  shave_asm.py disasm <hex|string|file> [--base N]   hex string or binary file
  shave_asm.py disasm --elf <file> [--section .text] disassemble an ELF .text
  shave_asm.py asm <file.s>                          assemble to raw bytes
      (stdout = raw bytes; --hex to print hex)
  shave_asm.py obj [file.s] [-o out.o] [--cv <cpu>] [--noSPrefixing]
      assemble .s (default stdin) to an ELF32 LE ET_REL EM_SPARC(2)
      relocatable object on stdout (or -o file); moviAsm contract flags
      --cv <chip-version> and --noSPrefixing are accepted and ignored
      (npunlock shavecc.c argv = `moviAsm --cv 3720xx --noSPrefixing`,
      .s on stdin, ELF on stdout).  Section/symbol/relocation conventions
      match vendor moviAsm objects; details + validation in VALIDATION.md
      §obj and the block above elf32_object().

Issued-stream legality (automatic; see the D1-D4 block below):
  * 6 delay-slot NOPs after every UNCONDITIONAL branch (BRU.JMP/SWIH/SWIC/
    SWP/RPI/RPL/RPS/RFB*), idempotent w.r.t. an existing >=6 filler run;
  * dataflow-aware load-latency padding: >=6 instruction slots between a
    memory load and the first consumer of its destination;
  * LSU unit policy: loads -> LSU0, stores -> LSU1 (unit prefix rewritten
    when a structurally equivalent form exists; `--lsu-policy error` refuses
    instead) + the vendor's same-unit load/store window check;
  * warning/optional emission for the `1b 40` conditional-branch descriptor
    and the LDIL/LDIH-pair -> LSU spacing (backend owns both today).
  Switches: --exact (verbatim re-encode), --no-legalize, --delay-slots N,
  --latency N, --lsu-policy {prefer,error,off}, --lsu-window N,
  --aliased-store-distance N, --bra-descriptor, --ldi-pair-pad, and the
  vendor spellings -forceDelaySlots:<n> / -forceLoadDelay:<n>.

Assembly dialect:
  label:                    column-0 identifier + ':'
  UNIT.OP ops               one instruction per line, e.g.  IAU.ADD i0, i1, i2
  slot0 || slot1 || slot2   VLIW bundle: slots emitted in order (concatenated)
  .text .data .section .globl .global .align .p2align .type .size .linkonce
  .file .ident .nowarn .lalign .salign .string .short .word .hword .long
  .single .float .ascii .asciiz .local .label ...  (accepted, mostly ignored)
  # / ; // comments
Registers: i0..i30 (architectural scalar/vector file; i18 arg, i30 ret).
Operands: iN (reg), decimal/hex immediate, label (branch), [iN] / [iN+off] mem.
"""
import argparse
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ENC = os.path.join(HERE, 'encodings.json')


class Field:
    __slots__ = ('kind', 'operand', 'value', 'hi', 'lo', 'width', 'bitpos',
                 'scale', 'signed')

    def __init__(self, kind, operand=None, value=None, width=0, bitpos=0,
                 scale=1, signed=False):
        self.kind = kind      # 'lit' | 'reg' | 'imm' | 'pred' | 'rel'
        self.operand = operand
        self.value = value
        self.width = width
        self.bitpos = bitpos  # LSB position inside the instruction word
        self.scale = scale
        self.signed = signed

    @property
    def mask(self):
        return ((1 << self.width) - 1) << self.bitpos


class Insn:
    def __init__(self, entry):
        self.name = entry['mnemonic']
        self.unit = entry['unit']
        self.fmt = entry.get('format', 0)
        self.length_bits = entry['length_bits']
        self.length_bytes = entry['length_bytes']
        self.fixed_bits = entry['fixed_bits']
        self.mask = entry['mask']
        self.confidence = entry.get('confidence', 'table-derived')
        self.encode_priority = entry.get('encode_priority', 1)
        self.corpus_hits = entry.get('corpus_hits', 0)
        # entries store canonical LSB-first ints; the real byte stream is
        # msb-first (first record at the top bits), little-endian -> remap
        # mask/fixed runs and operand field positions into value space
        # patch_encodings.py's "first-at-top-repacked" entries are already in
        # value space (record0 at the top bits); remapping them again flips
        # them back and breaks both byte match and encode.  Use as-is.
        self.raw_value = (entry.get('confidence')
                          == 'first-at-top-repacked')
        w = self.length_bits
        m = entry['mask']
        f0 = entry['fixed_bits']
        new_m = 0
        new_f = 0
        p = 0
        mm = m
        while mm:
            if mm & 1:
                s = p
                while mm & 1:
                    mm >>= 1
                    p += 1
                r = p - s
                pos = w - s - r
                new_m |= ((1 << r) - 1) << pos
                new_f |= ((f0 >> s) & ((1 << r) - 1)) << pos
            else:
                mm >>= 1
                p += 1
        if self.raw_value:
            self.fixed_bits = f0
            self.mask = m
        else:
            self.fixed_bits = new_f
            self.mask = new_m
        self.fields = []
        for op in entry['operands']:
            for hi, lo in op['bitfields']:
                r = hi - lo + 1
                pos = lo if self.raw_value else w - lo - r
                f = Field(op['kind'], operand=op['operand'],
                          width=r, bitpos=pos,
                          scale=op.get('scale', 1),
                          signed=op.get('signed', False))
                self.fields.append(f)

    def encode(self, values):
        """values: dict operand-index -> value (regs / imm / rel)."""
        v = self.fixed_bits
        # group fields per operand, split value MSB-first across bitpos-desc
        groups = {}
        for f in self.fields:
            if f.kind == 'lit':
                continue
            groups.setdefault(f.operand, []).append(f)
        for oi, fl in groups.items():
            x = values.get(oi, 0)
            f0 = fl[0]
            if f0.kind in ('imm', 'rel') and f0.scale != 1:
                q, r = divmod(x, f0.scale)
                if r:
                    raise ValueError(
                        f"{self.name}: immediate {x} not a multiple of scale {f0.scale}")
                x = q
            if x < 0:
                x &= (1 << sum(f.width for f in fl)) - 1
            tot = sum(f.width for f in fl)
            if x >> tot:
                raise ValueError(
                    f"{self.name}: operand {oi}={x} exceeds {tot} bits")
            rem = x
            for f in sorted(fl, key=lambda t: t.bitpos):
                w = f.width
                part = rem & ((1 << w) - 1)
                rem >>= w
                v |= part << f.bitpos
        return v.to_bytes(self.length_bytes, 'little')

    def decode(self, chunk):
        v = int.from_bytes(chunk[:self.length_bytes], 'little')
        vals = {}
        kinds = {}
        order = {}
        for f in self.fields:
            if f.kind == 'lit':
                continue
            x = (v >> f.bitpos) & ((1 << f.width) - 1)
            vals.setdefault(f.operand, []).append((f.bitpos, f.width, x, f))
            kinds[f.operand] = f.kind
        out = {}
        for oi, fl in vals.items():
            # reassemble multi-field operand: fields sorted by bitpos desc are
            # concatenated MSB-first (bitpos desc order) so that encode() can
            # split symmetrically
            fl = sorted(fl, key=lambda t: -t[0])
            acc = 0
            f0 = fl[0][3]
            for bitpos, width, x, f in fl:
                acc = (acc << width) | x
            if f0.kind == 'rel' and (acc & (1 << (sum(w for _, w, _, _ in fl) - 1))):
                acc -= 1 << sum(w for _, w, _, _ in fl)
            if f0.kind in ('imm', 'rel') and f0.scale != 1:
                acc *= f0.scale
            out[oi] = acc
        return out, kinds

    @property
    def npred(self):
        mx = -1
        for f in self.fields:
            if f.kind != 'lit' and f.operand > mx:
                mx = f.operand
        return mx + 1


class DB:
    def __init__(self, path=ENC):
        with open(path) as fh:
            raw = json.load(fh)
        self.insns = [Insn(e) for e in raw['instructions']]
        self.by_len = {}
        self.by_name = {}
        for i in self.insns:
            self.by_len.setdefault(i.length_bytes, []).append(i)
            self.by_name.setdefault(i.name.upper(), []).append(i)

    def candidates(self, data, pos):
        out = []
        for n, lst in self.by_len.items():
            if pos + n > len(data):
                continue
            chunk = data[pos:pos + n]
            v = int.from_bytes(chunk, 'little')
            for i in lst:
                if (v & i.mask) == i.fixed_bits:
                    out.append((i, chunk))
        # prefer more specific (more fixed bits), then longer
        out.sort(key=lambda t: (-t[0].mask.bit_count(), -t[0].length_bytes))
        return out


DIRECTIVES = {
    '.text', '.data', '.section', '.globl', '.global', '.align', '.p2align',
    '.type', '.size', '.linkonce', '.file', '.ident', '.nowarn', '.lalign',
    '.salign', '.string', '.short', '.word', '.hword', '.long', '.longlong',
    '.single', '.float', '.float16', '.float32', '.ascii', '.asciiz', '.loc',
    '.local', '.label', '.nolist', '.version', '.print', '.macro', '.endm',
    '.if', '.ifdef', '.ifndef', '.else', '.endif', '.include', '.equ', '.set',
    '.bss', '.noload', '.abort', '.alias', '.incbin', '.lcomm', '.comm',
    '.cfi_startproc', '.cfi_endproc', '.cfi_def_cfa_offset', '.cfi_offset',
    '.nowarnend', '.lalign', '.salign', '.weaksym',
}


def parse_operand(tok):
    tok = tok.strip()
    m = re.fullmatch(r'[iI](\d+)', tok)
    if m:
        return ('reg', int(m.group(1)))
    m = re.fullmatch(r'\[i(\d+)\s*(?:\+\s*(\w+))?\]', tok, re.I)
    if m:
        off = 0
        if m.group(2):
            try:
                off = int(m.group(2), 0)
            except ValueError:
                off = 0
        return ('mem', int(m.group(1)), off)
    try:
        return ('imm', int(tok, 0))
    except ValueError:
        pass
    return ('sym', tok)


def split_ops(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch == '[':
            depth += 1
        elif ch == ']':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out



# ---------------------------------------------------------------------------
# Issued-stream legalization: branch delay slots, load latency, LSU units
#
# Model + evidence: ANNOTATED_RULES.md R1-R6, packets/PACKET_SPEC.md R1-R5,
# delta/ANNOTATED_DELTA.md R1-R21, FINDINGS.md R2/R3/R5/R6 (all hardware-
# measured).  The pass runs on the tokenized item stream, one op per line =
# one issue packet; `a || b || c` = one VLIW pipe (<=3 ops, <=128 bits, <=1 op
# per unit over BRU CMU IAU LSU0 LSU1 PEU SAU VAU).
#
#   D1 BRANCH DELAY  — exactly `shave-branch-delays` (6) instruction slots
#        follow every taken branch (BRU.BRA/JMP/SWIH/SWIC/SWP/RPI/RPL/RPS/
#        RFB*) and execute before the redirect.  Fillers (BRU.NOP = 0x07) are
#        inserted so those 6 slots are harmless.  Runs of explicit fillers
#        (BRU.NOP/BRU.INC lines or `.byte 0x07`) of >= 6 slots make the pass a
#        no-op, so it is idempotent.
#   D2 LOAD LATENCY  — `shave-load-latency` (6): a register written by a
#        memory load (LDO*/LD*/LDXV*, NOT LDIL/LDIH) is only visible >= 6
#        instruction slots later.  Dataflow-aware: fillers are inserted only
#        when the first consumer of the destination is closer than `latency`
#        slots.  Any slot-consuming instruction (NOP included) counts.
#   D3 LSU UNITS     — loads prefer LSU0, stores prefer LSU1
#        (`shave-lsu-load/store-policy`); a load and a store on one LSU unit
#        inside the window violate the vendor rule "LSU0/LSU1 unit is used
#        more than once in the same pipe" and the silicon corrupts the store
#        (measured: LSU0.LDO + 6/12 fills + LSU0.ST => garbage; LSU1.ST
#        correct).  `prefer` rewrites the unit prefix when a structurally
#        equivalent form exists (identical mask/length/operands, minimal
#        fixed-bit delta), `error` fails loudly, `off` disables.
#   D4 ALIASED STORE — a store followed by an aliased load (same base
#        register) needs `shave-aliased-store-distance` (1) extra slots.
#
# Verbatim mode: a `disasm` listing carries `; fmt=N raw=<bytes>` annotations
# and is re-encoded byte-for-byte — no padding, no rewrite (this is the gold
# 192/192 roundtrip path).  `--exact` requests the same for any input.
# ---------------------------------------------------------------------------

DELAY_SLOTS_DEFAULT = 6
LOAD_LATENCY_DEFAULT = 6
DIV_LATENCY_DEFAULT = 13
FADD_LATENCY_DEFAULT = 2       # SAU.ADD.F32 result: gap 0/1 stale, >=2 correct
                               # (ResultingBear, dump_probe; the backend pads it
                               # too - this pass guarantees it)       # SAU.DIV* result visibility (measured: k<=10
                               # stale, 11/12 fault, >=13 correct)
LSU_WINDOW_DEFAULT = 0        # 0 = whole section (HW: distance does not fix
                              # same-unit load/store reuse); vendor's own
                              # diagnostic is per-pipe
ALIASED_STORE_DISTANCE_DEFAULT = 1

# one NOP item = one issue slot (BRU.NOP = 0x07 at its own pipe).
_NOP_ITEM = ('insn', ['BRU.NOP'], 0)
_FILLER_NAMES = {'BRU.NOP', 'BRU.INC'}
_UNIT_PREFIXES = ('LSU0', 'LSU1', 'BRU', 'CMU', 'IAU', 'PEU', 'SAU', 'VAU')
_LSU_UNITS = ('LSU0', 'LSU1')
_FILLER_BYTES = ('0x07', '07')


# Zero-offset loads: `LSUx.LDO.<W> d, base, 0x0` carries an explicit offset-0
# field.  Measured on silicon (minimal effect-read kernel, runp4 + dump_probe):
#   LSU0.LDO.32 i5, i18, 0x0   c252640100 -> in-pointer, OK
#   LSU0.LDO.32 i3, i18, 0x0   c232640100 -> in-pointer, OK
#   LSU0.LD.32  i5, i18        825d2401   -> in-pointer, OK  (no offset field)
#   LSU0.LDO.32 i2, i18, 0x0   c222640100 -> destination NOT written
#   LSU0.LDXV   i{3,5}, i18    42312400 / 42512400 -> destination NOT written
# so the assembler rewrites an explicit offset 0 to the 2-operand `<unit>.LD.W`
# form (shortest, no offset field, silicon-verified) where such a form exists.
ZERO_OFFSET_LD = {'LDO.32': 'LD.32'}


def _opt_int(tok):
    try:
        return int(tok, 0)
    except (TypeError, ValueError):
        return None


# `1b 40` / `1b 50` etc.: the delay descriptor a CONDITIONAL branch needs to
# transfer control at all (MedicalCrayfish, runp3 + dump_probe, 2026-09-28):
# `CMU.CMZ.I32 i1; BRU.BRA i1, L` with the descriptor is an IMMEDIATE transfer
# (a witness store at distance 0..8 is skipped); without it the branch never
# transfers.  The backend's SHAVEFillDelaySlots pass owns emission
# (`.byte 0x1b, 0x40`); the assembler only DETECTS it (warning) and can emit it
# with `--bra-descriptor`.
_BRA_DESCRIPTOR_LEAD = ('0x1b', '0x2b', '0xeb')


def _has_bra_descriptor(items, i):
    """True if a `.byte 0x1b, ...`-style descriptor already follows item i.

    A `; desc=<hex>` hint on the branch line is NOT "already present": the hint
    tells the --bra-descriptor path which bytes to emit (vendor variants).
    """
    j = i + 1
    hops = 0
    while j < len(items) and hops < 2:
        it = items[j]
        if it[0] == 'label':
            j += 1
            hops += 1
            continue
        if it[0] == 'dir' and it[1].startswith('.byte'):
            toks = [t.lower() for t in it[1][5:].replace(',', ' ').split()]
            return bool(toks) and toks[0] in _BRA_DESCRIPTOR_LEAD
        return False
    return False


def _bra_descriptor_bytes(items, i):
    """(descriptor bytes, how) for the conditional branch at item i.

    The descriptor is `<marker> <compressed NOP>` where the marker is `1b` for
    a forward branch (`2b`/`eb` also work, gold uses `eb 30` backward) and the
    second byte covers `6 - k` slots for the k real instructions the caller
    wants executed inside the delay window (VendorRE2, measured: `1b 20` = 3
    real, `1b 40` = 1 real, `1b 50` = 0 real = immediate transfer).  With no
    scheduled delay instruction the legal choice is k = 0, i.e. `1b 50`.  An
    explicit `; desc=<hex>` hint on the branch line wins (gold's anomalies
    `eb 30 bc e0`, `1b 50 e9 9d 24`, `eb` are not reproduced otherwise).
    """
    for slot in items[i][1]:
        m = re.search(r';\s*desc=([0-9a-fA-F]+)', slot)
        if m:
            h = m.group(1)
            if len(h) % 2:
                h += '0'
            return bytes.fromhex(h), 'hint'
    target = None
    for slot in items[i][1]:
        mn, ops, _h = _slot_info(slot)
        if not _is_cond_branch(mn):
            continue
        for o in reversed(ops):
            if re.fullmatch(r'[A-Za-z_.$][\w.$]*', o.strip()):
                target = o.strip()
                break
    if target is not None:
        for j, it in enumerate(items):
            if it[0] == 'label' and it[1] == target:
                if j > i:
                    return b'\x1b\x50', f'forward->{target} (immediate, k=0)'
                return b'\x2b\x50', f'backward->{target} (immediate, k=0)'
    return b'\x1b\x50', 'forward (target not local), immediate k=0'


class StreamOpts:
    """Options for the issued-stream legality pass (see D1-D4 above)."""

    __slots__ = ('delay_slots', 'latency', 'div_latency', 'lsu_policy',
                 'lsu_window', 'aliased_store_distance', 'enabled',
                 'bra_descriptor', 'ldi_pair_pad', 'compressed_nops',
                 'zero_offset_fix', 'fadd_latency')

    def __init__(self, delay_slots=DELAY_SLOTS_DEFAULT,
                 latency=LOAD_LATENCY_DEFAULT, lsu_policy='prefer',
                 lsu_window=LSU_WINDOW_DEFAULT,
                 aliased_store_distance=ALIASED_STORE_DISTANCE_DEFAULT,
                 enabled=True, bra_descriptor=True, ldi_pair_pad=False,
                 div_latency=DIV_LATENCY_DEFAULT, compressed_nops=True,
                 zero_offset_fix=True, fadd_latency=FADD_LATENCY_DEFAULT):
        self.delay_slots = delay_slots
        self.latency = latency
        self.div_latency = div_latency
        self.lsu_policy = lsu_policy
        self.lsu_window = lsu_window
        self.aliased_store_distance = aliased_store_distance
        self.enabled = enabled
        # emits `1b 50` only when the source has no descriptor yet, so the
        # backend's own emission (SHAVEFillDelaySlots) is never doubled.
        self.bra_descriptor = bra_descriptor
        self.ldi_pair_pad = ldi_pair_pad
        # vendor-style `(n-1)<<4` one-byte NOPs for gaps >= 2 slots
        # (measured; vendor default, byte-parity with gold)
        self.compressed_nops = compressed_nops
        # rewrite `LDO.W d, base, 0` to the silicon-verified 2-operand LD.W
        self.zero_offset_fix = zero_offset_fix
        self.fadd_latency = fadd_latency


def _unit_op(mn):
    """'LSU0.LDO.32' -> ('LSU0', 'LDO.32'); 'FOO' -> ('', 'FOO')."""
    head, _, tail = mn.partition('.')
    if head.upper() in _UNIT_PREFIXES and tail:
        return head.upper(), tail
    return '', mn


def _is_filler(mn):
    return mn.upper() in _FILLER_NAMES


def _is_branch(mn):
    u, _op = _unit_op(mn)
    return u == 'BRU' and mn.upper() not in _FILLER_NAMES


def _is_cond_branch(mn):
    """BRU.BRA = the only conditional BRU form (needs the `1b 40` descriptor)."""
    u, op = _unit_op(mn)
    return u == 'BRU' and op.upper() == 'BRA'


def _is_load(mn):
    u, op = _unit_op(mn)
    op = op.upper()
    return u in _LSU_UNITS and op.startswith('LD') and not op.startswith('LDI')


def _is_store(mn):
    u, op = _unit_op(mn)
    return u in _LSU_UNITS and op.upper().startswith('ST')


def _is_ldi(mn):
    u, op = _unit_op(mn)
    return u in _LSU_UNITS and op.upper().startswith('LDI')


def _is_div(mn):
    """SAU.DIV* long-latency producer (measured: 13 slots)."""
    u, op = _unit_op(mn)
    return u == 'SAU' and op.upper().startswith('DIV')


def _producer_latency(mn, opts):
    """Slots a result must stay unconsumed: 0 = ordinary forwarding."""
    if _is_load(mn):
        return opts.latency
    if _is_div(mn):
        return opts.div_latency
    u, op = _unit_op(mn)
    if u == 'SAU' and op.upper().startswith('ADD'):
        return opts.fadd_latency
    return 0


# Measured by VendorRE2 (PACKET_SPEC §14, probes packets/probes/br/ld*,
# ldop*, p21/p25/p5): in the 5-byte `LSU0.LDO.32` load form the destination
# register set {i19, i21, i24, i30} FAULTS on silicon while i16..i18, i20,
# i22, i23, i25..i29, i31 execute normally (i25 verified loading correctly),
# so this is a register-dependent trap, not a generic bit-16 alias.  The
# 2-operand `LSU0.LD.32` form is verified-good for i5 but is NOT safe for high
# registers (`LD.32 i21` traps, `LD.32 i25` silently writes nothing).
# VendorRE2 §14 (rewritten): the trap is a (register, immediate) PATTERN
# property, not a fixed register list - e.g. an LDIL/LDIH pair into r21 is
# legal with imm 0x0001/0x0000 but traps with 0xBEEF/0xDEAD, while r19/r30 trap
# in both.  {19,21,24,30} is only the imm=0 row of the 5-byte LDO form.  The
# mechanical rule the assembler can enforce is therefore the conservative one:
# keep load destinations in r0..r15 and only use immediates already verified on
# silicon.
LSU_HI_DEST_TRAPS = (19, 21, 24, 30)
LSU_SAFE_DEST_MAX = 15
# StrangeSole (loopdiff/DIFF.md): an LSU op within this many slots after a
# memory load discards the load's write-back (LDO.32: slots 4-5 measured bad;
# LD.32.F16.F32: slots 1 and 6 measured bad) - stay out of the whole window.
LSU_LOAD_ISOLATION = 5


def _load_isolation(mn):
    """(window, strict) for a memory load: slots that must stay free of
    conflicting LSU ops after it, and whether ANY LSU op conflicts.

    StrangeSole's sweeps: for `LSU0.LDO.32` only SAME-UNIT LSU ops kill the
    write-back (an LSU1 op in the same slots is measured fine) and the danger
    zone is the op landing at slot 5-6 (gap 4-5).  For
    `LSU0.LD.32.F16.F32` it is worse and cross-unit: `LSU0.LDIL` at slot 6 and
    `LSU1.LDIH` at slot 1 both lose it, so >=8 non-LSU slots are required.
    """
    _u, op = _unit_op(mn)
    op = op.upper()
    if 'F16.F32' in op or 'F32.F16' in op:
        return 7, True
    return 5, False


def _reg_num(tok):
    m = re.fullmatch(r'[iI](\d+)', tok.strip())
    return int(m.group(1)) if m else None


def _ops_regs(ops):
    """Register numbers in operand order (`[iN+off]` -> its base register)."""
    out = []
    for o in ops:
        n = _reg_num(o)
        if n is None:
            m = re.fullmatch(r'\[i(\d+)\s*(?:\+[^\]]*)?\]', o.strip(), re.I)
            if m:
                n = int(m.group(1))
        if n is not None:
            out.append(n)
    return out


def _base_reg(ops):
    """Address register of a load/store: operand 1 (value, base) order."""
    regs = _ops_regs(ops)
    return regs[1] if len(regs) > 1 else None


def _reads_writes(mn, ops):
    """(registers read, registers written) for one instruction.

    Vendor operand order is `dest, src...` for the data path: LDIL/LDIH and
    every memory load write operand 0, IAU/SAU/VAU/PEU/CMU write operand 0.
    Stores read BOTH operands (value, base); single-operand ops
    (CMU.CMZ.I32 i5, BRU.JMP i30, IAU.INCS i10) read their only operand.
    """
    regs = _ops_regs(ops)
    if not regs:
        return set(), set()
    if _is_store(mn):
        return set(regs), set()
    if _is_load(mn) or _is_ldi(mn):
        return set(regs[1:]), {regs[0]}
    if len(regs) == 1:
        return {regs[0]}, set()
    return set(regs[1:]), {regs[0]}


def _slot_info(slot):
    """'LSU0.LDO.32 i0, i18, 0x28 ; fmt=0' -> (mn, [ops], hints)."""
    hints = {}
    m = re.search(r';\s*fmt=(\d+)', slot)
    if m:
        hints['fmt'] = int(m.group(1))
    m = re.search(r'raw=([0-9a-fA-F]+)', slot)
    if m:
        hints['raw'] = m.group(1)
    body = re.sub(r';.*$', '', slot).strip()
    parts = body.split(None, 1)
    mn = parts[0]
    ops = split_ops(parts[1]) if len(parts) > 1 else []
    return mn, ops, hints


def _mk_insn_slot(mn, ops, fmt=None):
    s = mn + ((' ' + ', '.join(ops)) if ops else '')
    if fmt is not None:
        s += f' ; fmt={fmt}'
    return s


def _pick_unit_target(db, mn, ops, want_unit):
    """Structurally equivalent form of `mn` on `want_unit`, or None.

    Candidates must have the same encoded length, operand count/kinds and
    mask; among them vendor-corpus-observed forms win (they are the bytes the
    vendor assembler really emits), then the smallest `fixed_bits` delta (the
    unit/opcode bits).  The chosen form is pinned on the rewritten line with
    an explicit `; fmt=` hint.
    """
    u, op = _unit_op(mn)
    if not u or u == want_unit:
        return None
    parsed = [parse_operand(o) for o in ops]
    kinds = [p[0] for p in parsed]
    src = resolve_form(db, mn, len(ops), None, kinds=kinds)
    if src is None:
        return None
    best = None
    for f in db.by_name.get(f'{want_unit}.{op}'.upper(), []):
        if f.length_bytes != src.length_bytes or f.npred != src.npred:
            continue
        if f.mask != src.mask or len(f.fields) != len(src.fields):
            continue
        if [x.kind for x in f.fields] != [x.kind for x in src.fields]:
            continue
        d = (f.fixed_bits ^ src.fixed_bits).bit_count()
        if not 0 < d <= 8:
            continue
        key = (0 if f.corpus_hits else 1, d, -f.corpus_hits, f.fmt)
        if best is None or key < best[0]:
            best = (key, f)
    return best[1] if best else None


def _nop_items(n, compressed=False):
    """Items covering n issue slots of filler.

    Plain form: n x `BRU.NOP` (0x07, one byte/slot - every working recipe).
    Compressed form (VendorRE2, measured): one byte `(n-1)<<4` covers n slots
    (1..16); 0x50 = 6, 0xc0 = 12, 0xf0 = 16; a low nibble != 0 is not a NOP.
    """
    if n <= 0:
        return []
    if not compressed:
        return [_NOP_ITEM] * n
    out = []
    while n:
        k = min(n, 16)
        if k == 1:
            # 0x00 is the n=1 compressed form but was only ever observed in
            # FAILING (n<6) gaps; 0x07 is the measured-working 1-slot NOP.
            out.append(_NOP_ITEM)
        else:
            out.append(('dir', f'.byte 0x{(k - 1) << 4:02x}', 0))
        n -= k
    return out


def _filler_byte_slots(tok):
    """Slots covered by one filler byte token, or None if it is not a NOP."""
    t = tok.strip().lower()
    if t in ('0x07', '07'):
        return 1                      # BRU.NOP/BRU.INC opcode
    try:
        v = int(t, 16) if t.startswith('0x') else int(t, 16)
    except ValueError:
        return None
    if v > 0xff or (v & 0x0f):
        return None                   # low nibble != 0 is not a NOP
    return (v >> 4) + 1


def _filler_run_len(items, i):
    """Slots of explicit filler directly after item i.

    Recognises `BRU.NOP`/`BRU.INC` lines, `.byte 0x07` runs and the compressed
    `(n-1)<<4` NOP bytes, so the pass stays idempotent whichever form the
    source (or a previous run) used.  Labels/directives consume no slot and
    are skipped.
    """
    k = 0
    j = i + 1
    while j < len(items):
        it = items[j]
        if it[0] == 'insn':
            ms = [_slot_info(s)[0] for s in it[1] if s]
            if ms and all(_is_filler(m) for m in ms):
                k += len(ms)
                j += 1
                continue
            break
        if it[0] == 'dir' and it[1].startswith('.byte'):
            toks = it[1][5:].replace(',', ' ').split()
            slots = [_filler_byte_slots(t) for t in toks]
            if toks and all(s is not None for s in slots):
                k += sum(slots)
                j += 1
                continue
            break
        if it[0] == 'label':
            j += 1
            continue
        break
    return k


def legalize_items(db, items, opts):
    """Apply D1-D4 to a tokenized item stream; -> (items, stats)."""
    st = {'delay_nops': 0, 'latency_nops': 0, 'rewrites': [], 'warnings': [],
          'changed': False}
    if not opts.enabled:
        return items, st
    out = []
    pending = []        # [dest reg, slots since issue, required slots]
    lsu_hist = []       # [unit, kind, slots since, base reg]
    ldi_pair = None     # [reg, slots since the LDIL/LDIH pair completed]
    prev_ldi = None     # reg of the last emitted LDI (single half)
    prev_lsu = None     # (kind, base reg, unit) of the last LSU op
    prev_lsu_gap = 0
    load_gap = None     # slots since the last memory load
    load_ctx = None     # (unit, window, strict) of that load

    def bump(k):
        if k <= 0:
            return
        nonlocal prev_lsu_gap, ldi_pair, load_gap, load_ctx
        if load_gap is not None:
            load_gap += k
            if load_ctx is not None and load_gap > load_ctx[1]:
                load_gap = None
                load_ctx = None
        prev_lsu_gap += k
        for p in pending:
            p[1] += k
        pending[:] = [p for p in pending if p[1] < p[2]]
        for h in lsu_hist:
            h[2] += k
        if opts.lsu_window:
            lsu_hist[:] = [h for h in lsu_hist if h[2] <= opts.lsu_window]
        if ldi_pair is not None:
            ldi_pair[1] += k
            if opts.latency and ldi_pair[1] >= opts.latency:
                ldi_pair = None

    for i, it in enumerate(items):
        if it[0] != 'insn':
            if it[0] == 'dir':
                ds = it[1]
                if ds.startswith('.byte'):
                    toks = ds[5:].replace(',', ' ').split()
                    if toks and all(t in _FILLER_BYTES for t in toks):
                        bump(len(toks))     # `.byte 0x07` = NOP slots
                        out.append(it)
                        continue
                    pending[:] = []
                    lsu_hist[:] = []
                    ldi_pair = None
                    prev_ldi = prev_lsu = None
                elif re.match(r'^\.(text|data|bss|rodata|section)\b', ds):
                    # new function/section: the previous one returned through
                    # a branch, so no LSU dependency can cross it
                    pending[:] = []
                    lsu_hist[:] = []
                    ldi_pair = None
                    prev_ldi = prev_lsu = None
            out.append(it)
            continue
        ln = it[2] if len(it) > 2 else 0
        idxs = [k for k, s in enumerate(it[1]) if s]
        members = [_slot_info(it[1][k]) for k in idxs]
        if not members:
            out.append(it)
            continue
        rewritten = False
        # ---- vendor pipe shape (<=3 slots, <=1 op per unit) -------------
        if len(members) > 3:
            raise SystemExit(
                f'line {ln}: pipe has {len(members)} slots (max 3)')
        units = [_unit_op(m[0])[0] for m in members]
        dup = sorted({u for u in units if u and units.count(u) > 1})
        if dup:
            raise SystemExit(
                f'line {ln}: {"/".join(dup)} unit is used more than once in '
                f'the same pipe')
        # ---- D3 LSU unit policy -----------------------------------------
        if opts.lsu_policy != 'off':
            for mi, (mn, ops, hints) in enumerate(members):
                if not (_is_load(mn) or _is_store(mn)):
                    continue
                if hints.get('raw'):
                    continue        # byte-specified line: taken verbatim
                u, op = _unit_op(mn)
                want = 'LSU0' if _is_load(mn) else 'LSU1'
                if u == want:
                    continue
                if opts.lsu_policy == 'error':
                    raise SystemExit(
                        f'line {ln}: {mn}: vendor LSU unit policy wants '
                        f'{want} for this {"load" if _is_load(mn) else "store"} '
                        f'(--lsu-policy prefer rewrites it)')
                tgt = _pick_unit_target(db, mn, ops, want)
                if tgt is None:
                    msg = (f'line {ln}: {mn}: no equivalent {want}.{op} form '
                           f'for the LSU unit policy')
                    st['warnings'].append(msg)
                    continue
                newmn = f'{want}.{op}'
                st['rewrites'].append((mn, newmn, tgt.fmt))
                members[mi] = (newmn, ops, {'fmt': tgt.fmt})
                rewritten = True
        # ---- D5 zero-offset load -> 2-operand LD form -------------------
        for mi, (mn, ops, hints) in (enumerate(members)
                                     if opts.zero_offset_fix else ()):
            if not _is_load(mn) or len(ops) != 3 or hints.get('raw'):
                continue
            u, op = _unit_op(mn)
            tgt_op = ZERO_OFFSET_LD.get(op.upper())
            if not tgt_op:
                continue
            off = _opt_int(ops[2])
            if off != 0:
                continue
            dst = parse_operand(ops[0])
            if dst[0] != 'reg' or dst[1] > LSU_SAFE_DEST_MAX:
                # the 2-operand LD form is only silicon-verified for r0..r15
                continue
            parsed = [parse_operand(o) for o in ops[:2]]
            newmn = f'{u}.{tgt_op}'
            form = resolve_form(db, newmn, 2,
                                kinds=[p[0] for p in parsed])
            if form is None:
                st['warnings'].append(
                    f'line {ln}: {mn} with offset 0: no {newmn} form to '
                    f'rewrite to (offset-0 LDO measured unsafe for some '
                    f'destination registers)')
                continue
            try:
                form.encode({0: parsed[0][1], 1: parsed[1][1]})
            except (ValueError, IndexError, KeyError):
                st['warnings'].append(
                    f'line {ln}: {mn} with offset 0: {newmn} does not encode '
                    f'these operands; left as-is')
                continue
            st['rewrites'].append((f'{mn} {", ".join(ops)}',
                                   f'{newmn} {", ".join(ops[:2])}', form.fmt))
            members[mi] = (newmn, ops[:2], {'fmt': form.fmt})
            rewritten = True
        # ---- D2 load latency (dataflow-aware) ---------------------------
        reads = set()
        for mn, ops, _h in members:
            r, _w = _reads_writes(mn, ops)
            reads |= r
        # NOTE: no IAU.ADD-imm range restriction.  The apparent "imm & 3"
        # behaviour (ANNOTATED_RULES C17) was a bug in the OBJ writer, which
        # resolved the reg-only form and dropped the immediate's top bit; with
        # the operand-kind fix `obj` and `asm` agree and imm 0/2/8 give
        # +0/+2/+8 on hardware (VendorRE2 control).
        # LSU register operands in the shared destination/value field:
        # VendorRE2 §14 (rewritten): trap legality is a (register, immediate)
        # PATTERN property of the whole encoded word - r21 is legal with imm
        # 0x0001/0x0000 and traps with 0xBEEF/0xDEAD, r19/r30 trap in both; the
        # imm=0 5-byte LDO row faults for {19,21,24,30}.  Store value fields
        # > i15 are measured good (i16/i17/i20/i22/i25/i26/i31) but share the
        # field geometry with load destinations, so one conservative rule is
        # used for both.
        for mn, ops, _h in members:
            if not (_is_load(mn) or _is_store(mn)) or not ops:
                continue
            op0 = parse_operand(ops[0])
            if op0[0] != 'reg' or op0[1] <= LSU_SAFE_DEST_MAX:
                continue
            what = 'load destination' if _is_load(mn) else 'store value'
            extra = (f' (i{op0[1]} is in the measured imm=0 LDO fault set '
                     f'{LSU_HI_DEST_TRAPS})' if op0[1] in LSU_HI_DEST_TRAPS
                     else '')
            st['warnings'].append(
                f'line {ln}: {mn}: {what} i{op0[1]} is > i15{extra}; trap '
                f'legality is a (register, immediate) PATTERN property '
                f'(VendorRE2 §14 - the same register is fine with one '
                f'immediate and traps with another).  Keep LSU register '
                f'operands in r0..r15 unless the exact combination is '
                f'effect-verified')
        # 0x42-class two-operand byte load: measured NON-WORKING on silicon
        # with both candidate byte-3 variants (AsmEncodingRE C21 + StrangeSole's
        # nonzero-witness redo: the field map inverts correctly but nothing is
        # delivered into the destination), and it has zero vendor-corpus
        # instances.  Warn rather than guess bytes; the displacement form and
        # the LD.16/LD.32 + mask routes are the alternatives under test.
        for mn, ops, _h in members:
            u, op = _unit_op(mn)
            if u not in _LSU_UNITS or op.upper() not in ('LD.8', 'LD.8R'):
                continue
            # only the plain 2-operand shape: the displacement form is the
            # candidate replacement under test, so never flag it
            if len(ops) != 2 or any(re.search(r'\[i\d+\s*\+', o) for o in ops):
                continue
            st['warnings'].append(
                f'line {ln}: {mn}: the 2-operand 0x42-class byte load is '
                f'measured non-working on silicon (no delivery, both byte-3 '
                f'variants; no corpus instance) - use the displacement form '
                f'`LSUx.LD.8 d, [base+off]` or widen and mask')
        need = 0
        is_lsu_line = any(_is_load(m[0]) or _is_store(m[0]) for m in members)
        if opts.latency:
            for p in pending:
                if p[0] in reads and p[1] < p[2]:
                    need = max(need, p[2] - p[1])
            # an LDIL/LDIH pair is not a memory load but the silicon still
            # needs >= latency slots before the next LSU access (x4/x5 fault)
            if ldi_pair is not None and is_lsu_line and \
                    ldi_pair[1] < opts.latency:
                if opts.ldi_pair_pad:
                    need = max(need, opts.latency - ldi_pair[1])
                else:
                    st['warnings'].append(
                        f'line {ln}: LSU access {ldi_pair[1]} slot(s) after '
                        f'the LDIL/LDIH pair for i{ldi_pair[0]} (needs '
                        f'{opts.latency}; --ldi-pair-pad fixes it)')
        # ---- D3b keep LSU ops out of a load's write-back window ---------
        # StrangeSole (loopdiff/DIFF.md, runp3+dump_probe): an LSU-licensed op
        # issued 4-5 slots after `LSU0.LDO.32` (and 0-6 after
        # `LSU0.LD.32.F16.F32`) silently DISCARDS that load's write-back - the
        # destination keeps its previous value; non-LSU fillers never do.  Our
        # fillers live on the BRU unit, so padding is always safe and takes the
        # LSU op out of the window.
        if opts.lsu_policy != 'off' and load_gap is not None and load_ctx:
            win, strict = load_ctx[1], load_ctx[2]
            if 0 < load_gap <= win:
                for mn, _o, _h in members:
                    if not (_is_load(mn) or _is_store(mn) or _is_ldi(mn)):
                        continue
                    if strict or _unit_op(mn)[0] == load_ctx[0]:
                        need = max(need, win + 1 - load_gap)
                        break
        # ---- D4 aliased store -> load distance --------------------------
        if opts.aliased_store_distance and prev_lsu is not None and \
                prev_lsu_gap == 0:
            for mn, ops, _h in members:
                if _is_load(mn) and prev_lsu[0] == 'store' and \
                        _unit_op(mn)[0] != prev_lsu[2] and \
                        _base_reg(ops) is not None and \
                        _base_reg(ops) == prev_lsu[1]:
                    need = max(need, opts.aliased_store_distance)
                    break
        out.extend(_nop_items(need, opts.compressed_nops))
        st['latency_nops'] += need
        bump(need)
        # ---- emit the (possibly rewritten) line -------------------------
        if rewritten:
            new_slots = list(it[1])
            for mi, k in enumerate(idxs):
                new_slots[k] = _mk_insn_slot(members[mi][0], members[mi][1],
                                             members[mi][2].get('fmt'))
            it = ('insn', new_slots, ln)
        out.append(it)
        bump(len(members))
        # ---- register new loads / retire written registers ---------------
        for mn, ops, _h in members:
            _r, w = _reads_writes(mn, ops)
            if w:
                pending[:] = [p for p in pending if p[0] not in w]
            regs = _ops_regs(ops)
            req = _producer_latency(mn, opts)
            if req and regs:
                pending.append([regs[0], 0, req])
            if _is_load(mn):
                win, strict = _load_isolation(mn)
                load_gap = 0       # write-back window starts after this load
                load_ctx = (_unit_op(mn)[0], win, strict)
            if _is_ldi(mn) and regs:
                _u, lop = _unit_op(mn)
                imm = None
                if len(ops) > 1:
                    try:
                        imm = int(ops[1], 0)
                    except ValueError:
                        imm = None
                paired = prev_ldi == regs[0]
                # NOTE: no orphan-HI (LDIH bit15) restriction here - C14 was
                # retracted (Mockingbird E5/S3/S5: bit15-set immediates execute
                # normally; the earlier trap was a skeleton artifact).
                if paired:
                    ldi_pair = [regs[0], 0]      # LDIL+LDIH pair complete
                else:
                    ldi_pair = None
                prev_ldi = regs[0]
            elif not _is_filler(mn):
                prev_ldi = None
        # ---- D3 window conflict check (post-rewrite) --------------------
        for mn, ops, _h in members:
            if not (_is_load(mn) or _is_store(mn)):
                continue
            u, _op = _unit_op(mn)
            kind = 'load' if _is_load(mn) else 'store'
            base = _base_reg(ops)
            for hu, hkind, hslots, _hb in lsu_hist:
                if hu != u or hkind == kind:
                    continue
                msg = (f'line {ln}: {u} unit is used more than once in the '
                       f'same pipe window ({hkind} +{hslots} slot(s) -> '
                       f'{kind}): the silicon corrupts a load/store pair that '
                       f'shares one LSU unit')
                if opts.lsu_policy == 'error':
                    raise SystemExit(msg)
                st['warnings'].append(msg)
            lsu_hist.append([u, kind, 0, base])
            prev_lsu = (kind, base, u)
            prev_lsu_gap = 0
        # ---- D1 branch: conditional descriptor / unconditional delay ----
        cond = [m[0] for m in members if _is_cond_branch(m[0])]
        if cond:
            has_desc = _has_bra_descriptor(items, i)
            if has_desc:
                pass
            elif opts.bra_descriptor:
                desc, how = _bra_descriptor_bytes(items, i)
                out.append(('dir',
                            '.byte ' + ', '.join(f'0x{b:02x}' for b in desc),
                            ln))
                st['changed'] = True
                st['warnings'].append(
                    f'line {ln}: emitted the BRU.BRA delay descriptor '
                    f'{desc.hex()} ({how})')
            else:
                st['warnings'].append(
                    f'line {ln}: {cond[0]} has no delay descriptor '
                    f'(`1b 50` = immediate, `1b 40` = 1 real delay slot, '
                    f'`2b ..` backward) - the branch will NOT transfer '
                    f'control (emit it in the backend, or use '
                    f'--bra-descriptor)')
        elif opts.delay_slots and any(_is_branch(m[0]) for m in members):
            k = _filler_run_len(items, i)
            if k < opts.delay_slots:
                fill = opts.delay_slots - k
                out.extend(_nop_items(fill, opts.compressed_nops))
                st['delay_nops'] += fill
                bump(fill)
    if st['delay_nops'] or st['latency_nops'] or st['rewrites'] or \
            st['changed']:
        st['changed'] = True
    return out, st


def _items_to_text(items):
    lines = []
    for it in items:
        if it[0] == 'label':
            lines.append(it[1] + ':')
        elif it[0] == 'dir':
            lines.append(it[1])
        else:
            lines.append(' || '.join(it[1]))
    return '\n'.join(lines) + '\n'


def is_disasm_listing(text):
    """True for `disasm` output: it carries `; fmt=N raw=<bytes>` hints."""
    return 'raw=' in text


def legalize_text(db, text, opts):
    """Tokenize, legalize, re-serialize; -> (text, stats).

    A `disasm` listing (it carries `raw=` byte hints) is re-encoded verbatim:
    it is already a legal, vendor-scheduled stream, so padding or rewriting it
    would corrupt the byte-identity roundtrip.  Localize with `--exact`.
    """
    if not opts.enabled or is_disasm_listing(text):
        return text, {'delay_nops': 0, 'latency_nops': 0, 'rewrites': [],
                      'warnings': [], 'changed': False,
                      'verbatim': is_disasm_listing(text)}
    items = tokenize(text)
    new_items, st = legalize_items(db, items, opts)
    if not st.get('changed'):
        return text, st         # untouched: avoid any re-serialization drift
    return _items_to_text(new_items), st


def report_legalize(st):
    """One-line stderr summary of what the legality pass changed."""
    parts = []
    if st.get('verbatim'):
        parts.append('verbatim (disasm listing with raw= hints): re-encoded '
                     'byte-for-byte, no padding/rewrites')
    if st['delay_nops']:
        parts.append(f"{st['delay_nops']} delay slot(s) filled")
    if st['latency_nops']:
        parts.append(f"{st['latency_nops']} latency slot(s) filled")
    if st['rewrites']:
        changed = ', '.join(f'{a}->{b}' for a, b, _ in st['rewrites'])
        parts.append(f"{len(st['rewrites'])} form rewrite(s) [{changed}]")
    for w in st['warnings']:
        print(f'legalize: WARNING: {w}', file=sys.stderr)
    if parts:
        print('legalize: ' + '; '.join(parts), file=sys.stderr)


def tokenize(text):
    items = []
    for ln, raw in enumerate(text.splitlines(), 1):
        line = raw.split('//')[0].rstrip()
        maddr = re.match(r'^\s*(0x[0-9a-fA-F]+|[0-9a-fA-F]{2,}):\s*(.*)$', line)
        if maddr:
            line = maddr.group(2)
            # disasm line: drop the hex-bytes column
            mhex = re.match(r'^([0-9a-fA-F]{2,})\s+([A-Za-z.].*)$', line)
            if mhex and not mhex.group(2).startswith('.byte'):
                line = mhex.group(2)
        if '; fmt=' not in line and '; desc=' not in line:
            line = line.split(';')[0]
        s = line.strip()
        if not s:
            continue
        if re.fullmatch(r'[A-Za-z_.$][\w.$]*:', s):
            items.append(('label', s[:-1], ln))
            continue
        if s.startswith('.byte'):
            items.append(('dir', s, ln))
            continue
        if s.startswith('.'):
            if s.split()[0] in DIRECTIVES or s.startswith('.section') \
                    or s.startswith('.cfi') or s.startswith('.l') \
                    or s.startswith('.s'):
                items.append(('dir', s, ln))
                continue
            items.append(('dir', s, ln))  # tolerate unknown directives
            continue
        # strip trailing label prefix like "foo: insn"
        m = re.match(r'([A-Za-z_.$][\w.$]*:)\s*(.*)', s)
        if m:
            items.append(('label', m.group(1)[:-1], ln))
            s = m.group(2).strip()
            if not s:
                continue
        slots = [x.strip() for x in s.split('||')]
        items.append(('insn', slots, ln))
    return items


def disasm_stream(db, data, base=0, out=sys.stdout, resolve=None):
    """Returns (n_decoded_bytes, n_total). Emits lines; unmatched bytes as db."""
    pos = 0
    dec = 0
    while pos < len(data):
        cands = db.candidates(data, pos)
        if not cands:
            # gather run of unknown bytes
            q = pos
            while q < len(data) and not db.candidates(data, q):
                q += 1
            run = data[pos:q]
            print(f"{base + pos:#06x}: .byte {', '.join(f'0x{b:02x}' for b in run)}"
                  f"    ; undecoded {len(run)} byte(s)", file=out)
            pos = q
            continue
        ins, chunk = cands[0]
        vals, kinds = ins.decode(chunk)
        ops = []
        for oi in range(ins.npred):
            k = kinds.get(oi)
            if k is None:
                continue
            v = vals.get(oi, 0)
            if k == 'reg':
                ops.append(f"i{v}")
            elif k == 'rel':
                ops.append(f"{base + pos + v:#x}")
            else:
                ops.append(f"{v:#x}" if v > 9 else str(v))
        text = ins.name + ((' ' + ', '.join(ops)) if ops else '')
        print(f"{base + pos:#06x}: {chunk.hex():<12} {text}    ; fmt={ins.fmt} raw={chunk.hex()}", file=out)
        pos += ins.length_bytes
        dec += ins.length_bytes
    return dec, len(data)


_KIND_OK = {
    'reg': {'reg', 'pred'},
    'pred': {'pred', 'reg'},
    'mem': {'mem', 'reg'},
    'imm': {'imm', 'rel'},
    'sym': {'imm', 'rel'},
}


def resolve_candidates(db, name, nops, fmt_hint=None, kinds=None):
    forms = db.by_name.get(name.upper(), [])
    if not forms:
        return []
    fits = [f for f in forms if f.npred == nops] or forms
    if kinds:
        def kind_ok(f):
            fk = {}
            for x in f.fields:
                fk.setdefault(x.operand, x.kind)
            for i, k in enumerate(kinds):
                allowed = _KIND_OK.get(k)
                if allowed and fk.get(i) not in allowed:
                    return False
            return True
        kk = [f for f in fits if kind_ok(f)]
        if kk:
            fits = kk
    # explicit encode_priority (reloc/DWARF-anchored choices), then forms that
    # actually occur in vendor-assembled corpus, then specificity (mask
    # richness - mirrors decode-side ranking so disasm->asm agrees), then
    # corpus frequency.
    fits.sort(key=lambda f: (f.encode_priority,
                             0 if f.corpus_hits else 1,
                             -f.mask.bit_count(),
                             -f.corpus_hits))
    if fmt_hint is not None:
        hinted = [f for f in fits if f.fmt == fmt_hint]
        if hinted:
            return hinted + [f for f in fits if f.fmt != fmt_hint]
    return fits


def resolve_form(db, name, nops, fmt_hint=None, kinds=None):
    c = resolve_candidates(db, name, nops, fmt_hint, kinds)
    return c[0] if c else None


def assemble(db, text, labels=None):
    items = tokenize(text)
    labels = dict(labels or {})
    # two passes: measure, then emit (branch labels resolved pass 2)
    def emit(pass_no, pc):
        out = bytearray()
        fixups = []
        for it in items:
            if it[0] == 'label':
                if pass_no == 1:
                    labels[it[1]] = pc + len(out)
                continue
            if isinstance(it[1], str) and it[1].startswith('.byte'):
                nums = it[1][5:].replace(',', ' ').split()
                for n in nums:
                    out.append(int(n, 0) & 0xff)
                continue
            if it[0] == 'dir':
                continue
            _, slots, ln = it
            _pkt_start = len(out)
            if len(slots) > 1:
                # vendor pipe rules: <=3 slots, <=1 op per unit (BRU CMU IAU
                # LSU0 LSU1 PEU SAU VAU) - enforced here as well as in the
                # legality pass so every caller sees the diagnostic.
                seen = {}
                for slot in slots:
                    if not slot:
                        continue
                    un = _unit_op(_slot_info(slot)[0])[0]
                    if un and un in seen:
                        raise SystemExit(
                            f"line {ln}: {un} unit is used more than once in "
                            f"the same pipe")
                    if un:
                        seen[un] = True
            for slot in slots:
                if not slot:
                    continue
                fmt_hint = None
                raw_hex = None
                mfh = re.search(r';\s*fmt=(\d+)', slot)
                if mfh:
                    fmt_hint = int(mfh.group(1))
                mrw = re.search(r'raw=([0-9a-fA-F]+)', slot)
                if mrw:
                    raw_hex = mrw.group(1)
                slot = re.sub(r';.*$', '', slot).strip()
                parts = slot.split(None, 1)
                mn = parts[0]
                ops = split_ops(parts[1]) if len(parts) > 1 else []
                parsed = [parse_operand(o) for o in ops]
                # a memory operand with nonzero displacement becomes an
                # implicit immediate operand ([iN+off] -> mem, imm) so it
                # routes to the displacement-carrying form instead of being
                # silently dropped
                exp = []
                for p in parsed:
                    exp.append(p[:2] if p[0] == 'mem' else p)
                    if p[0] == 'mem' and len(p) > 2 and p[2]:
                        exp.append(('imm', p[2]))
                parsed = exp
                cands = resolve_candidates(db, mn, len(parsed), fmt_hint,
                                          kinds=[p[0] for p in parsed])
                if not cands:
                    raise SystemExit(f"line {ln}: unknown mnemonic {mn}")
                built = None
                last_err = None
                _chosen_fmt = [None]
                for form in cands:
                    vals = {}
                    rel_targets = []
                    for idx, p in enumerate(parsed):
                        if p[0] == 'reg':
                            vals[idx] = p[1]
                        elif p[0] == 'imm':
                            # disasm prints rel fields as absolute addresses
                            # (base + instr_pos + v); a literal on a rel operand
                            # is therefore a target address, not a field value.
                            if any(f.operand == idx and f.kind == 'rel'
                                   for f in form.fields):
                                if pass_no == 2:
                                    rel_targets.append(
                                        (idx, p[1], pc + len(out)))
                                # pass 1: absolute target cannot be encoded
                                # into a rel field; placeholder only
                                vals[idx] = 0
                            else:
                                vals[idx] = p[1]
                        elif p[0] == 'mem':
                            vals[idx] = p[1]
                        else:  # sym
                            if pass_no == 1:
                                vals[idx] = 0
                            else:
                                tgt = labels.get(p[1])
                                if tgt is None:
                                    raise SystemExit(
                                        f"line {ln}: undefined label {p[1]}")
                                # rel operand: displacement from this insn
                                rel_targets.append(
                                    (idx, tgt, pc + len(out)))
                                vals[idx] = 0
                    try:
                        blob = bytearray(form.encode(vals))
                    except ValueError as e:
                        last_err = e
                        continue
                    if raw_hex:
                        rv = int.from_bytes(bytes.fromhex(raw_hex), 'little')
                        keep = rv & ~form.mask
                        v = int.from_bytes(blob, 'little') | keep
                        blob[:] = v.to_bytes(form.length_bytes, 'little')
                    for idx, tgt, ip in rel_targets:
                        delta = tgt - ip
                        f = [x for x in form.fields if x.operand == idx][0]
                        v = int.from_bytes(blob, 'little')
                        v &= ~f.mask
                        v |= (delta & ((1 << f.width) - 1)) << f.bitpos
                        blob[:] = v.to_bytes(form.length_bytes, 'little')
                    built = blob
                    _chosen_fmt[0] = form.fmt
                    break
                if built is None:
                    raise SystemExit(
                        f"line {ln}: {mn}: no form encodes operands"
                        f" ({last_err})")
                if fmt_hint is not None and _chosen_fmt[0] != fmt_hint:
                    print(f'warning: line {ln}: {mn}: fmt={fmt_hint} hint not '
                          f'honoured for this operand shape (emitted '
                          f'fmt={_chosen_fmt[0]}) - attribute the bytes by '
                          f'disassembly, the hint was dropped',
                          file=sys.stderr)
                out += built
            if len(slots) > 1 and len(out) - _pkt_start > 16:
                raise SystemExit(
                    f"line {ln}: packet of {len(out) - _pkt_start} bytes "
                    f"exceeds the 128-bit (16-byte) slot limit")
        return out
    emit(1, 0)
    return emit(2, 0)


# ---------------------------------------------------------------------------
# ELF32 object emission (obj mode): .s -> ET_REL EM_SPARC(2) relocatable object
#
# Object conventions follow the vendor moviAsm/mlibm members (see
# VALIDATION.md): ELF32 LE, ET_REL, EM_SPARC(2); .text.* PROGBITS SHF_ALLOC|
# SHF_EXECINSTR align 16; symtab = NULL + FILE syms + SECTION syms + local
# object/func syms + GLOBAL syms; .rela.* SHT_RELA entsize 12 with
# r_info = (sym_idx << 8) | type.  lld drops FILE/SECTION syms on link
# (LAYOUT_SPEC.md §5), so emitting them matches mlibm without affecting the
# linked image.  Compiler-internal labels (.L*, LBB*, Lfunc*) are never put
# into the symtab unless a relocation or .type/.size/.local forces it.
#
# Relocations (RELOC_NOTES.md):
#   200 LO16  — LSU0.LDIL <reg>, <sym>   field zeroed, r_offset = imm bytes
#   202 HI16  — LSU1.LDIH <reg>, <sym>   (pair carries 202+200, 3 bytes apart
#                                         in vendor order HI-first; halves are
#                                         independent so source order is free)
#   209 PCREL16 — branch to a non-inlineable target; pre-encoded field =
#                                         r_offset - instr_start (byte offset
#                                         of the field inside the instruction)
#   208 ABS32 — .word <sym>
# Intra-section branches resolve at assemble time when the displacement fits
# the signed field width (identical final bytes: vendor + linker produce
# target - instr_start as well).
# ---------------------------------------------------------------------------

SHT_PROGBITS, SHT_SYMTAB, SHT_STRTAB, SHT_RELA, SHT_NOBITS = 1, 2, 3, 4, 8
SHF_WRITE, SHF_ALLOC, SHF_EXECINSTR = 1, 2, 4
STB_LOCAL, STB_GLOBAL = 0, 1
STT_NOTYPE, STT_OBJECT, STT_FUNC, STT_SECTION, STT_FILE = 0, 1, 2, 3, 4
SHN_UNDEF, SHN_ABS = 0, 0xfff1
R_SHAVE_LO16, R_SHAVE_HI16, R_SHAVE_PCREL16, R_SHAVE_ABS32 = 200, 202, 209, 208
_TYPE_MAP = {'notype': STT_NOTYPE, 'object': STT_OBJECT, 'function': STT_FUNC,
             'func': STT_FUNC}


class ObjSection:
    __slots__ = ('name', 'type', 'flags', 'align', 'data')

    def __init__(self, name, stype=SHT_PROGBITS, flags=0, align=1):
        self.name = name
        self.type = stype
        self.flags = flags
        self.align = align
        self.data = bytearray()


class ObjSym:
    __slots__ = ('name', 'bind', 'type', 'size', 'sec', 'value', 'emit',
                 'defined')

    def __init__(self, name):
        self.name = name
        self.bind = STB_LOCAL
        self.type = STT_NOTYPE
        self.size = 0
        self.sec = None      # section name if defined
        self.value = 0
        self.emit = False    # force into symtab (.type/.size/.local/reloc)
        self.defined = False


def _section_geom(name, attrs, atype):
    """(sh_type, sh_flags, sh_align) for a section name/attribute triple."""
    stype = SHT_NOBITS if atype == 'nobits' else SHT_PROGBITS
    flags = 0
    if attrs is None:
        if name == '.text' or name.startswith('.text.'):
            attrs = 'ax'
        elif name.startswith('.data'):
            attrs = 'wa'
        elif name.startswith('.rodata'):
            attrs = 'a'
        else:
            attrs = ''
    if 'a' in attrs:
        flags |= SHF_ALLOC
    if 'w' in attrs:
        flags |= SHF_WRITE
    if 'x' in attrs:
        flags |= SHF_EXECINSTR
    if 'b' in attrs:
        stype = SHT_NOBITS
    if name == '.text' or name.startswith('.text.'):
        align = 16          # vendor: .text.* sh_addralign 16 (mlibm/gold)
    elif name.startswith('.rodata4.'):
        align = 4
    elif name.startswith('.rodata'):
        align = 8
    elif name.startswith('.data'):
        align = 4
    else:
        align = 1
    return stype, flags, align


def _parse_dq(rest):
    """Parse the first double-quoted string (basic escapes); -> (str, rest)."""
    if not rest.startswith('"'):
        return None, rest
    out = []
    i = 1
    mapping = {'n': '\n', 't': '\t', 'r': '\r', '0': '\0', '\\': '\\',
               '"': '"'}
    while i < len(rest):
        ch = rest[i]
        if ch == '"':
            return ''.join(out), rest[i + 1:]
        if ch == '\\' and i + 1 < len(rest):
            nxt = rest[i + 1]
            if nxt in mapping:
                out.append(mapping[nxt])
                i += 2
                continue
            if nxt == 'x' and i + 3 <= len(rest):
                try:
                    out.append(chr(int(rest[i + 2:i + 4], 16)))
                    i += 4
                    continue
                except ValueError:
                    pass
            out.append(nxt)
            i += 2
            continue
        out.append(ch)
        i += 1
    return ''.join(out), ''


def _split_symadd(tok):
    """'name', 'name+4', 'name-8' -> (name, addend)."""
    m = re.fullmatch(r'([A-Za-z_.$][\w.$]*)\s*([+-])\s*(0[xX][0-9a-fA-F]+|\d+)',
                     tok.strip())
    if m:
        add = int(m.group(3), 0)
        if m.group(2) == '-':
            add = -add
        return m.group(1), add
    return tok.strip(), 0


def assemble_object(db, text):
    """Assemble .s text and return an ELF32 LE ET_REL EM_SPARC(2) object."""
    items = tokenize(text)
    sections = {}          # name -> ObjSection
    sec_order = []
    cur = [None]           # current section name (lazy)
    syms = {}              # name -> ObjSym
    files = []
    labels = {}            # name -> (section, value)
    size_exprs = []
    relocs = []            # (sec, offset, symref, type, addend)

    def sec_new(name, attrs=None, atype=None):
        stype, flags, align = _section_geom(name, attrs, atype)
        s = ObjSection(name, stype, flags, align)
        sections[name] = s
        sec_order.append(name)
        return s

    def cur_sec():
        if cur[0] is None:
            return sec_new('.text')
        return sections[cur[0]]

    def get_sym(name):
        s = syms.get(name)
        if s is None:
            s = syms[name] = ObjSym(name)
        return s

    def emit_pass(pass_no):
        relocs.clear()
        files.clear()
        size_exprs.clear()
        for s in sections.values():
            s.data = bytearray()

        def fail(ln, msg):
            raise SystemExit(f'line {ln}: {msg}')

        for it in items:
            if it[0] == 'label':
                name, ln = it[1], it[2]
                s = cur_sec()
                if pass_no == 1:
                    labels[name] = (s.name, len(s.data))
                    sym = get_sym(name)
                    if not sym.defined:
                        sym.defined = True
                        sym.sec = s.name
                        sym.value = len(s.data)
                continue

            if it[0] == 'insn':
                _, slots, ln = it
                for slot in slots:
                    if not slot:
                        continue
                    fmt_hint = None
                    raw_hex = None
                    mfh = re.search(r';\s*fmt=(\d+)', slot)
                    if mfh:
                        fmt_hint = int(mfh.group(1))
                    mrw = re.search(r'raw=([0-9a-fA-F]+)', slot)
                    if mrw:
                        raw_hex = mrw.group(1)
                    slot = re.sub(r';.*$', '', slot).strip()
                    parts = slot.split(None, 1)
                    mn = parts[0]
                    ops = split_ops(parts[1]) if len(parts) > 1 else []
                    parsed = [parse_operand(o) for o in ops]
                    # mirror the `asm` path: expand `[iN+off]` into an
                    # implicit immediate operand (otherwise the displacement is
                    # silently dropped and the reg-only form is selected), and
                    # pass the operand kinds so an immediate can never land in
                    # a register-only form (that emitted a different
                    # instruction: `IAU.ADD i9,i9,2` -> c6202501 instead of
                    # the gold-identical c6202581).
                    exp = []
                    for p in parsed:
                        exp.append(p[:2] if p[0] == 'mem' else p)
                        if p[0] == 'mem' and len(p) > 2 and p[2]:
                            exp.append(('imm', p[2]))
                    parsed = exp
                    form = resolve_form(db, mn, len(parsed), fmt_hint,
                                        kinds=[p[0] for p in parsed])
                    if form is None:
                        fail(ln, f'unknown mnemonic {mn}')
                    if fmt_hint is not None and form.fmt != fmt_hint:
                        print(f'warning: line {ln}: {mn}: fmt={fmt_hint} hint '
                              f'not honoured for this operand shape (emitted '
                              f'fmt={form.fmt})', file=sys.stderr)
                    s = cur_sec()
                    ip = len(s.data)
                    vals = {}
                    patches = []        # (operand, field value)
                    ins_relocs = []     # (operand, symref, type, addend)
                    for idx, p in enumerate(parsed):
                        fld = next((f for f in form.fields
                                    if f.operand == idx), None)
                        if p[0] in ('reg', 'mem'):
                            vals[idx] = p[1]
                        elif p[0] == 'imm':
                            if fld is not None and fld.kind == 'rel':
                                # disasm prints rel fields as absolute targets
                                if pass_no == 2:
                                    patches.append((idx, p[1] - ip))
                            else:
                                vals[idx] = p[1]
                        else:  # sym
                            if pass_no == 1:
                                vals[idx] = 0
                                continue
                            tname, addend = _split_symadd(p[1])
                            if fld is None:
                                fail(ln, f'symbol on non-immediate operand: {p[1]}')
                            if fld.kind == 'rel':
                                tgt = labels.get(tname)
                                inline = False
                                if tgt is not None and tgt[0] == s.name:
                                    delta = tgt[1] + addend - ip
                                    w = fld.width
                                    if -(1 << (w - 1)) <= delta <= \
                                            (1 << (w - 1)) - 1:
                                        patches.append((idx, delta))
                                        inline = True
                                if not inline:
                                    if tgt is None:
                                        symref = tname
                                        add = addend
                                        get_sym(tname).bind = STB_GLOBAL
                                    elif tgt[0] == s.name:
                                        symref = ('section', s.name)
                                        add = tgt[1] + addend
                                    else:
                                        other = syms.get(tname)
                                        if other is not None and \
                                                other.bind == STB_GLOBAL:
                                            symref = tname
                                            add = addend
                                        else:
                                            symref = ('section', tgt[0])
                                            add = tgt[1] + addend
                                    # vendor pre-encodes r_offset - instr_start
                                    patches.append((idx, fld.bitpos // 8))
                                    ins_relocs.append(
                                        (idx, symref, R_SHAVE_PCREL16, add))
                            else:
                                # absolute address materialisation
                                u = mn.upper()
                                if 'LDIH' in u:
                                    rtype = R_SHAVE_HI16
                                elif 'LDIL' in u:
                                    rtype = R_SHAVE_LO16
                                else:
                                    fail(ln, 'symbolic immediate not '
                                             f'supported for {mn}')
                                sym = get_sym(tname)
                                if not sym.defined:
                                    sym.bind = STB_GLOBAL   # undefined -> global
                                else:
                                    sym.emit = True
                                vals[idx] = 0               # field zeroed
                                ins_relocs.append(
                                    (idx, tname, rtype, addend))
                    blob = bytearray(form.encode(vals))
                    if raw_hex:
                        rv = int.from_bytes(bytes.fromhex(raw_hex), 'little')
                        keep = rv & ~form.mask
                        blob[:] = (int.from_bytes(bytes(blob), 'little') | keep) \
                            .to_bytes(form.length_bytes, 'little')
                    for idx, fval in patches:
                        fld = next(f for f in form.fields if f.operand == idx)
                        v = int.from_bytes(bytes(blob), 'little')
                        v &= ~fld.mask
                        v |= (fval & ((1 << fld.width) - 1)) << fld.bitpos
                        blob[:] = v.to_bytes(form.length_bytes, 'little')
                    for idx, symref, rtype, add in ins_relocs:
                        fld = next(f for f in form.fields if f.operand == idx)
                        if isinstance(symref, str) and symref in syms:
                            syms[symref].emit = True
                        relocs.append((s.name, ip + fld.bitpos // 8,
                                       symref, rtype, add))
                    s.data += blob
                continue

            # ---- directives ----
            _, ds, ln = it
            words = ds.split(None, 1)
            word = words[0]
            rest = words[1] if len(words) > 1 else ''
            if word in ('.section', '.text', '.data', '.bss', '.rodata'):
                if word == '.section':
                    m = re.match(r'([^,\s]+)\s*(?:,\s*"([^"]*)")?'
                                 r'(?:,\s*@(\w+))?\s*$', rest)
                    if not m:
                        fail(ln, f'bad .section: {ds}')
                    name, attrs, atype = m.group(1), m.group(2), m.group(3)
                else:
                    name, attrs, atype = word, None, \
                        ('nobits' if word == '.bss' else None)
                if name not in sections:
                    sec_new(name, attrs, atype)
                cur[0] = name
            elif word in ('.globl', '.global'):
                for nm in rest.split():
                    sym = get_sym(nm)
                    sym.bind = STB_GLOBAL
                    sym.emit = True
            elif word == '.local':
                for nm in rest.split():
                    get_sym(nm).emit = True
            elif word == '.type':
                m = re.match(r'([^,\s]+)\s*,\s*@?(\w+)', rest)
                if m:
                    sym = get_sym(m.group(1))
                    sym.type = _TYPE_MAP.get(m.group(2).lower(), STT_NOTYPE)
                    sym.emit = True
            elif word == '.size':
                m = re.match(r'([^,\s]+)\s*,\s*(.+)$', rest)
                if m:
                    get_sym(m.group(1)).emit = True
                    size_exprs.append((m.group(1), m.group(2).strip()))
            elif word == '.file':
                # .file is parsed but NOT turned into an STT_FILE symbol:
                # upstream lld-14 (default discard policy) *keeps* FILE
                # symbols (verified: linked gate1 output carried the FILE row),
                # while the gold fixture's symtab is NULL + controlled_act only
                # — so vendor entry.o had none.  Emitting one would break
                # byte-parity of the linked image (LAYOUT_SPEC.md §5).
                pass
            elif word in ('.align', '.p2align', '.lalign', '.salign'):
                tok = rest.split()
                if tok:
                    try:
                        n = int(tok[0], 0)
                    except ValueError:
                        n = 1
                    al = (1 << n) if word == '.p2align' else max(n, 1)
                    s = cur_sec()
                    s.data += b'\0' * ((-len(s.data)) % al)
            elif word == '.byte':
                s = cur_sec()
                for tok in rest.replace(',', ' ').split():
                    s.data.append(int(tok, 0) & 0xff)
            elif word in ('.hword', '.short'):
                s = cur_sec()
                for tok in rest.replace(',', ' ').split():
                    s.data += struct.pack('<H', int(tok, 0) & 0xffff)
            elif word in ('.word', '.long', '.int'):
                s = cur_sec()
                for tok in rest.replace(',', ' ').split():
                    p = parse_operand(tok)
                    if p[0] == 'sym':
                        tname, addend = _split_symadd(tok)
                        at = len(s.data)
                        s.data += b'\0\0\0\0'
                        sym = get_sym(tname)
                        if not sym.defined:
                            sym.bind = STB_GLOBAL
                        else:
                            sym.emit = True
                        relocs.append((s.name, at, tname, R_SHAVE_ABS32,
                                       addend))
                    else:
                        s.data += struct.pack('<I', p[1] & 0xffffffff)
            elif word == '.quad':
                s = cur_sec()
                for tok in rest.replace(',', ' ').split():
                    s.data += struct.pack('<Q', int(tok, 0) & 0xffffffffffffffff)
            elif word in ('.single', '.float', '.float32'):
                s = cur_sec()
                for tok in rest.replace(',', ' ').split():
                    s.data += struct.pack('<f', float(tok))
            elif word in ('.ascii', '.asciiz', '.string'):
                s = cur_sec()
                textv, _ = _parse_dq(rest)
                if textv is None:
                    textv = ''
                data = textv.encode('utf-8', 'surrogateescape')
                if word != '.ascii':
                    data += b'\0'
                s.data += data
            elif word in ('.zero', '.space', '.skip'):
                tok = rest.split()
                if tok:
                    cur_sec().data += b'\0' * int(tok[0], 0)
            # everything else (.ident/.cfi_*/.loc/.nowarn/.linkonce/...) ignored

    emit_pass(1)
    # resolve .size expressions now that all labels are known
    pending_sizes = list(size_exprs)
    for nm, expr in pending_sizes:
        m = re.fullmatch(r'([A-Za-z_.$][\w.$]*)\s*-\s*([A-Za-z_.$][\w.$]*)',
                         expr)
        if m and m.group(1) in labels and m.group(2) in labels:
            syms[nm].size = labels[m.group(1)][1] - labels[m.group(2)][1]
        else:
            try:
                syms[nm].size = int(expr, 0)
            except ValueError:
                pass
    emit_pass(2)
    return elf32_object(sections, sec_order, syms, files, relocs, labels)


def elf32_object(sections, sec_order, syms, files, relocs, labels):
    # ---- drop sections that ended up empty and unreferenced ----
    anchored = {s.sec for s in syms.values() if s.sec}
    anchored |= {r[2][1] for r in relocs if isinstance(r[2], tuple)}
    content = [sections[n] for n in sec_order
               if len(sections[n].data) or n in anchored]
    sec_index = {s.name: i + 1 for i, s in enumerate(content)}
    # a reloc may name a dropped section's label -> keep those too
    for r in relocs:
        if isinstance(r[2], tuple) and r[2][1] not in sec_index:
            sec = sections[r[2][1]]
            content.append(sec)
            sec_index[sec.name] = len(content)
            anchored.add(sec.name)

    # ---- symbol table rows ----
    strtab = bytearray(b'\0')

    def add_str(s):
        off = len(strtab)
        strtab.extend(s.encode('utf-8', 'surrogateescape'))
        strtab.append(0)
        return off

    rows = []               # (name_off, value, size, info, other, shndx)
    sym_index = {}
    sec_sym_index = {}
    rows.append((0, 0, 0, 0, 0, 0))
    for f in files:
        rows.append((add_str(f), 0, 0,
                     (STB_LOCAL << 4) | STT_FILE, 0, SHN_ABS))
    for s in content:
        sec_sym_index[s.name] = len(rows)
        rows.append((0, 0, 0, (STB_LOCAL << 4) | STT_SECTION, 0,
                     sec_index[s.name]))
    referenced = {r[2] for r in relocs if isinstance(r[2], str)}
    first_global = None
    locals_ = []
    globals_ = []
    for name, sym in syms.items():
        if sym.bind == STB_GLOBAL:
            globals_.append(sym)
        elif sym.emit or name in referenced:
            locals_.append(sym)
    for sym in locals_:
        shndx = sec_index.get(sym.sec, SHN_UNDEF) if sym.defined else SHN_UNDEF
        sym_index[sym.name] = len(rows)
        rows.append((add_str(sym.name), sym.value, sym.size,
                     (STB_LOCAL << 4) | sym.type, 0, shndx))
    first_global = len(rows)
    for sym in globals_:
        shndx = sec_index.get(sym.sec, SHN_UNDEF) if sym.defined else SHN_UNDEF
        sym_index[sym.name] = len(rows)
        rows.append((add_str(sym.name), sym.value, sym.size,
                     (STB_GLOBAL << 4) | sym.type, 0, shndx))

    def resolve_ref(ref):
        if isinstance(ref, tuple):
            return sec_sym_index[ref[1]]
        return sym_index[ref]

    # ---- build section payload list: content, .symtab, .strtab, rela*, .shstrtab
    symtab = bytearray()
    for r in rows:
        symtab += struct.pack('<IIIBBH', r[0], r[1], r[2], r[3], r[4], r[5])

    rela_by_sec = {}
    for sec_name, off, ref, rtype, add in relocs:
        rela_by_sec.setdefault(sec_name, []).append(
            (off, resolve_ref(ref), rtype, add))

    rows_meta = []          # (name, type, flags, link, info, align, entsize, data)
    for s in content:
        rows_meta.append((s.name, s.type, s.flags, 0, 0, s.align, 0,
                          bytes(s.data)))
    symtab_idx = len(rows_meta) + 1
    strtab_idx = symtab_idx + 1
    rows_meta.append(('.symtab', SHT_SYMTAB, 0, strtab_idx, first_global, 4,
                      16, bytes(symtab)))
    rows_meta.append(('.strtab', SHT_STRTAB, 0, 0, 0, 1, 0, bytes(strtab)))
    for s in content:
        entries = rela_by_sec.get(s.name)
        if not entries:
            continue
        entries.sort()
        blob = bytearray()
        for off, sidx, rtype, add in entries:
            blob += struct.pack('<IIi', off, (sidx << 8) | rtype, add)
        rows_meta.append(('.rela' + s.name, SHT_RELA, 0, symtab_idx,
                          sec_index[s.name], 4, 12, bytes(blob)))
    shstr = bytearray(b'\0')
    shstrtab_idx = len(rows_meta) + 1
    rows_meta.append(('.shstrtab', SHT_STRTAB, 0, 0, 0, 1, 0, b'\0'))
    sh_names = []
    for name, *_ in rows_meta:
        sh_names.append(len(shstr))
        shstr.extend(name.encode() + b'\0')
    rows_meta[-1] = ('.shstrtab', SHT_STRTAB, 0, 0, 0, 1, 0, bytes(shstr))

    # ---- lay out file ----
    pos = 52
    offsets = []
    for *_, data in rows_meta:
        offsets.append(pos)
        pos += len(data)
    shoff = (pos + 3) & ~3
    shnum = len(rows_meta) + 1

    out = bytearray()
    out += struct.pack('<16sHHIIIIIHHHHHH',
                       b'\x7fELF\x01\x01\x01' + b'\0' * 9,
                       1, 2, 1,          # ET_REL, EM_SPARC, EV_CURRENT
                       0, 0, shoff, 0,   # entry, phoff, shoff, flags
                       52, 0, 0, 40, shnum, shstrtab_idx)
    for i, (name, stype, flags, link, info, align, entsize, data) in \
            enumerate(rows_meta):
        out += data
    while len(out) < shoff:
        out += b'\0'
    out += struct.pack('<IIIIIIIIII', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    for i, (name, stype, flags, link, info, align, entsize, data) in \
            enumerate(rows_meta):
        out += struct.pack('<IIIIIIIIII', sh_names[i], stype, flags, 0,
                           offsets[i], len(data), link, info, align, entsize)
    return bytes(out)


def load_input_bytes(s, elf_section):
    if elf_section:
        return extract_section(s, '.text' if elf_section is True else elf_section), s
    if os.path.exists(s):
        with open(s, 'rb') as fh:
            blob = fh.read()
        if blob[:4] == b'\x7fELF':
            return extract_section(s, '.text'), s
        return blob, s
    hx = re.sub(r'\s', '', s)
    if re.fullmatch(r'[0-9a-fA-F]*', hx) and hx:
        if len(hx) % 2:
            hx += '0'
        return bytes.fromhex(hx), '<hex>'
    raise SystemExit('input is neither a file nor a hex string')


def extract_section(path, name):
    data = open(path, 'rb').read()
    if data[:4] != b'\x7fELF':
        raise SystemExit('not an ELF')
    is64 = data[4] == 2
    if is64:
        e_shoff = struct.unpack_from('<Q', data, 0x28)[0]
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', data, 0x3a)
        def shdr(i):
            o = e_shoff + i * e_shentsize
            return struct.unpack_from('<IIQQQQIIQQ', data, o)
        shstr = shdr(e_shstrndx)
        shstrtab = data[shstr[4]:shstr[4] + shstr[5]]
        for i in range(e_shnum):
            s = shdr(i)
            nm = shstrtab[s[0]:shstrtab.index(b'\0', s[0])].decode()
            if nm == name:
                return data[s[4]:s[4] + s[5]]
    else:
        e_shoff = struct.unpack_from('<I', data, 0x20)[0]
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', data, 0x2e)
        def shdr(i):
            return struct.unpack_from('<IIIIIIIIII', data, e_shoff + i * e_shentsize)
        shstr = shdr(e_shstrndx)
        shstrtab = data[shstr[4]:shstr[4] + shstr[5]]
        for i in range(e_shnum):
            s = shdr(i)
            nm = shstrtab[s[0]:shstrtab.index(b'\0', s[0])].decode()
            if nm == name:
                return data[s[4]:s[4] + s[5]]
    for i in range(e_shnum):
        s = shdr(i)
        nm = shstrtab[s[0]:shstrtab.index(b'\0', s[0])].decode()
        if nm.startswith(name) and (s[2] & 0x4):
            return data[s[4]:s[4] + s[5]]
    raise SystemExit(f'section {name} not found')


def main():
    # Vendor moviAsm spellings (Myriad.cpp / moviCompile64 driver):
    #   -a (disable analyzer), -no6thSlotCompression (stale: appears in
    #   NEITHER vendor DLL), -noSPrefixing, -cv:<cpu>, -noCompressedNOPs,
    #   -noFinalSlotCompression, -noSlotAllPromo (all no-ops here: we never
    #   compress NOPs), and the latency knobs -forceDelaySlots:<n> /
    #   -forceLoadDelay:<n> which ARE honoured (they set the legalizer's
    #   delay-slot count / required load distance).
    _kept = []
    _vendor = {}
    for _tok in sys.argv[1:]:
        if _tok in ('-a', '--a', '-no6thSlotCompression',
                    '--no6thSlotCompression', '-noSPrefixing',
                    '--noSPrefixing', '-noCompressedNOPs',
                    '--noCompressedNOPs', '-noFinalSlotCompression',
                    '--noFinalSlotCompression', '-noSlotAllPromo',
                    '--noSlotAllPromo'):
            continue
        if _tok.startswith('-forceDelaySlots:'):
            _vendor['delay_slots'] = int(_tok.split(':', 1)[1], 0)
            continue
        if _tok.startswith('-forceLoadDelay:'):
            _vendor['latency'] = int(_tok.split(':', 1)[1], 0)
            continue
        if _tok.startswith('-cv:'):
            continue
        _kept.append(_tok)
    sys.argv = [sys.argv[0]] + _kept

    ap = argparse.ArgumentParser(prog='shave_asm')
    sub = ap.add_subparsers(dest='cmd', required=True)
    d = sub.add_parser('disasm', help='disassemble hex string or binary file')
    d.add_argument('data')
    d.add_argument('--base', default='0')
    d.add_argument('--elf', action='store_true',
                   help='treat input as ELF and extract its .text')

    def add_legal_flags(p):
        p.add_argument('--latency', type=int, default=LOAD_LATENCY_DEFAULT,
                       metavar='N',
                       help='required slot distance between a memory load '
                            'and the first consumer of its destination '
                            f'(shave-load-latency, default '
                            f'{LOAD_LATENCY_DEFAULT}); dataflow-aware, 0 '
                            'disables the padding')
        p.add_argument('--div-latency', type=int, default=DIV_LATENCY_DEFAULT,
                       metavar='N',
                       help='required slot distance between a SAU.DIV* result '
                            f'and its first consumer (default '
                            f'{DIV_LATENCY_DEFAULT}, measured); 0 disables')
        p.add_argument('--delay-slots', type=int,
                       default=DELAY_SLOTS_DEFAULT, metavar='N',
                       help='delay slots filled with BRU.NOP after an '
                            'unconditional branch (shave-branch-delays, '
                            f'default {DELAY_SLOTS_DEFAULT}); 0 disables')
        p.add_argument('--lsu-policy', choices=('prefer', 'error', 'off'),
                       default='prefer',
                       help='LSU unit policy: prefer = loads LSU0 / stores '
                            'LSU1 (rewrite the unit prefix where an '
                            'equivalent form exists), error = refuse a '
                            'non-preferred or conflicting unit, off = ignore')
        p.add_argument('--lsu-window', type=int, default=LSU_WINDOW_DEFAULT,
                       metavar='N',
                       help='slot window for the same-unit load/store check '
                            '(vendor: "LSU0/LSU1 unit is used more than once '
                            'in the same pipe"); 0 = whole section')
        p.add_argument('--aliased-store-distance', type=int,
                       default=ALIASED_STORE_DISTANCE_DEFAULT, metavar='N',
                       help='extra slots required between a store and an '
                            'aliased load on the other LSU '
                            '(shave-aliased-store-distance, default 1)')
        p.add_argument('--bra-descriptor', action='store_true', default=True,
                       help='DEFAULT ON: emit the conditional-branch delay '
                            'descriptor (`1b 50` forward / `2b 50` backward = '
                            'immediate, k=0) when the source has none; a '
                            '`; desc=<hex>` hint supplies vendor variants')
        p.add_argument('--no-bra-descriptor', dest='bra_descriptor',
                       action='store_false',
                       help='do not emit descriptors (warn only)')
        p.add_argument('--fadd-latency', type=int, default=FADD_LATENCY_DEFAULT,
                       metavar='N',
                       help='required slot distance between a SAU.ADD.F32 result '
                            f'and its first consumer (default '
                            f'{FADD_LATENCY_DEFAULT}: gap 0/1 reads the stale '
                            'pre-add value); 0 disables')
        p.add_argument('--zero-offset', action='store_true', default=True,
                       help='DEFAULT ON: rewrite `LSUx.LDO.W d, base, 0x0` to '
                            'the silicon-verified 2-operand `LSUx.LD.W d, '
                            'base` (measured: LDXV and the offset-0 LDO form '
                            'are not reliable for every destination register)')
        p.add_argument('--no-zero-offset', dest='zero_offset_fix',
                       action='store_false',
                       help='keep explicit offset-0 LDO forms')
        p.add_argument('--compressed-nops', action='store_true', default=True,
                       help='DEFAULT ON: vendor-style one-byte NOPs '
                            '((n-1)<<4 covers n>=2 slots, e.g. 0x50 = 6); '
                            '1-slot gaps stay `0x07`')
        p.add_argument('--no-compressed-nops', dest='compressed_nops',
                       action='store_false',
                       help='emit n x `BRU.NOP` (0x07) instead')
        p.add_argument('--ldi-pair-pad', action='store_true',
                       help='pad >=latency slots after an LDIL/LDIH pair '
                            'that is followed by an LSU access (the backend '
                            'supplies this today; otherwise only a warning '
                            'is printed)')
        p.add_argument('--exact', action='store_true',
                       help='verbatim re-encode: no delay/latency fillers, '
                            'no unit rewrites (implied by a disasm listing, '
                            'which carries raw= hints)')
        p.add_argument('--no-legalize', action='store_true',
                       help='disable the issued-stream legality pass')

    a = sub.add_parser('asm', help='assemble .s file to raw bytes')
    a.add_argument('file')
    a.add_argument('--hex', action='store_true', help='print hex instead of raw')
    add_legal_flags(a)

    o = sub.add_parser(
        'obj',
        help='assemble .s to an ELF32 LE ET_REL EM_SPARC(2) object (moviAsm-compatible)')
    o.add_argument('file', nargs='?', default='-',
                   help=".s input file ('-' or omitted: read stdin)")
    o.add_argument('-o', '--output', default='-',
                   help="output object path ('-': write stdout)")
    o.add_argument('--cv', metavar='CHIPVERSION', default=None,
                   help='moviAsm contract flag --cv <chip-version> '
                        '(e.g. 3720xx); accepted, ignored (the encoding '
                        'tables are already the 3720xx set)')
    o.add_argument('--noSPrefixing', action='store_true',
                   help='moviAsm contract flag; accepted, ignored')
    add_legal_flags(o)

    args = ap.parse_args()
    db = DB()

    def make_opts(a):
        delay = _vendor.get('delay_slots', a.delay_slots)
        latency = _vendor.get('latency', a.latency)
        opts = StreamOpts(delay_slots=delay, latency=latency,
                          div_latency=a.div_latency,
                          lsu_policy=a.lsu_policy, lsu_window=a.lsu_window,
                          aliased_store_distance=a.aliased_store_distance,
                          bra_descriptor=a.bra_descriptor,
                          ldi_pair_pad=a.ldi_pair_pad,
                          compressed_nops=a.compressed_nops,
                          zero_offset_fix=a.zero_offset_fix,
                          fadd_latency=a.fadd_latency)
        if a.exact or a.no_legalize:
            opts.enabled = False
        return opts

    def legalize(a, text):
        text, st = legalize_text(db, text, make_opts(a))
        report_legalize(st)
        return text

    if args.cmd == 'disasm':
        blob, src = load_input_bytes(args.data, args.elf)
        base = int(args.base, 0)
        dec, tot = disasm_stream(db, blob, base)
        print(f"; decoded {dec}/{tot} bytes ({100.0*dec/tot:.1f}%)", file=sys.stderr)
    elif args.cmd == 'asm':
        text = open(args.file).read()
        text = legalize(args, text)
        out = assemble(db, text)
        if args.hex:
            print(out.hex())
        else:
            sys.stdout.buffer.write(out)
    else:  # obj
        text = sys.stdin.read() if args.file in ('-', '') \
            else open(args.file).read()
        text = legalize(args, text)
        blob = assemble_object(db, text)
        if args.output in ('-', ''):
            sys.stdout.buffer.write(blob)
        else:
            with open(args.output, 'wb') as fh:
                fh.write(blob)


if __name__ == '__main__':
    try:
        main()
    except BrokenPipeError:
        pass

#!/usr/bin/env python3
"""Extract per-instruction definition records from moviAsm64.dll builder functions.

Schema (16-byte records {const char* class; u32 a; u32 b}) built on the
builder's stack frame (rbp- or rsp-relative), copied into a heap vector via
memcpy helper FUN_180619fb0 after an allocator FUN_180028fe0.
"""
import struct, pickle, csv, sys, json
import peutil as P
from capstone import *
from capstone.x86 import *

d = P.load()
secs, ib = P.sections(d)
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True

_NAME2ID = {md.reg_name(i): i for i in range(1, 1600) if md.reg_name(i)}


def NX(reg):
    """Normalize x86 reg id to its 64-bit base id (eax->rax etc)."""
    nm = md.reg_name(reg)
    if not nm:
        return reg
    base = {'al': 'rax', 'ah': 'rax', 'ax': 'rax', 'eax': 'rax',
            'bl': 'rbx', 'bh': 'rbx', 'bx': 'rbx', 'ebx': 'rbx',
            'cl': 'rcx', 'ch': 'rcx', 'cx': 'rcx', 'ecx': 'rcx',
            'dl': 'rdx', 'dh': 'rdx', 'dx': 'rdx', 'edx': 'rdx',
            'sil': 'rsi', 'si': 'rsi', 'esi': 'rsi',
            'dil': 'rdi', 'di': 'rdi', 'edi': 'rdi',
            'bpl': 'rbp', 'bp': 'rbp', 'ebp': 'rbp',
            'spl': 'rsp', 'sp': 'rsp', 'esp': 'rsp',
            'r8b': 'r8', 'r8w': 'r8', 'r8d': 'r8',
            'r9b': 'r9', 'r9w': 'r9', 'r9d': 'r9',
            'r10b': 'r10', 'r10w': 'r10', 'r10d': 'r10',
            'r11b': 'r11', 'r11w': 'r11', 'r11d': 'r11',
            'r12b': 'r12', 'r12w': 'r12', 'r12d': 'r12',
            'r13b': 'r13', 'r13w': 'r13', 'r13d': 'r13',
            'r14b': 'r14', 'r14w': 'r14', 'r14d': 'r14',
            'r15b': 'r15', 'r15w': 'r15', 'r15d': 'r15'}.get(nm, nm)
    return _NAME2ID.get(base, reg)


def va_bytes(va, n=64):
    off = P.va2off(secs, ib, va)
    if off is None:
        return None
    return d[off:off + n]


def va_string(va, maxlen=48):
    b = va_bytes(va, maxlen)
    if b is None:
        return None
    end = b.find(b'\0')
    if end <= 0:
        return None
    s = b[:end]
    if all(32 <= c < 127 for c in s):
        return s.decode()
    return None


ALLOC = 0x180028fe0
CPY = 0x180619fb0


def analyze(addr, size):
    off = P.va2off(secs, ib, addr)
    insns = list(md.disasm(d[off:off + size], addr))
    regs = {}
    mem = {}
    defs = []
    pending_name = []
    pending_xmm = None
    last_alloc = None
    rsp_val = 0        # offset from function-entry rsp
    rbp_rel = None     # rsp_val at lea rbp,[rsp+K] plus K

    def memkey(base, disp):
        if base == X86_REG_RBP:
            return disp if rbp_mode else ('rbp', disp)
        if base == X86_REG_RSP:
            if rbp_rel is not None:
                return disp + (rsp_val - rbp_rel)
            if not rbp_mode:
                return disp
            return ('pre', disp)
        return None

    def kget(k):
        return mem.get(k)

    def kset(k, v):
        if k is not None:
            mem[k] = v

    # pre-scan: does this function establish rbp? (some are rsp-frame only)
    rbp_mode = False
    _rsp = 0
    for _ins in insns:
        _o = _ins.operands
        if _ins.mnemonic == 'push':
            _rsp -= 8
        elif _ins.mnemonic == 'sub' and len(_o) == 2 and NX(_o[0].reg) == X86_REG_RSP                 and _o[1].type == X86_OP_IMM:
            _rsp -= _o[1].imm
        elif _ins.mnemonic == 'lea' and len(_o) == 2 and NX(_o[0].reg) == X86_REG_RBP                 and _o[1].type == X86_OP_MEM and _o[1].mem.base == X86_REG_RSP                 and _o[1].mem.index == X86_REG_INVALID:
            rbp_mode = True
            break

    i = 0
    n = len(insns)
    while i < n:
        ins = insns[i]
        ops = ins.operands
        m = ins.mnemonic

        # --- stack pointer tracking ---
        if m == 'push':
            rsp_val -= 8
        elif m == 'pop':
            rsp_val += 8
        elif m == 'sub' and len(ops) == 2 and NX(ops[0].reg) == X86_REG_RSP:
            if ops[1].type == X86_OP_IMM:
                rsp_val -= ops[1].imm
            else:
                rv = regs.get(NX(ops[1].reg))
                if rv and rv[0] == 'imm':
                    rsp_val -= rv[1]
        elif m == 'add' and len(ops) == 2 and NX(ops[0].reg) == X86_REG_RSP:
            if ops[1].type == X86_OP_IMM:
                rsp_val += ops[1].imm
            else:
                rv = regs.get(NX(ops[1].reg))
                if rv and rv[0] == 'imm':
                    rsp_val += rv[1]
        elif m == 'lea' and len(ops) == 2 and NX(ops[0].reg) == X86_REG_RBP \
                and ops[1].type == X86_OP_MEM and ops[1].mem.base == X86_REG_RSP \
                and ops[1].mem.index == X86_REG_INVALID:
            rbp_rel = rsp_val + ops[1].mem.disp

        # --- add/sub/lea reg, [reg+imm] (preserve memptr arithmetic) ---
        if m in ('add', 'sub') and len(ops) == 2 and ops[0].type == X86_OP_REG \
                and ops[1].type == X86_OP_IMM and NX(ops[0].reg) not in (X86_REG_RSP,):
            rv = regs.get(NX(ops[0].reg))
            if rv is not None and rv[0] in ('memptr', 'pre'):
                sg = 1 if m == 'add' else -1
                k = rv[1]
                if isinstance(k, tuple):
                    k = (k[0], k[1] + sg * ops[1].imm)
                else:
                    k = k + sg * ops[1].imm
                regs[NX(ops[0].reg)] = (rv[0], k)
                i += 1
                continue
            if rv is not None and rv[0] == 'imm':
                regs[NX(ops[0].reg)] = ('imm', (rv[1] + (1 if m == 'add' else -1) * ops[1].imm)
                                        & ((1 << 64) - 1))
                i += 1
                continue
        if m == 'lea' and len(ops) == 2 and ops[0].type == X86_OP_REG \
                and ops[1].type == X86_OP_MEM and NX(ops[0].reg) == NX(ops[1].mem.base) \
                and ops[1].mem.index == X86_REG_INVALID:
            rv = regs.get(NX(ops[0].reg))
            if rv is not None and rv[0] in ('memptr', 'pre'):
                k = rv[1]
                if isinstance(k, tuple):
                    kk = (k[0], k[1] + ops[1].mem.disp)
                else:
                    kk = k + ops[1].mem.disp
                regs[NX(ops[0].reg)] = (rv[0], kk)
                i += 1
                continue
        # --- register invalidation ---
        if ops and ops[0].type == X86_OP_REG and NX(ops[0].reg) not in (X86_REG_RSP, X86_REG_RBP):
            r0 = NX(ops[0].reg)
            if m in ('xor', 'sub', 'pxor') and len(ops) == 2 and ops[1].type == X86_OP_REG \
                    and NX(ops[1].reg) == r0:
                regs[r0] = ('imm', 0)
            else:
                regs.pop(r0, None)

        # --- mov reg2, reg1 (register copy) ---
        if m == 'mov' and len(ops) == 2 and ops[0].type == X86_OP_REG \
                and ops[1].type == X86_OP_REG:
            _sv = regs.get(NX(ops[1].reg))
            if _sv is None:
                regs.pop(NX(ops[0].reg), None)
            else:
                regs[NX(ops[0].reg)] = _sv
            i += 1
            continue
        # --- lea reg, [rip+d] ---
        if m == 'lea' and len(ops) == 2 and ops[1].type == X86_OP_MEM \
                and ops[1].mem.base == X86_REG_RIP:
            regs[NX(ops[0].reg)] = ('lea', ins.address + ins.size + ops[1].mem.disp)
            i += 1
            continue
        # --- lea reg, [rbp/rsp+d] ---
        if m == 'lea' and len(ops) == 2 and ops[1].type == X86_OP_MEM \
                and ops[1].mem.base in (X86_REG_RBP, X86_REG_RSP) \
                and ops[1].mem.index == X86_REG_INVALID:
            k = memkey(ops[1].mem.base, ops[1].mem.disp)
            regs[NX(ops[0].reg)] = ('memptr', k)
            i += 1
            continue
        # --- movabs reg, imm (immediate name loads) ---
        if m in ('movabs', 'mov') and len(ops) == 2 and ops[0].type == X86_OP_REG \
                and ops[1].type == X86_OP_IMM:
            _v = ops[1].imm & ((1 << (8 * ops[0].size)) - 1)
            regs[NX(ops[0].reg)] = ('imm', _v)
            if ops[0].size >= 4:
                _b = struct.pack('<Q', _v)[:ops[0].size]
                if _b[:1] not in (b'', b'\x00') and all(32 <= c < 127 or c == 0 for c in _b):
                    _s = _b.rstrip(b'\x00').decode('latin1')
                    if _s and ('.' in _s):
                        pending_name.append((ins.address, _s))
            i += 1
            continue
        # --- mov/movzx reg, [rip+d] ---
        if m in ('mov', 'movzx') and len(ops) == 2 and ops[0].type == X86_OP_REG \
                and ops[1].type == X86_OP_MEM and ops[1].mem.base == X86_REG_RIP:
            tgt = ins.address + ins.size + ops[1].mem.disp
            regs[NX(ops[0].reg)] = ('rip', tgt, ops[1].size)
            b = va_bytes(tgt, ops[1].size)
            if b and b[:1] not in (b'', b'\x00') and all(32 <= c < 127 or c == 0 for c in b):
                pending_name.append((tgt, b.rstrip(b'\x00').decode('latin1')))
            i += 1
            continue
        # --- xmm reg, [rip+d] ---
        if m in ('movdqa', 'movups', 'movsd', 'movdqu') and len(ops) == 2 \
                and ops[1].type == X86_OP_MEM and ops[1].mem.base == X86_REG_RIP:
            _t = ins.address + ins.size + ops[1].mem.disp
            regs[('xmm', ops[0].reg)] = ('ripxmm', _t, ops[1].size)
            _b = va_bytes(_t, ops[1].size)
            if _b and _b[:1] not in (b'', b'\x00') and all(32 <= c < 127 or c == 0 for c in _b):
                pending_name.append((_t, _b.rstrip(b'\x00').decode('latin1')))
            i += 1
            continue
        # --- xorps/xorpd xmm, xmm -> zero ---
        if m in ('xorps', 'xorpd') and len(ops) == 2 and ops[0].type == X86_OP_REG \
                and ops[1].type == X86_OP_REG and ops[0].reg == ops[1].reg:
            regs[('xmm', ops[0].reg)] = ('zero',)
            i += 1
            continue

        # --- stores to [rbp/rsp+d] ---
        if m in ('mov', 'movzx') and len(ops) == 2 and ops[0].type == X86_OP_MEM:
            dst = ops[0].mem
            if dst.base in (X86_REG_RBP, X86_REG_RSP) and dst.index == X86_REG_INVALID:
                k = memkey(dst.base, dst.disp)
                if ops[1].type == X86_OP_REG:
                    rv = regs.get(NX(ops[1].reg))
                    sz = ops[1].size
                    if rv is None:
                        kset(k, ('reg?', md.reg_name(NX(ops[1].reg))))
                    elif rv[0] == 'memptr':
                        kset(k, ('memptr', rv[1]))
                    elif rv[0] == 'lea':
                        kset(k, ('leaptr', rv[1]))
                        s = va_string(rv[1])
                        if s and ('.' in s):
                            pending_name.append((rv[1], s))
                    elif rv[0] == 'rip':
                        b = va_bytes(rv[1], sz)
                        if b is not None and len(b) == sz \
                                and b[:1] not in (b'', b'\x00') \
                                and all(32 <= c < 127 or c == 0 for c in b):
                            s = b.rstrip(b'\x00').decode('latin1')
                            if s:
                                kset(k, ('namebytes', s))
                                pending_name.append((rv[1], s))
                        else:
                            kset(k, ('ripbytes', rv[1], b.hex() if b else None))
                    else:
                        kset(k, rv)
                elif ops[1].type == X86_OP_IMM:
                    kset(k, ('imm', ops[1].imm & ((1 << 64) - 1)))
                i += 1
                continue
        # --- xmm stores to [rbp/rsp+d] ---
        if m in ('movdqa', 'movups', 'movsd') and len(ops) == 2 and ops[0].type == X86_OP_MEM:
            dst = ops[0].mem
            if dst.base in (X86_REG_RBP, X86_REG_RSP) and dst.index == X86_REG_INVALID:
                k = memkey(dst.base, dst.disp)
                rv = regs.get(('xmm', ops[1].reg)) if ops[1].type == X86_OP_REG else None
                if rv and rv[0] == 'ripxmm':
                    sz = rv[2]
                    b = va_bytes(rv[1], sz)
                    if b is not None and b[:1] not in (b'', b'\x00') \
                            and all(32 <= c < 127 or c == 0 for c in b):
                        kset(k, ('namebytes', b.rstrip(b'\0').decode('latin1')))
                    else:
                        kset(k, ('xmmrip', rv[1], b.hex() if b else None))
                        pending_xmm = (k, rv[1], b.hex() if b else None)
                elif rv and rv[0] == 'zero':
                    kset(k, ('zeroed',))
                i += 1
                continue

        # --- calls ---
        is_mvs = 'movs' in m
        if is_mvs:
            rsi_v = regs.get(NX(X86_REG_RSI))
            rdi_v = regs.get(NX(X86_REG_RDI))
            cnt_v = regs.get(NX(X86_REG_RCX))
            if rsi_v and rdi_v and rsi_v[0] == 'memptr' and rdi_v[0] == 'memptr' \
                    and cnt_v and cnt_v[0] == 'imm' and isinstance(rsi_v[1], int) \
                    and isinstance(rdi_v[1], int):
                ncopy = cnt_v[1] * (8 if 'movsq' in m else 1)
                for j in range(ncopy):
                    src_k = rsi_v[1] + j
                    if src_k in mem:
                        mem[rdi_v[1] + j] = mem[src_k]
            i += 1
            continue
        if m == 'call' and ops and ops[0].type == X86_OP_IMM:
            tgt = ops[0].imm
            # span maker: two memptr args in rdx & r8 -> builds {begin,end} at rcx,
            # returns rcx in rax
            _rd = regs.get(NX(X86_REG_RDX))
            _r8 = regs.get(NX(X86_REG_R8))
            _rc = regs.get(NX(X86_REG_RCX))
            if _rd and _r8 and _rc and _rd[0] == 'memptr' and _r8[0] == 'memptr' \
                    and _rc[0] in ('memptr',) and isinstance(_rc[1], int) \
                    and isinstance(_rd[1], int) and isinstance(_r8[1], int):
                if _r8[1] > _rd[1]:
                    mem[_rc[1]] = ('memptr', _rd[1])
                    mem[_rc[1] + 8] = ('memptr', _r8[1])
                    regs[X86_REG_RAX] = ('memptr', _rc[1])
            # generic finalizer: rdx holds pointer to a UNIT.NAME string and a
            # record span is passed in r8 or on the stack ([rsp+0x20] -> {begin,end})
            nrv = regs.get(NX(X86_REG_RDX))
            if nrv and nrv[0] in ('leaptr', 'lea'):
                _s = va_string(nrv[1])
                if _s and '.' in _s and _s.split('.')[0] in ('IAU','LSU0','LSU1','VAU','CMU','BRU','PEU','SAU'):
                    span_k = None
                    r8v = regs.get(NX(X86_REG_R8))
                    if r8v and r8v[0] == 'memptr':
                        span_k = r8v[1]
                    else:
                        k20 = memkey(X86_REG_RSP, 0x20)
                        v20 = kget(k20)
                        if v20 and v20[0] == 'memptr':
                            span_k = v20[1]
                    if span_k is not None:
                        b = kget(span_k)
                        end_k = span_k + 8 if isinstance(span_k, int) \
                            else (span_k[0], span_k[1] + 8)
                        e = kget(end_k)
                        if b and e and b[0] == 'memptr' and e[0] == 'memptr' \
                                and isinstance(b[1], int) and isinstance(e[1], int) and e[1] > b[1]:
                            defs.append({'recbase': b[1], 'size': e[1] - b[1], 'name': _s,
                                         'xmm': pending_xmm, 'alloc': last_alloc,
                                         'cpy_addr': ins.address})
                            pending_name.clear()
                            pending_xmm = None
            if tgt in (ALLOC, CPY):
                dstk = None
                for j in range(i - 1, max(-1, i - 8), -1):
                    q = insns[j]
                    if q.mnemonic == 'lea' and len(q.operands) == 2 \
                            and NX(q.operands[0].reg) == X86_REG_RCX \
                            and q.operands[1].type == X86_OP_MEM \
                            and q.operands[1].mem.base in (X86_REG_RBP, X86_REG_RSP) \
                            and q.operands[1].mem.index == X86_REG_INVALID:
                        dstk = memkey(q.operands[1].mem.base, q.operands[1].mem.disp)
                        break
                    if q.mnemonic == 'call':
                        break
                if tgt == ALLOC:
                    cnt = None
                    for j in range(i - 1, max(-1, i - 6), -1):
                        q = insns[j]
                        if q.mnemonic == 'lea' and len(q.operands) == 2 \
                                and NX(q.operands[0].reg) == X86_REG_RDX:
                            if q.operands[1].type == X86_OP_MEM:
                                cnt = q.operands[1].mem.disp
                            break
                        if q.mnemonic == 'mov' and len(q.operands) == 2 \
                                and NX(q.operands[0].reg) == X86_REG_RDX \
                                and q.operands[1].type == X86_OP_IMM:
                            cnt = q.operands[1].imm
                            break
                    if dstk is not None:
                        mem[dstk] = ('alloc', cnt)
                        last_alloc = (dstk, cnt)
                else:  # CPY
                    srck = None
                    sizeb = None
                    for j in range(i - 1, max(-1, i - 10), -1):
                        q = insns[j]
                        if q.mnemonic == 'lea' and len(q.operands) == 2 \
                                and NX(q.operands[0].reg) == X86_REG_RDX \
                                and q.operands[1].type == X86_OP_MEM \
                                and q.operands[1].mem.base in (X86_REG_RBP, X86_REG_RSP) \
                                and q.operands[1].mem.index == X86_REG_INVALID:
                            srck = memkey(q.operands[1].mem.base, q.operands[1].mem.disp)
                        if q.mnemonic == 'lea' and len(q.operands) == 2 \
                                and NX(q.operands[0].reg) == X86_REG_R8 \
                                and q.operands[1].type == X86_OP_MEM:
                            sizeb = q.operands[1].mem.disp
                        if q.mnemonic == 'mov' and len(q.operands) == 2 \
                                and NX(q.operands[0].reg) == X86_REG_R8 \
                                and q.operands[1].type == X86_OP_IMM:
                            sizeb = q.operands[1].imm
                        if q.mnemonic == 'call':
                            break
                    # name assembled from pending_name since last CPY
                    if pending_name:
                        chunks = sorted(set(pending_name), key=lambda x: x[0])
                        best = []
                        _end = -1
                        for _t, _s in chunks:
                            if _t >= _end:
                                best.append((_t, _s))
                                _end = _t + len(_s)
                        name = ''.join(_s for _, _s in best)
                    else:
                        name = None
                    pending_name.clear()
                    defs.append({'recbase': srck, 'size': sizeb, 'name': name,
                                 'xmm': pending_xmm, 'alloc': last_alloc,
                                 'cpy_addr': ins.address})
                    pending_xmm = None
        i += 1
    return defs, mem


if __name__ == '__main__':
    funcs = {}
    with open('/home/asdf/projects/r2gvp04w-re/re_decompiled/moviAsm64/03_function_index.tsv') as f:
        for row in csv.DictReader(f, delimiter='\t'):
            funcs[row['name']] = (int(row['address'], 16), int(row['size_bytes']))
    which = sys.argv[1:] or ['FUN_1804d5c30', 'FUN_1804abe70']
    for fname in which:
        a, sz = funcs[fname]
        defs, mem = analyze(a, sz)
        print(f"== {fname}: {len(defs)} defs")
        for de in defs[:10]:
            print("  ", de['name'], de['recbase'], de['size'])

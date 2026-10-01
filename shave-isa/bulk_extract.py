#!/usr/bin/env python3
import csv, pickle, re, bisect, sys
import peutil as P
import extract_defs as E

d = E.d
mn, _, _ = pickle.load(open('mn.pkl','rb'))
canon = set(mn)
UNIT = re.compile(r'^(IAU|LSU0|LSU1|VAU|CMU|BRU|PEU|SAU)\.')
CLASS16 = set('D X 0x IW IR VW VR IE IH IHL VE VH VEI IRR IRW VRR VRW F T E P BS Av As'.split())

funcs = {}
with open('/home/asdf/projects/r2gvp04w-re/re_decompiled/moviAsm64/03_function_index.tsv') as f:
    for row in csv.DictReader(f, delimiter='\t'):
        funcs[row['name']] = (int(row['address'],16), int(row['size_bytes']))

hits = pickle.load(open('rel32_all.pkl','rb'))
fidx = sorted((a, sz, nm) for nm,(a,sz) in funcs.items())
starts = [x[0] for x in fidx]
func_refs = {}
for o, t in hits:
    if P.off2sec(E.secs, o) != '.text': continue
    va = P.off2va(E.secs, E.ib, o)
    i = bisect.bisect_right(starts, va) - 1
    if i < 0: continue
    a, sz, fname = fidx[i]
    if a <= va < a+sz:
        func_refs.setdefault(fname, []).append((va, t))
for f in func_refs: func_refs[f].sort()

def parse16(mem, base, nrec):
    recs, ok = [], True
    for k in range(nrec):
        c = mem.get(base+16*k); v1 = mem.get(base+16*k+8); v2 = mem.get(base+16*k+12)
        if c is None or v1 is None or v2 is None:
            ok = False; recs.append(None); continue
        if c[0]=='leaptr':
            cv = E.va_string(c[1]) or f"<{c[1]:#x}>"
        elif c[0]=='reg?': cv = f"<?{c[1]}>"
        elif c[0]=='imm': cv = '<imm?>'
        elif c[0]=='memptr': cv = '<mem>'
        else: cv = str(c)
        a1 = v1[1] if v1[0]=='imm' else str(v1)
        a2 = v2[1] if v2[0]=='imm' else str(v2)
        recs.append((cv, a1, a2))
    return recs, ok

def parse20(mem, base, nrec):
    recs, ok = [], True
    for k in range(nrec):
        vals = []
        for j in range(5):
            v = mem.get(base+20*k+4*j)
            vals.append(v[1] if v and v[0]=='imm' else None)
        if any(v is None for v in vals):
            ok = False; recs.append(None); continue
        recs.append(tuple(vals))
    return recs, ok

all_defs = []
stats = {}
for fname in sorted(funcs):
    try:
        defs, mem = E.analyze(*funcs[fname])
    except Exception as ex:
        print("FAIL", fname, ex, file=sys.stderr)
        continue
    if not defs: continue
    drefs = func_refs.get(fname, [])
    prev_cpy = funcs[fname][0]
    prev_name = None
    for de in defs:
        rb, size = de['recbase'], de['size']
        if rb is None or size is None or not isinstance(rb, int): continue
        cnt = de['alloc'][1] if de['alloc'] else None
        recsize = None
        if cnt and size % cnt == 0 and cnt > 0:
            recsize = size // cnt
        if recsize not in (16, 20):
            if size % 16 == 0: recsize = 16
            elif size % 20 == 0: recsize = 20
            else: continue
        nrec = size // recsize
        if recsize == 16:
            recs, ok = parse16(mem, rb, nrec)
            known = sum(1 for r in recs if r and r[0] in CLASS16)
            if known == 0 and nrec > 0 and size % 20 == 0:
                recsize = 20
        if recsize == 20:
            recs, ok = parse20(mem, rb, nrec)
        else:
            recs, ok = parse16(mem, rb, nrec)
        cpy = de.get('cpy_addr', 0)
        cand = [t for va_, t in drefs if prev_cpy <= va_ <= cpy]
        nm = None; via = None
        if cand:
            s = E.va_string(min(cand), 48)
            if s and UNIT.match(s):
                nm, via = s, 'nameva'
        if nm is None:
            p = de['name']
            if p:
                if p in canon: nm, via = p, 'pending'
                else:
                    ext = [c for c in canon if c.startswith(p)]
                    if len(ext)==1: nm, via = ext[0], 'extended'
                    elif ext: nm, via = max(ext,key=len), 'suspect'
                    else: nm, via = p, 'suspect'
        if nm is None and prev_name:
            nm, via = prev_name, 'carry'
        if nm is None: via = 'none'
        if via != 'carry':
            prev_name = nm
        stats[via] = stats.get(via,0)+1
        all_defs.append({'func': fname, 'name': nm, 'via': via, 'recs': recs, 'ok': ok,
                         'schema': recsize, 'n': nrec, 'cpy': cpy})
        prev_cpy = cpy
print(stats)
print("total:", len(all_defs))
s1 = [x for x in all_defs if x['schema']==16 and x['ok']]
s2 = [x for x in all_defs if x['schema']==20]
print("schema16(ok):", len(s1), "schema20:", len(s2))
n1 = set(x['name'] for x in s1 if x['name'])
n2 = set(x['name'] for x in s2 if x['name'])
print("unique s16:", len(n1), "s20:", len(n2), "union:", len(n1|n2), "canon:", len(canon))
missing = canon - (n1|n2)
print("missing:", len(missing))
from collections import Counter
print("missing by unit:", Counter(m.split('.')[0] for m in missing).most_common())
bad = [x for x in all_defs if x['via'] in ('none','suspect')]
print("bad:", len(bad), Counter(x['func'] for x in bad).most_common(6))
pickle.dump(all_defs, open('defs_extracted.pkl','wb'))

#!/usr/bin/env python3
import bisect,re,sys
DEC='/home/asdf/projects/r2gvp04w-re/re_decompiled/movicomp/decompiled_all.c'
hdr=re.compile(r'/\* ===== Function (\d+): (\S+) @ (\S+) size=(\d+)')
starts=[];names=[]
with open(DEC,encoding='latin1') as f:
    for i,line in enumerate(f,1):
        m=hdr.match(line)
        if m: starts.append(i); names.append((m.group(1),m.group(2),m.group(3),m.group(4)))
names_=[(int(n[3]),n) for n in names]
by_size=[n for _,n in sorted(names_,reverse=True)]
def at(line):
    j=bisect.bisect_right(starts,line)-1
    return names[j]
if __name__=='__main__':
    mode=sys.argv[1]
    if mode=='line':
        for t in sys.argv[2:]: print(t, at(int(t)))
    elif mode=='top':
        import json
        for s,n in by_size[:int(sys.argv[2] or 40)]:
            print('%8d %s %s %s'%(s,n[1],n[2],n[0]))
    elif mode=='range':
        lo,hi=sys.argv[2],sys.argv[3]
        for s,n in names_:
            if lo<=n[2]<=hi: print('%8d %s %s fn%s'%(s,n[1],n[2],n[0]))

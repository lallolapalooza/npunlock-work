import sys, pickle
sys.path.insert(0,'.')
from defdec import DB, load_defs
data=open('/tmp/gold192.bin','rb').read()
db=DB(load_defs())
def shift_bytes(data, nbits, start=0):
    """return (bytearray, first_bit_offset) of data[start:] rotated left by nbits"""
    v=int.from_bytes(data[start:],'little')
    # drop nbits low bits, keep the rest as a byte stream
    v >>= nbits
    n=(len(data)-start)*8 - nbits
    return v.to_bytes((n+7)//8,'little')
for nb in range(0,8):
    for start in (0x59,0x00):
        d=shift_bytes(data,nb,start)
        # greedy decode
        off=0; und=0; ninstr=0; ends=[]
        while off<len(d):
            h=db.match(d,off)
            if not h:
                off+=1; und+=1; continue
            h.sort(key=lambda r:(-bin(r['msk']).count('1'),-r['nbytes']))
            r=h[0]
            ninstr+=1; ends.append(off)
            off+=r['nbytes']
        print(f'shift={nb} start={start:#x}: covered={len(d)-und}/{len(d)} instr={ninstr}')

import re, sys
src = open('/home/asdf/projects/npunlock-work/corpus/build/cw2base.l01_silu_f32.s').read()
i = src.index('.section\t.text.controlled_act'); j = src.index('Lfunc_end', i)
old, new = sys.argv[1], sys.argv[2]
body = src[i:j]
b2 = re.sub(r'\b%s\b' % old, '@Z@', body).replace('@Z@', new)
open(sys.argv[3],'w').write(src[:i] + b2 + src[j:])

import sys; sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))
import sys
from cmpf import *
for p in sys.argv[1:]:
    a=int(p,16)
    print("#####",p,'size',hex(o.size(a)))
    for s,c in o.callers.get(a,())[:12]:
        g=fm.get(c)
        print(f"  caller {c:X} site {s:X} -> {g:X}" if g else f"  caller {c:X} site {s:X} UNMAPPED")
        if g and o.size(c)<0x1800: show(c,g)

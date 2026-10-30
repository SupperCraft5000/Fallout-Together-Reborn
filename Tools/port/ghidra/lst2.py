import sys
sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))
from ctx import *
o,n,fm,dm=get()
for w,x in (('old',o),('new',n)):
    t=set()
    for f,l in x.refs.items():
        for s,tg,b in l:
            if tg not in x.ends and x.pe.section(tg)=='.text' and x.func_of(tg) is None: t.add(tg)
    prev=[l for l in Path(__import__("os").environ["FTR_WORK"] + rf"/ghidra/work/leaf-{w}.txt").read_text().split()]
    Path(__import__("os").environ["FTR_WORK"] + rf"/ghidra/work/leaf-{w}.txt").write_text("\n".join(prev+[f"{v:X}" for v in sorted(t)]))
    print(w,len(t))

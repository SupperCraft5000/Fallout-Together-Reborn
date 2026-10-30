import sys; sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))
from ctx import *
o,n,fm,dm=get()
for w,x in (('old',o),('new',n)):
    t=set()
    for tg in x.callers: 
        if x.func_of(tg) is None and x.pe.section(tg)=='.text': t.add(tg)
    Path(__import__("os").environ["FTR_WORK"] + rf"\ghidra\work\leaf-{w}.txt").write_text("\n".join(f"{v:X}" for v in sorted(t)))
    print(w,len(t))

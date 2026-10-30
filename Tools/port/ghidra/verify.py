import sys; sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))
from ctx import *
from sim import sim, feats, jacc
o,n,_,_=get()
fm,dm=pickle.load(open(W/"ext.pkl","rb"))
rev={v:k for k,v in fm.items()}
def tok(x,f,M,side):
    t=Counter()
    for s,c in x.callees.get(f,()):
        c2=M.get(c) if side=='o' else c
        if c2 is not None and (side=='o' or c in rev): t['C%x'%c2]+=1
    for s,c,b in x.refs.get(f,()):
        c2=(M.get(c) or dm.get(c)) if side=='o' else c
        if side=='o':
            if c2 is not None: t['D%x'%c2]+=1
        else:
            if c in rev or c in drev: t['D%x'%c2]+=1
        st=x.pe.cstr(c)
        if st: t['S'+st]+=1
    for s,c in x.callers.get(f,()):
        c2=M.get(c) if side=='o' else c
        if c2 is not None and (side=='o' or c in rev): t['K%x'%c2]+=1
    m,cc=feats(x,f)
    for k,v in cc.items():
        if int(k,16)>=0x18: t['I'+k]+=v
    return t
drev={v:k for k,v in dm.items()}
print("index...",flush=True)
inv=defaultdict(list); nt={}
for g in n.ends:
    if g!=n.root.get(g,g): continue
    if g in rev: continue
    t=tok(n,g,fm,'n'); nt[g]=t
    for k in t: inv[k].append(g)
N=len(nt)
idf=lambda k:math.log(1+N/(1+len(inv[k])))
res={}
for r in opens():
    v=int(r['old_rva'],16)
    if r['art'].startswith('Daten'): continue
    if v in fm:
        g=fm[v]
        a,b=tok(o,v,fm,'o'),tok(n,g,fm,'n') if g in n.ends else Counter()
        print(f"{r['name']:<28} {v:#x} -> {g:#x} sim {sim(o,n,v,g,fm,dm):.2f} size {o.size(v):#x}/{n.size(g):#x} tok-jacc {jacc(a,b):.2f} shared {len(a.keys()&b.keys())}/{len(a)} ")
        continue
    a=tok(o,v,fm,'o')
    sc=Counter()
    for k,w in a.items():
        if len(inv[k])>2000: continue
        for g in inv[k]: sc[g]+=idf(k)*min(w,nt[g][k])
    best=[]
    for g,s in sc.most_common(8):
        best.append(f"{g:#x}:{s:.0f}/j{jacc(a,nt[g]):.2f}/s{sim(o,n,v,g,fm,dm):.2f}/{n.size(g):#x}")
    print(f"{r['name']:<28} {v:#x} OPEN size {o.size(v):#x} tok {len(a)} cand {' '.join(best[:5])}")

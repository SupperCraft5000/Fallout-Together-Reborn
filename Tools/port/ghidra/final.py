import sys, json
sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))
from cmpf import *
from sim import sim, feats, jacc
src=(Path(__file__).resolve().parents[1]/"graphport.py").read_text(encoding="utf-8")
ns={}
exec(compile(src[:src.index("ap = argparse.ArgumentParser()")],"gp","exec"),ns)
vo=ns["vtables"](ns["Pe"](OLD_EXE)); vn=ns["vtables"](ns["Pe"](NEW_EXE))
ordo=sorted((v,k) for k,v in vo.items()); ks=[v for v,k in ordo]
def slot(p):
    i=bisect.bisect_right(ks,p)-1
    if i<0: return None
    v,k=ordo[i]
    return (k,(p-v)//8) if (p-v)//8<600 else None
rows={}
for r in opens():
    v=int(r['old_rva'],16)
    if r['art'].startswith('Daten'): continue
    g=fm.get(v)
    if g is None: print(r['name'],'OPEN'); continue
    ca={fm.get(f) for _,f in o.callers.get(v,())}-{None}; cb={f for _,f in n.callers.get(g,())}
    A,B=ev(o,v,1),ev(n,g,0)
    q=difflib.SequenceMatcher(None,A,B,autojunk=False).ratio() if A or B else 1
    ce=[fm.get(t) for _,t in o.callees.get(v,())]; ceb={t for _,t in n.callees.get(g,())}
    cem=[c for c in ce if c is not None]; ces=sum(1 for c in cem if c in ceb)
    vs=[]
    for p in o.ptrs.get(v,()):
        s=slot(p)
        if s:
            nv=vn.get(s[0])
            if nv:
                t=struct.unpack('<Q',n.pe.bytes(nv+s[1]*8,8))[0]-0x140000000
                vs.append((s[0][0].decode()[:40],s[1],n.root.get(t,t)==g))
    ma,ca2=feats(o,v);mb,cb2=feats(n,g)
    rows[r['name']]=dict(id=r['og_id'],old=v,new=g,sim=round(sim(o,n,v,g,fm,dm),2),seq=round(q,2),cal=(len(ca&cb),len(ca)),cee=(ces,len(cem)),size=(o.size(v),n.size(g)),vt=(sum(1 for x in vs if x[2]),len(vs),vs[:2]),konst=round(jacc(ca2,cb2),2))
    print(r['name'],hex(v),hex(g),rows[r['name']])
pickle.dump(rows,open(W/"final.pkl","wb"))

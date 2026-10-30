import sys; sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))
from ctx import *
import difflib
o,n,_,_=get()
fm,dm=pickle.load(open(W/"ext.pkl","rb"))
rev={v:k for k,v in fm.items()}; drev={v:k for k,v in dm.items()}
def ev(x,f,old):
    s=[]
    for site,t in x.callees.get(f,()): s.append((site,'c',t))
    for site,t,b in x.refs.get(f,()): s.append((site,'r',t))
    s.sort(); out=[]
    for site,k,t in s:
        d=x.describe(t)
        if d: out.append(k+d[:40]); continue
        if old: m=fm.get(t,dm.get(t))
        else: m=t if (t in rev or t in drev) else None
        out.append(f"{k}{m:X}" if m is not None else f"{k}?{t:X}")
    return out
def show(a,b):
    A,B=ev(o,a,1),ev(n,b,0)
    sm=difflib.SequenceMatcher(None,A,B,autojunk=False)
    print(f"== {a:X} <-> {b:X} size {o.size(a):#x}/{n.size(b):#x} ratio {sm.ratio():.2f} callers {len(o.callers.get(a,()))}/{len(n.callers.get(b,()))} ptr {len(o.ptrs.get(a,()))}/{len(n.ptrs.get(b,()))}")
    for tag,i,j,k,l in sm.get_opcodes():
        if tag=='equal': print('   =',' '.join(A[i:j])[:300])
        else: print('   -',' '.join(A[i:j])[:300]); print('   +',' '.join(B[k:l])[:300])
    ca={fm.get(f) for s,f in o.callers.get(a,())}-{None}
    cb={f for s,f in n.callers.get(b,())}
    print('   callers mapped',len(ca),'shared',len(ca&cb))
if __name__=='__main__':
    for p in sys.argv[1:]:
        if ':' in p:
            a,b=p.split(':'); show(int(a,16),int(b,16))

import sys; sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))
from ctx import *
o,n,fm,dm=get()
print(len(o.blobs),len(n.blobs),len(fm),len(dm))
for r in opens():
    v=int(r['old_rva'],16)
    if r['art'].startswith('Daten'):
        rf=o.refd.get(v,[]); print(r['name'],hex(v),'DATA refs',len(rf),'mappedfn',sum(1 for s,f in rf if f in fm), 'pat',Counter(o.func_of(s) in fm for s,f in rf)); continue
    cs=o.callers.get(v,[]); ce=o.callees.get(v,[]); rd=o.refd.get(v,[]); p=o.ptrs.get(v,[])
    print(r['name'],hex(v),'in',v in o.ends,'root',hex(o.root.get(v,v)),'size',hex(o.size(v)),'ins',len(o.listing(v)),'callers',len(cs),'mapped',sum(1 for s,f in cs if f in fm),'callees',len(ce),'mapped',sum(1 for s,t in ce if t in fm),'lea',len(rd),'ptr',len(p))

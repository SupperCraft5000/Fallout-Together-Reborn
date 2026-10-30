"""Lädt beide Dumps mit Cache und erweiterter Karte."""
import pickle, csv, sys, re, math, struct, bisect
from collections import Counter, defaultdict
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))

from fdis import *
W = Path(__import__("os").environ["FTR_WORK"]) / "ghidra" / "work"
def get():
    p = W / "ctx.pkl"
    if p.exists():
        with open(p, "rb") as f:
            return pickle.load(f)
    o, n = Dump("old"), Dump("new")
    for x in (o, n):
        x.dataptrs()
        x.callees, x.callers, x.refs, x.refd, x.ptrs = dict(x.callees), dict(x.callers), dict(x.refs), dict(x.refd), dict(x.ptrs)
    fm0, dm = load_map()
    votes = defaultdict(Counter)
    for a, b in fm0.items():
        votes[o.root.get(a, a)][n.root.get(b, b)] += 1 + (a in o.root and o.root[a] == a) * 2
    fm = {a: c.most_common(1)[0][0] for a, c in votes.items()}
    ids = {r["real_id"]: int(r["rva_1_10_163"], 16) for r in csv.DictReader(open(PORT / "ids-1.10.163.csv", encoding="utf-8"))}
    ids2 = {r["id"]: int(r["rva_1_10_163"], 16) for r in csv.DictReader(open(PORT / "ids-1.10.163.csv", encoding="utf-8"))}
    for r in csv.DictReader(open(PORT / "offsets-1-11-240-0.csv", encoding="utf-8")):
        v = ids.get(r["og_id"], ids2.get(r["og_id"]))
        if v is None: continue
        nv = int(r["rva"], 16)
        (fm if v in o.ends else dm)[o.root.get(v, v)] = n.root.get(nv, nv)
    with open(p, "wb") as f:
        pickle.dump((o, n, fm, dm), f, protocol=5)
    return o, n, fm, dm
def opens():
    return list(csv.DictReader(open(PORT / "open-1-11-240-0.csv", encoding="utf-8")))

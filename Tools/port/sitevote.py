r"""Findet Funktionen und Daten über ihre Verwendungsstelle in bereits zugeordneten Funktionen.

Pro zugeordnetem Funktionspaar werden die Aufrufziele (bzw. Datenzugriffe) in Reihenfolge an bekannten Paaren
ausgerichtet. Liegt das gesuchte Ziel in einem Abschnitt, stimmen alle noch freien Ziele im selben Abschnitt der neuen
Funktion anteilig ab. Verlangt sind mehrere Verwendungsstellen und ein klarer Vorsprung.

python -I sitevote.py --old alt.exe --new neu.exe --map map.csv --ids ids.csv --out out.csv [--holdout 2000]
"""
import argparse
import csv
import random
import sys
from collections import Counter, defaultdict
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument("--old", required=True)
ap.add_argument("--new", required=True)
ap.add_argument("--map", nargs="+", required=True)
ap.add_argument("--ids")
ap.add_argument("--out")
ap.add_argument("--holdout", type=int, default=0)
ap.add_argument("--min-sites", type=int, default=2)
ap.add_argument("--min-lead", type=float, default=2.0)
a = ap.parse_args()

src = Path(__file__).with_name("graphport.py").read_text(encoding="utf-8")
ns = {}
exec(compile(src[:src.index("def vtables(pe):")], "graphport", "exec"), ns)
Pe = ns["Pe"]
old, new = Pe(a.old), Pe(a.new)
oc, od = old.scan()
nc, nd = new.scan()

fmap, dmap = {}, {}
for m in a.map:
    with open(m, encoding="utf-8") as fh:
        for r in csv.DictReader(fh):
            (fmap if r["kind"] == "f" else dmap).setdefault(int(r["old_rva"], 16), int(r["new_rva"], 16))


def sections(lst, known):
    """Teilt eine Zielliste an bekannten Zielen in Abschnitte."""
    out, cur, key = [], [], None
    for t in lst:
        if t in known:
            out.append((key, known[t], cur))
            cur, key = [], known[t]
        else:
            cur.append(t)
    out.append((key, None, cur))
    return out


def votes_for(targets, callmap_old, callmap_new, known, fm):
    """known: bekanntes Mapping dieser Art (Funktionen oder Daten)."""
    tset, used = set(targets), set(known.values())
    votes, sites = defaultdict(Counter), Counter()
    users = defaultdict(set)
    for f, lst in callmap_old.items():
        if f in fm:
            for _, t in lst:
                if t in tset:
                    users[t].add(f)
    for t, fs in users.items():
        for f in fs:
            o = [x for _, x in callmap_old.get(f, [])]
            n = [x for _, x in callmap_new.get(fm[f], [])]
            secn = {}
            for k0, k1, cur in sections(n, {v: v for v in used}):
                secn.setdefault((k0, k1), []).extend(x for x in cur if x not in used)
            for k0, k1, cur in sections(o, known):
                if t in cur:
                    cand = set(secn.get((k0, k1), []))
                    if cand:
                        sites[t] += 1
                        for c in cand:
                            votes[t][c] += 1 / len(cand)
                    break
    res = {}
    for t, v in votes.items():
        top = v.most_common(2)
        lead = top[0][1] / top[1][1] if len(top) > 1 else 99
        if sites[t] >= a.min_sites and top[0][1] >= 1.5 and lead >= a.min_lead:
            res[t] = (top[0][0], round(top[0][1], 2), sites[t])
    return res


if a.holdout:
    random.seed(2)
    hide = random.sample(sorted(fmap), a.holdout)
    hs = set(hide)
    rest = {k: v for k, v in fmap.items() if k not in hs}
    r = votes_for(hide, oc, nc, rest, rest)
    ok = sum(v[0] == fmap[k] for k, v in r.items())
    print(f"Funktionen Holdout: {len(r)} von {a.holdout}, {ok} richtig, {len(r) - ok} falsch")
    hide = random.sample(sorted(dmap), min(a.holdout, len(dmap)))
    hs = set(hide)
    rest = {k: v for k, v in dmap.items() if k not in hs}
    r = votes_for(hide, od, nd, rest, fmap)
    ok = sum(v[0] == dmap[k] for k, v in r.items())
    print(f"Daten Holdout: {len(r)} von {len(hide)}, {ok} richtig, {len(r) - ok} falsch")
    sys.exit()

with open(a.ids, encoding="utf-8") as fh:
    rows = list(csv.DictReader(fh))
frows, drows = {}, {}
for r in rows:
    rva = int(r["old_rva"], 16)
    if old.section_of(rva) in (".data", ".rdata"):
        drows[rva] = r
    else:
        f = old.func_of(rva)
        if f == rva:
            frows[rva] = r
rf = votes_for(list(frows), oc, nc, fmap, fmap)
rd = votes_for(list(drows), od, nd, dmap, fmap)
with open(a.out, "w", newline="", encoding="utf-8") as fh:
    w = csv.writer(fh)
    w.writerow(["og_id", "rva", "name", "votes", "sites"])
    for src_, tbl in ((rf, frows), (rd, drows)):
        for t, (g, s, c) in src_.items():
            w.writerow([tbl[t]["og_id"], f"{g:X}", tbl[t]["name"], s, c])
print(f"Funktionen {len(rf)} von {len(frows)}, Daten {len(rd)} von {len(drows)} -> {a.out}")

r"""Ordnet Funktionen zu, die graphport.py nicht gefunden hat, über Ähnlichkeit ihrer Nachbarschaft.

Merkmale einer Funktion: zugeordnete Aufrufziele, zugeordnete Aufrufer, zugeordnete Daten, Strings.
Bewertet wird mit Jaccard, verlangt sind ein klarer Abstand zum Zweitbesten und mehrere gemeinsame Merkmale.
Mit --holdout N werden N bekannte Paare versteckt und die Trefferquote gemessen.

python -I fuzzport.py --old alt.exe --new neu.exe --map map.csv --ids open.csv --out fuzz.csv [--holdout 500]
"""
import argparse
import csv
import importlib.util
import random
import sys
from collections import Counter, defaultdict
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument("--old", required=True)
ap.add_argument("--new", required=True)
ap.add_argument("--map", required=True)
ap.add_argument("--ids", help="CSV mit Spalten real_id,name,rva_1_10_163 der gesuchten Funktionen")
ap.add_argument("--out")
ap.add_argument("--holdout", type=int, default=0)
ap.add_argument("--min-score", type=float, default=0.5)
ap.add_argument("--min-shared", type=int, default=3)
ap.add_argument("--min-margin", type=float, default=0.2)
ap.add_argument("--rounds", type=int, default=4)
a = ap.parse_args()

# Pe aus graphport.py laden, ohne dessen Hauptteil auszuführen
src = Path(__file__).with_name("graphport.py").read_text(encoding="utf-8")
ns = {}
exec(compile(src[:src.index("def vtables(pe):")], "graphport", "exec"), ns)
Pe = ns["Pe"]

old, new = Pe(a.old), Pe(a.new)
oc, od = old.scan()
nc, nd = new.scan()


def graph(calls, data, pe):
    callees, callers, datas, strs = defaultdict(set), defaultdict(set), defaultdict(set), defaultdict(set)
    for f, lst in calls.items():
        for _, t in lst:
            callees[f].add(t)
            callers[t].add(f)
    for f, lst in data.items():
        for _, t in lst:
            datas[f].add(t)
            s = pe.cstring(t)
            if s:
                strs[f].add(s)
    return callees, callers, datas, strs


og = graph(oc, od, old)
ng = graph(nc, nd, new)

fmap, dmap = {}, {}
with open(a.map, encoding="utf-8") as fh:
    for r in csv.DictReader(fh):
        (fmap if r["kind"] == "f" else dmap)[int(r["old_rva"], 16)] = int(r["new_rva"], 16)


def tokens_old(f, fm):
    t = set()
    t |= {("c", fm[x]) for x in og[0].get(f, ()) if x in fm}
    t |= {("r", fm[x]) for x in og[1].get(f, ()) if x in fm}
    t |= {("d", dmap[x]) for x in og[2].get(f, ()) if x in dmap}
    t |= {("s", x) for x in og[3].get(f, ())}
    return t


def build_index(fm):
    """Merkmale aller neuen Funktionen, die noch frei sind."""
    used = set(fm.values())
    mapped_new, mapped_data = used, set(dmap.values())
    index = defaultdict(set)
    toks = {}
    for g in new.starts:
        if g in used:
            continue
        t = set()
        t |= {("c", x) for x in ng[0].get(g, ()) if x in mapped_new}
        t |= {("r", x) for x in ng[1].get(g, ()) if x in mapped_new}
        t |= {("d", x) for x in ng[2].get(g, ()) if x in mapped_data}
        t |= {("s", x) for x in ng[3].get(g, ())}
        toks[g] = t
        for k in t:
            index[k].add(g)
    return index, toks


def match(targets, fm):
    fm = dict(fm)
    res = {}
    for rnd in range(a.rounds):
        index, toks = build_index(fm)
        props = {}
        for f in targets:
            if f in res:
                continue
            t = tokens_old(f, fm)
            if len(t) < a.min_shared:
                continue
            cand = Counter()
            for k in t:
                if len(index.get(k, ())) <= 200:
                    for g in index.get(k, ()):
                        cand[g] += 1
            scored = sorted(((c / len(t | toks[g]), c, g) for g, c in cand.items()), reverse=True)
            if not scored:
                continue
            s0, c0, g0 = scored[0]
            s1 = scored[1][0] if len(scored) > 1 else 0.0
            if s0 >= a.min_score and c0 >= a.min_shared and s0 - s1 >= a.min_margin:
                props[f] = (g0, s0, c0)
        owners = Counter(v[0] for v in props.values())
        added = 0
        for f, v in props.items():
            if owners[v[0]] == 1:
                res[f] = v
                fm[f] = v[0]
                added += 1
        print(f"Runde {rnd + 1}: {added} neu, {len(res)} gesamt")
        if not added:
            break
    return res


if a.holdout:
    random.seed(1)
    hide = random.sample(sorted(fmap), a.holdout)
    rest = {k: v for k, v in fmap.items() if k not in set(hide)}
    res = match(hide, rest)
    ok = sum(v[0] == fmap[f] for f, v in res.items())
    print(f"Holdout: {a.holdout} versteckt, {len(res)} zugeordnet, {ok} richtig, {len(res) - ok} falsch")
    sys.exit()

with open(a.ids, encoding="utf-8") as fh:
    rows = list(csv.DictReader(fh))
targets = {}
for r in rows:
    f = old.func_of(int(r["rva_1_10_163"], 16))
    if f is not None:
        targets[f] = r
res = match(list(targets), fmap)
with open(a.out, "w", newline="", encoding="utf-8") as fh:
    w = csv.writer(fh)
    w.writerow(["og_id", "rva", "name", "score", "shared"])
    for f, (g, s, c) in res.items():
        r = targets[f]
        w.writerow([r["real_id"], f"{g + int(r['rva_1_10_163'], 16) - f:X}", r["name"], f"{s:.2f}", c])
print(f"{len(res)} von {len(targets)} zugeordnet -> {a.out}")

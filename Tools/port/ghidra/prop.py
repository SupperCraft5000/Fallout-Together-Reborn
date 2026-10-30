r"""Ausbreitung auf Stammfunktionen: Aufrufe, Tail-Jumps, LEA-Funktionszeiger, Datenreferenzen, VTable-Slots.
Paart die geordneten Referenzlisten zugeordneter Funktionen zwischen bekannten Stützpunkten, nur bei gleich
langen Lücken und gleicher Art. Annahme: mindestens 2 Stimmen ohne Gegenkandidat, oder 1 Stimme mit Ähnlichkeit.
Aufruf: prop.py [holdout]  -> %FTR_WORK%\ghidra\work\ext.pkl
"""
import sys, random
sys.path.insert(0, str(__import__("pathlib").Path(__file__).parent))
from ctx import *
from sim import sim

o, n, fm0, dm0 = get()
holdout = int(sys.argv[1]) if len(sys.argv) > 1 else 0
fm, dm = dict(fm0), dict(dm0)
hidden = {}
if holdout:
    random.seed(7)
    for k in random.sample(sorted(fm), holdout):
        hidden[k] = fm.pop(k)
SEED = {0xE211A0: 0xCE9A70, 0xD8E370: 0xC78980, 0xDFCAB0: 0xCCD840, 0xD6E9A0: 0xC57B60, 0xD6ED80: 0xC57F90,
        0x1D17E10: 0x1825340, 0x1B2BE30: 0x1672540, 0xD35930: 0xC2F910, 0x64E530: 0x6BC5E0, 0xCD3980: 0xBE3CC0,
        0xCD6680: 0xBE6710, 0xE1EB50: 0xCE7FB0, 0xF60320: 0xDF21E0, 0x34D490: 0x4799B0, 0xE83A60: 0xD46AB0,
        0xC6D4F0: 0xB8C940}
if not holdout:
    for k, v in SEED.items():
        for a, b in list(fm.items()):
            if b == v and a != k:
                print("Paar verworfen", hex(a), hex(b))
                del fm[a]
        fm[k] = v
rev = {v: k for k, v in fm.items()}


def seq(x, f):
    s = []
    for site, t in x.callees.get(f, ()):
        s.append((site, "c", t))
    for site, t, b in x.refs.get(f, ()):
        k = "f" if t in x.ends else ("d" if x.pe.section(t) != ".text" else None)
        if k:
            s.append((site, k, t))
    s.sort()
    return s


def vtable_pairs():
    src = (Path(__file__).resolve().parents[1] / "graphport.py").read_text(encoding="utf-8")
    ns = {}
    exec(compile(src[:src.index("ap = argparse.ArgumentParser()")], "gp", "exec"), ns)
    Pg, vt = ns["Pe"], ns["vtables"]
    a, b = vt(Pg(OLD_EXE)), vt(Pg(NEW_EXE))
    pairs = []
    for key, va in a.items():
        vb = b.get(key)
        if vb is None:
            continue
        la, lb = [], []
        for x, v, l in ((o, va, la), (n, vb, lb)):
            for i in range(600):
                p = x.pe.bytes(v + i * 8, 8)
                t = struct.unpack("<Q", p)[0] - 0x140000000 if len(p) == 8 else 0
                if x.pe.section(t) != ".text":
                    break
                if i and x.refd.get(v + i * 8):  # nächste VTable
                    break
                l.append(x.root.get(t, t))
        pairs.append((key, va, vb, la, lb))
    return pairs


vts = vtable_pairs()
print("vtables", len(vts))
oseq = {}
nseq = {}


def gs(c, x, f):
    if f not in c:
        c[f] = seq(x, f)
    return c[f]


for rnd in range(30):
    votes = defaultdict(Counter)
    dvotes = defaultdict(Counter)

    def vote(a, b, w=1):
        ka, kb = a[1], b[1]
        if (ka == "d") != (kb == "d"):
            return
        if ka == "d":
            if a[2] not in dm:
                dvotes[a[2]][b[2]] += w
        elif a[2] not in fm and b[2] not in rev and a[2] in o.ends and b[2] in n.ends:
            votes[a[2]][b[2]] += w

    for f, g in list(fm.items()):
        if g not in n.ends:
            continue
        A, B = gs(oseq, o, f), gs(nseq, n, g)
        if not A or not B:
            continue
        # Stützpunkte: eindeutig gemappte Einträge in Ordnung
        idxb = defaultdict(list)
        for j, e in enumerate(B):
            idxb[e[2]].append(j)
        anch = [(-1, -1)]
        for i, e in enumerate(A):
            t = fm.get(e[2], dm.get(e[2]))
            if t is None:
                s = o.pe.cstr(e[2]) if e[1] == "d" else None
                if s and len(s) > 5:
                    pass
                continue
            js = [j for j in idxb.get(t, ()) if j > anch[-1][1]]
            if js:
                anch.append((i, js[0]))
        anch.append((len(A), len(B)))
        for (i0, j0), (i1, j1) in zip(anch, anch[1:]):
            la, lb = A[i0 + 1:i1], B[j0 + 1:j1]
            if len(la) == len(lb) and la and len(la) <= 12:
                for p, q in zip(la, lb):
                    vote(p, q)
    for key, va, vb, la, lb in vts:
        if len(la) == len(lb):
            for p, q in zip(la, lb):
                if p not in fm and q not in rev and p in o.ends and q in n.ends:
                    votes[p][q] += 2
    add = 0
    for a, c in sorted(votes.items(), key=lambda kv: -kv[1].most_common(1)[0][1]):
        (b, v), *rest = c.most_common(2)
        if a in fm or b in rev:
            continue
        r = rest[0][1] if rest else 0
        s = sim(o, n, a, b, fm, dm)
        if (v >= 2 and v >= 3 * r and s >= 0.45) or (v >= 1 and not rest and s >= 0.7):
            fm[a] = b
            rev[b] = a
            add += 1
    dr = {v: k for k, v in dm.items()}
    dadd = 0
    for a, c in dvotes.items():
        (b, v), *rest = c.most_common(2)
        if a not in dm and b not in dr and v >= 2 and v >= 3 * (rest[0][1] if rest else 0):
            dm[a] = b
            dr[b] = a
            dadd += 1
    print("Runde", rnd, "neu", add, "Daten", dadd, flush=True)
    if not add and not dadd:
        break
if hidden:
    ok = sum(1 for k, v in hidden.items() if fm.get(k) == v)
    bad = sum(1 for k, v in hidden.items() if k in fm and fm[k] != v)
    print("Holdout", len(hidden), "richtig", ok, "falsch", bad)
else:
    with open(W / "ext.pkl", "wb") as fh:
        pickle.dump((fm, dm), fh)
    print(len(fm), len(dm))
    for r in opens():
        v = int(r["old_rva"], 16)
        t = fm.get(v, dm.get(v))
        if t is not None:
            print(r["og_id"], r["name"], hex(v), "->", hex(t))

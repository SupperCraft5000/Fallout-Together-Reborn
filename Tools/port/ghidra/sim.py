"""Inhaltsähnlichkeit zweier Funktionen über Mnemonics, Konstanten/Offsets und zugeordnete Ziele."""
import re
from collections import Counter

NUM = re.compile(r"0x[0-9a-f]+")
_c = {}


def feats(x, f):
    k = (x.which, f)
    if k in _c:
        return _c[k]
    m, c = Counter(), Counter()
    for ln in x.listing(f):
        p = ln.split(" ;")[0].split(" ", 2)
        if len(p) < 2:
            continue
        m[p[1]] += 1
        if len(p) > 2:
            for t in NUM.findall(p[2]):
                if not t.startswith("0x14") or len(t) != 11:
                    c[t] += 1
    _c[k] = (m, c)
    return m, c


def jacc(a, b):
    s = sum((a & b).values())
    u = sum((a | b).values())
    return s / u if u else 1.0


def sim(o, n, a, b, fm, dm):
    ma, ca = feats(o, a)
    mb, cb = feats(n, b)
    s1, s2 = jacc(ma, mb), jacc(ca, cb)
    sa, sb = sum(ma.values()), sum(mb.values())
    if not sa or not sb:
        return 0.0
    if min(sa, sb) / max(sa, sb) < 0.4:
        return 0.0
    return 0.6 * s1 + 0.4 * s2

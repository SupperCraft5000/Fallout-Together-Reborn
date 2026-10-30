r"""Überträgt Adressen von Fallout4.exe 1.10.163 auf eine Next-Gen-exe über Anker und Aufrufgraph.

1. Anker: Funktionen, die als einzige einen bestimmten Text (String in .rdata) per RIP-Adresse laden.
   Gleicher Text in beiden Versionen -> gleiche Funktion.
2. Ausbreitung: Für zugeordnete Funktionspaare werden die Aufrufziele (call rel32) und die
   RIP-Datenzugriffe in Reihenfolge verglichen. Bei gleicher Anzahl werden sie paarweise zugeordnet.
   Jede Zuordnung sammelt Stimmen; Widersprüche verwerfen den Kandidaten.
3. Zusätzlich: eindeutige Byte-Signaturen (sigport.py) als weitere Anker.

Ergebnis: CSV og_id,rva für make_offsets.py und ein Bericht mit Stimmen pro Adresse.

Aufruf:
  python -I graphport.py --ids ids-1.10.163.csv --old Fallout4.analysis.exe --new Fallout4-1.11.240.exe
      --sig manual-1-11-240-0.csv --out graph-1-11-240-0.csv
"""
import argparse
import bisect
import csv
import re
import struct
from collections import Counter, defaultdict
from pathlib import Path


class Pe:
    def __init__(self, path):
        self.raw = Path(path).read_bytes()
        pe = struct.unpack_from("<I", self.raw, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.raw, pe + 6)[0]
        optsize = struct.unpack_from("<H", self.raw, pe + 20)[0]
        opt = pe + 24
        self.image_size = struct.unpack_from("<I", self.raw, opt + 56)[0]
        pdata_rva, pdata_size = struct.unpack_from("<II", self.raw, opt + 112 + 3 * 8)
        self.sections = []
        for i in range(nsec):
            s = opt + optsize + i * 40
            name = self.raw[s:s + 8].rstrip(b"\0").decode(errors="replace")
            vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", self.raw, s + 8)
            self.sections.append((name, vaddr, max(vsize, rsize), rptr, rsize))
        t = next(s for s in self.sections if s[0] == ".text")
        self.text_rva, self.text = t[1], self.raw[t[3]:t[3] + t[4]]
        p = self.rva_to_off(pdata_rva)
        ranges = {}
        for i in range(pdata_size // 12):
            b, e, _ = struct.unpack_from("<III", self.raw, p + i * 12)
            if b and (b not in ranges or e > ranges[b]):
                ranges[b] = e
        self.starts = sorted(ranges)
        self.ends = [ranges[b] for b in self.starts]
        self.start_set = set(self.starts)

    def rva_to_off(self, rva):
        for _, va, size, rp, rs in self.sections:
            if va <= rva < va + rs:
                return rp + rva - va
        return None

    def section_of(self, rva):
        for name, va, size, _, _ in self.sections:
            if va <= rva < va + size:
                return name
        return None

    def func_of(self, rva):
        i = bisect.bisect_right(self.starts, rva) - 1
        if i >= 0 and rva < self.ends[i]:
            return self.starts[i]
        return None

    def func_end(self, start):
        return self.ends[bisect.bisect_left(self.starts, start)]

    def cstring(self, rva, minlen=6):
        off = self.rva_to_off(rva)
        if off is None:
            return None
        end = self.raw.find(b"\0", off, off + 256)
        if end < 0 or end - off < minlen:
            return None
        s = self.raw[off:end]
        if all(32 <= c < 127 for c in s):
            return s
        return None

    def scan(self):
        """Liefert pro Funktion: Liste der Aufrufziele und Liste der RIP-Datenziele (in Reihenfolge)."""
        calls = defaultdict(list)
        data = defaultdict(list)
        text, base = self.text, self.text_rva
        for m in re.finditer(rb"\xE8", text):
            i = m.start()
            if i + 5 > len(text):
                break
            target = base + i + 5 + struct.unpack_from("<i", text, i + 1)[0]
            if target in self.start_set:
                f = self.func_of(base + i)
                if f is not None:
                    calls[f].append((base + i, target))
        # mov/lea/cmp/movss/... mit ModRM mod=00 rm=101 (RIP-relativ), ohne Immediate
        rip = re.compile(rb"[\x40-\x4F]?(?:\x0F[\x10\x11\x28\x29\x2E\x2F\xB6\xB7\xBE\xBF]|[\x8B\x8D\x89\x3B\x39\x03\x2B\x33\x87\x85])([\x05\x0D\x15\x1D\x25\x2D\x35\x3D])", re.DOTALL)
        for m in rip.finditer(text):
            d = m.end()
            if d + 4 > len(text):
                break
            target = base + d + 4 + struct.unpack_from("<i", text, d)[0]
            sec = self.section_of(target)
            if sec in (".data", ".rdata"):
                f = self.func_of(base + m.start())
                if f is not None:
                    data[f].append((base + m.start(), target))
        return calls, data


def vtables(pe):
    """VTables über RTTI: vor jeder VTable steht ein Zeiger auf den CompleteObjectLocator, der auf den
    TypeDescriptor mit dem dekorierten Klassennamen zeigt. Schlüssel: (Name, Offset im Objekt)."""
    base = 0x140000000
    rdata = next(s for s in pe.sections if s[0] == ".rdata")
    off0, size = rdata[3], rdata[4]
    raw = pe.raw
    result = {}
    for i in range(off0, off0 + size - 16, 8):
        col_va = struct.unpack_from("<Q", raw, i)[0]
        col_rva = col_va - base
        if not (rdata[1] <= col_rva < rdata[1] + rdata[2]):
            continue
        co = pe.rva_to_off(col_rva)
        if co is None:
            continue
        sig, offset, _, td_rva, _, self_rva = struct.unpack_from("<IIIIII", raw, co)
        if sig != 1 or self_rva != col_rva:
            continue
        to = pe.rva_to_off(td_rva + 16)
        if to is None:
            continue
        end = raw.find(b"\0", to, to + 512)
        name = raw[to:end]
        if not name.startswith(b".?A"):
            continue
        vt_rva = rdata[1] + (i - off0) + 8
        key = (name, offset)
        result[key] = None if key in result else vt_rva  # doppelt -> unbrauchbar
    return {k: v for k, v in result.items() if v is not None}


ap = argparse.ArgumentParser()
ap.add_argument("--ids", required=True)
ap.add_argument("--old", required=True)
ap.add_argument("--new", required=True)
ap.add_argument("--sig")
ap.add_argument("--anchors", help="CSV old_rva,new_rva mit gesicherten Paaren (hex)")
ap.add_argument("--gaps", type=int, default=0, help="Lücken bis zu dieser Größe nach Reihenfolge füllen (0 = aus)")
ap.add_argument("--vtable-anchors", action="store_true", help="virtuelle Funktionen gleicher VTable-Slots als Stimmen")
ap.add_argument("--rva-col", default="rva_1_10_163", help="Spalte mit der Adresse in der alten exe")
ap.add_argument("--dump-map", help="alle gefundenen Paare als CSV kind,old_rva,new_rva schreiben (für fuzzport.py)")
ap.add_argument("--out", required=True)
a = ap.parse_args()

old, new = Pe(a.old), Pe(a.new)
oc, od = old.scan()
nc, nd = new.scan()
print(f"Funktionen: alt {len(old.starts)}, neu {len(new.starts)}; Aufrufe alt {sum(map(len, oc.values()))}, neu {sum(map(len, nc.values()))}")


def string_refs(pe, data):
    refs = defaultdict(set)
    for f, lst in data.items():
        for _, t in lst:
            s = pe.cstring(t)
            if s:
                refs[s].add(f)
    return refs


osr, nsr = string_refs(old, od), string_refs(new, nd)
func_votes = defaultdict(Counter)   # alte Funktion -> neue Funktion -> Stimmen
for s, fs in osr.items():
    if len(fs) == 1 and len(nsr.get(s, ())) == 1:
        func_votes[next(iter(fs))][next(iter(nsr[s]))] += 3
print(f"String-Anker: {len(func_votes)}")

if a.sig:
    with open(a.sig, encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    with open(a.ids, encoding="utf-8") as fh:
        old_rva = {int(r["real_id"]): int(r[a.rva_col], 16) for r in csv.DictReader(fh)}
    for r in rows:
        o, n = old_rva[int(r["og_id"])], int(r["rva"], 16)
        if o in old.start_set and n in new.start_set:
            func_votes[o][n] += 3


def best(votes):
    if not votes:
        return None
    (n, c), *rest = votes.most_common(2) + [(None, 0)]
    if c >= 2 and c >= 2 * rest[0][1]:
        return n
    return None


def align(a_list, b_list, known):
    """Paart zwei Ziel-Listen. Gleich lang: direkt. Sonst über bereits bekannte Paare als Stützpunkte
    (monoton, gierig) und paart nur Lücken gleicher Länge zwischen zwei Stützpunkten."""
    if not a_list or not b_list:
        return []
    if len(a_list) == len(b_list):
        return list(zip(a_list, b_list))
    pos_b = defaultdict(list)
    for j, t in enumerate(b_list):
        pos_b[t].append(j)
    anchors, last_j = [(-1, -1)], -1
    for i, t in enumerate(a_list):
        n = known.get(t)
        if n is None:
            continue
        nxt = next((j for j in pos_b.get(n, ()) if j > last_j), None)
        if nxt is not None:
            anchors.append((i, nxt))
            last_j = nxt
    anchors.append((len(a_list), len(b_list)))
    pairs = []
    for (i0, j0), (i1, j1) in zip(anchors, anchors[1:]):
        if i1 - i0 == j1 - j0:
            pairs.extend(zip(a_list[i0 + 1:i1], b_list[j0 + 1:j1]))
    return pairs


if a.anchors:
    with open(a.anchors, encoding="utf-8") as fh:
        n_anchor = 0
        for r in csv.DictReader(fh):
            o, n = int(r["old_rva"], 16), int(r["new_rva"], 16)
            if o in old.start_set and n in new.start_set:
                func_votes[o][n] += 10
                n_anchor += 1
    print(f"Gesicherte Anker: {n_anchor}")

def vtable_slots(pe, vt_rva):
    slots = []
    off = pe.rva_to_off(vt_rva)
    while off is not None and len(slots) < 2000:
        target = struct.unpack_from("<Q", pe.raw, off)[0] - 0x140000000
        if target not in pe.start_set:
            break
        slots.append(target)
        off += 8
    return slots


ov, nv = vtables(old), vtables(new)
vt_map = {ov[k]: nv[k] for k in ov.keys() & nv.keys()}
print(f"VTables über RTTI: alt {len(ov)}, neu {len(nv)}, gemeinsam {len(vt_map)}")
if a.vtable_anchors:
    n_slot = 0
    for o_vt, n_vt in vt_map.items():
        so, sn = vtable_slots(old, o_vt), vtable_slots(new, n_vt)
        if so and len(so) == len(sn):
            for fo, fn in zip(so, sn):
                func_votes[fo][fn] += 1
                n_slot += 1
    print(f"VTable-Slots als Stimmen: {n_slot}")

data_votes = defaultdict(Counter)
matched = {}
for round_ in range(12):
    matched = {o: best(v) for o, v in func_votes.items()}
    matched = {o: n for o, n in matched.items() if n is not None}
    # eindeutig halten: eine neue Funktion darf nur einer alten gehören
    owners = Counter(matched.values())
    matched = {o: n for o, n in matched.items() if owners[n] == 1}
    before = len(matched)
    new_votes = defaultdict(Counter)
    data_votes = defaultdict(Counter)
    data_known = {}
    for o, n in matched.items():
        for ta, tb in align([t for _, t in oc.get(o, [])], [t for _, t in nc.get(n, [])], matched):
            new_votes[ta][tb] += 1
    for o, n in matched.items():
        for ta, tb in align([t for _, t in od.get(o, [])], [t for _, t in nd.get(n, [])], data_known):
            data_votes[ta][tb] += 1
    data_known = {t: best(v) for t, v in data_votes.items() if best(v) is not None}
    # zweiter Datendurchlauf mit den gefundenen Daten als Stützpunkte
    data_votes = defaultdict(Counter)
    for o, n in matched.items():
        for ta, tb in align([t for _, t in od.get(o, [])], [t for _, t in nd.get(n, [])], data_known):
            data_votes[ta][tb] += 1
    # Lücken füllen: zwischen zwei benachbarten Zuordnungen mit gleicher Reihenfolge und gleicher
    # Anzahl Funktionen dazwischen werden die Funktionen der Reihe nach gepaart (Linker-Reihenfolge).
    if a.gaps:
        seq = sorted(matched.items())
        for (o0, n0), (o1, n1) in zip(seq, seq[1:]):
            if n1 <= n0:
                continue
            i0, i1 = bisect.bisect_right(old.starts, o0), bisect.bisect_left(old.starts, o1)
            j0, j1 = bisect.bisect_right(new.starts, n0), bisect.bisect_left(new.starts, n1)
            if 0 < i1 - i0 == j1 - j0 <= a.gaps:
                for of, nf in zip(old.starts[i0:i1], new.starts[j0:j1]):
                    new_votes[of][nf] += 1
    for o, v in new_votes.items():
        func_votes[o].update(v)
    print(f"Runde {round_ + 1}: {before} Funktionen zugeordnet")

if a.dump_map:
    with open(a.dump_map, "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["kind", "old_rva", "new_rva"])
        for o, n in matched.items():
            w.writerow(["f", f"{o:X}", f"{n:X}"])
        for o, v in data_votes.items():
            n = best(v)
            if n is not None:
                w.writerow(["d", f"{o:X}", f"{n:X}"])

with open(a.ids, encoding="utf-8") as fh:
    ids = list(csv.DictReader(fh))
seen = set()
found = 0
with open(a.out, "w", newline="", encoding="utf-8") as fh, open(Path(a.out).with_suffix(".report.csv"), "w", newline="", encoding="utf-8") as rh:
    w, rep = csv.writer(fh), csv.writer(rh)
    w.writerow(["og_id", "rva"])
    rep.writerow(["og_id", "name", "old_rva", "new_rva", "art", "stimmen"])
    for r in ids:
        og = int(r["real_id"])
        if og in seen:
            continue
        seen.add(og)
        rva = int(r[a.rva_col], 16)
        res, kind, votes = None, "", 0
        if rva in vt_map:
            res, kind, votes = vt_map[rva], "VTable (RTTI)", 99
        elif rva in matched:
            res, kind, votes = matched[rva], "Funktion", func_votes[rva][matched[rva]]
        elif old.section_of(rva) in (".data", ".rdata"):
            n = best(data_votes.get(rva, Counter()))
            if n is not None:
                res, kind, votes = n, "Daten", data_votes[rva][n]
            else:
                kind = "Daten offen"
        else:
            f = old.func_of(rva)
            if f in matched:
                # Stelle innerhalb einer Funktion: dieselben Bytes rund um die Stelle in der neuen Funktion suchen
                nf = matched[f]
                o_off = old.rva_to_off(rva)
                needle = old.raw[o_off:o_off + 12]
                lo, hi = new.rva_to_off(nf), new.rva_to_off(new.func_end(nf))
                pos = new.raw.find(needle, lo, hi)
                if pos >= 0 and new.raw.find(needle, pos + 1, hi) < 0:
                    res, kind, votes = nf + pos - lo, "in Funktion", func_votes[f][nf]
                else:
                    kind = "in Funktion offen"
            else:
                kind = "Funktion offen"
        if res is not None:
            w.writerow([og, f"{res:X}"])
            found += 1
        rep.writerow([og, r["name"], f"{rva:X}", f"{res:X}" if res is not None else "", kind, votes])
print(f"{found} von {len(seen)} IDs zugeordnet -> {a.out}")

r"""Ordnet die RTTI-IDs aus RTTI.cpp einer neuen Spielversion zu.

Die IDs zeigen auf TypeDescriptor-Einträge. Der Name steht 0x10 Byte dahinter (".?AVTESForm@@") und ändert sich
zwischen den Versionen nicht, also reicht die Suche nach dem Namen in der neuen exe.

python -I rttiport.py --rtti RTTI.cpp --og-bin version-1-10-163-0.bin --og-exe Fallout4.analysis.exe
    --new Fallout4.exe --out rtti-1-11-240-0.csv
"""
import argparse
import csv
import re
import struct

ap = argparse.ArgumentParser()
ap.add_argument("--rtti", required=True)
ap.add_argument("--og-bin", required=True)
ap.add_argument("--og-exe", required=True)
ap.add_argument("--new", required=True)
ap.add_argument("--out", required=True)
a = ap.parse_args()


class Pe:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        d = self.d
        pe = struct.unpack_from("<I", d, 0x3c)[0]
        n = struct.unpack_from("<H", d, pe + 6)[0]
        so = struct.unpack_from("<H", d, pe + 20)[0]
        self.secs = []
        o = pe + 24 + so
        for _ in range(n):
            vs, va, rs, rp = struct.unpack_from("<IIII", d, o + 8)
            self.secs.append((d[o:o + 8].rstrip(b"\0"), va, vs, rs, rp))
            o += 40

    def name_at(self, rva):
        for _, va, _, rs, rp in self.secs:
            if va <= rva < va + rs:
                x = rva - va + rp
                return self.d[x:self.d.index(b"\0", x)]
        return None

    def find_all(self, text):
        """RVAs aller exakten Vorkommen von text + NUL in .data."""
        out = []
        pat = text + b"\0"
        for name, va, _, rs, rp in self.secs:
            if name != b".data":
                continue
            p = self.d.find(pat, rp, rp + rs)
            while p >= 0:
                out.append(p - rp + va)
                p = self.d.find(pat, p + 1, rp + rs)
        return out


raw = open(a.og_bin, "rb").read()
n = struct.unpack_from("<Q", raw, 0)[0]
ids = {}
for i in range(n):
    k, v = struct.unpack_from("<QQ", raw, 8 + i * 16)
    ids[k] = v

old, new = Pe(a.og_exe), Pe(a.new)
src = open(a.rtti, encoding="utf-8").read()
wanted = [int(x) for x in re.findall(r"registerRtti_\w+\((\d+)\);", src)]
found, missing, ambiguous = [], [], []
for og in wanted:
    name = old.name_at(ids[og] + 0x10)
    hits = new.find_all(name)
    if len(hits) == 1:
        found.append((og, hits[0] - 0x10))
    elif hits:
        ambiguous.append((og, name.decode("latin1"), len(hits)))
    else:
        missing.append((og, name.decode("latin1")))

with open(a.out, "w", newline="", encoding="utf-8") as fh:
    w = csv.writer(fh)
    w.writerow(["og_id", "rva"])
    for og, rva in found:
        w.writerow([og, f"0x{rva:X}"])
with open(a.out.replace(".csv", ".missing.csv"), "w", newline="", encoding="utf-8") as fh:
    w = csv.writer(fh)
    w.writerow(["og_id", "name", "treffer"])
    for og, nm in missing:
        w.writerow([og, nm, 0])
    for og, nm, c in ambiguous:
        w.writerow([og, nm, c])
print(f"{len(wanted)} RTTI-IDs: {len(found)} zugeordnet, {len(missing)} nicht gefunden, {len(ambiguous)} mehrdeutig")
print("Beispiele fehlend:", [m[1] for m in missing[:12]])

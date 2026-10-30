r"""Überträgt Funktionsadressen von Fallout4.exe 1.10.163 auf eine andere Version per Byte-Signatur.

Für jede Funktion aus ids-1.10.163.csv wird am Funktionsanfang eine Signatur gebildet. Relative
Sprung-, Aufruf- und RIP-Adressen (rel32, die ins Image zeigen) werden zu Jokern. Gesucht wird nur an
Funktionsanfängen der Zielversion (aus der .pdata-Tabelle). Ein Treffer zählt nur, wenn er eindeutig ist;
sonst wird die Signatur schrittweise verlängert.

Globale Variablen und VTables lassen sich so nicht finden, sie landen im Bericht als "manuell".

Aufruf:
  python -I sigport.py --ids ids-1.10.163.csv --old Fallout4-1.10.163.exe --new Fallout4-1.11.240.exe --out manual-1-11-240-0.csv
  python -I sigport.py --selftest Fallout4.exe      # prüft Signaturen gegen dieselbe exe
"""
import argparse
import bisect
import csv
import random
import re
import struct
from pathlib import Path


class Pe:
    def __init__(self, path):
        self.raw = Path(path).read_bytes()
        pe = struct.unpack_from("<I", self.raw, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.raw, pe + 6)[0]
        optsize = struct.unpack_from("<H", self.raw, pe + 20)[0]
        opt = pe + 24
        self.image_size = struct.unpack_from("<I", self.raw, opt + 56)[0]
        pdata_rva, pdata_size = struct.unpack_from("<II", self.raw, opt + 112 + 3 * 8)  # Exception-Verzeichnis
        self.sections = []
        for i in range(nsec):
            s = opt + optsize + i * 40
            name = self.raw[s:s + 8].rstrip(b"\0").decode(errors="replace")
            vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", self.raw, s + 8)
            self.sections.append((name, vaddr, max(vsize, rsize), rptr, rsize))
        text = next(s for s in self.sections if s[0] == ".text")
        self.text_rva, self.text = text[1], self.raw[text[3]:text[3] + text[4]]
        self.funcs = sorted({struct.unpack_from("<I", self.raw, self.rva_to_off(pdata_rva) + i * 12)[0]
                             for i in range(pdata_size // 12)})
        self.func_set = set(self.funcs)

    def rva_to_off(self, rva):
        for _, vaddr, vsize, rptr, rsize in self.sections:
            if vaddr <= rva < vaddr + rsize:
                return rptr + rva - vaddr
        return None

    def is_code(self, rva):
        return self.text_rva <= rva < self.text_rva + len(self.text)


def signature(pe, rva, length):
    off = rva - pe.text_rva
    data = pe.text[off:off + length]
    mask = [True] * len(data)
    i = 0
    while i + 4 <= len(data):
        val = struct.unpack_from("<i", data, i)[0]
        target = rva + i + 4 + val
        if abs(val) >= 0x100 and 0 <= target < pe.image_size:
            for k in range(i, i + 4):
                mask[k] = False
            i += 4
        else:
            i += 1
    if sum(mask) < length // 2:
        return None
    return re.compile(b"".join(re.escape(bytes([b])) if m else b"." for b, m in zip(data, mask)), re.DOTALL)


def find(pe, sig):
    hits = []
    for m in sig.finditer(pe.text):
        rva = pe.text_rva + m.start()
        if rva in pe.func_set:
            hits.append(rva)
            if len(hits) > 1:
                break
    return hits


def match(old, new, rva):
    for length in (32, 64, 128, 256):
        sig = signature(old, rva, length)
        if sig is None:
            continue
        hits = find(new, sig)
        if len(hits) == 1:
            return hits[0], length
        if not hits:
            return None, length
    return None, -1


ap = argparse.ArgumentParser()
ap.add_argument("--ids")
ap.add_argument("--old")
ap.add_argument("--new")
ap.add_argument("--out")
ap.add_argument("--selftest")
ap.add_argument("--rva-col", default="rva_1_10_163")
a = ap.parse_args()

if a.selftest:
    pe = Pe(a.selftest)
    sample = random.Random(1).sample(pe.funcs, 300)
    ok = sum(1 for r in sample if match(pe, pe, r)[0] == r)
    print(f"{len(pe.funcs)} Funktionen in .pdata; Selbsttest: {ok} von {len(sample)} eindeutig wiedergefunden")
    raise SystemExit(0)

old, new = Pe(a.old), Pe(a.new)
with open(a.ids, encoding="utf-8") as fh:
    rows = {int(r["real_id"]): r for r in csv.DictReader(fh)}

found = manual = 0
with open(a.out, "w", newline="", encoding="utf-8") as fh, open(Path(a.out).with_suffix(".report.csv"), "w", newline="", encoding="utf-8") as rh:
    w, rep = csv.writer(fh), csv.writer(rh)
    w.writerow(["og_id", "rva"])
    rep.writerow(["og_id", "name", "old_rva", "new_rva", "status"])
    for og, r in sorted(rows.items()):
        rva = int(r[a.rva_col], 16)
        if rva not in old.func_set:
            # Mitten in einer Funktion (Hook-Stelle) oder Daten: nur Funktionsanfänge lassen sich direkt suchen
            start = old.funcs[bisect.bisect_right(old.funcs, rva) - 1] if old.is_code(rva) else None
            if start is not None:
                hit, _ = match(old, new, start)
                if hit is not None:
                    w.writerow([og, f"{hit + rva - start:X}"])
                    rep.writerow([og, r["name"], f"{rva:X}", f"{hit + rva - start:X}", f"Funktion+{rva - start:X}, Abstand ungeprüft"])
                    found += 1
                    continue
            rep.writerow([og, r["name"], f"{rva:X}", "", "manuell (Daten/VTable)" if start is None else "manuell (in Funktion)"])
            manual += 1
            continue
        hit, length = match(old, new, rva)
        if hit is None:
            rep.writerow([og, r["name"], f"{rva:X}", "", "kein eindeutiger Treffer"])
            manual += 1
        else:
            w.writerow([og, f"{hit:X}"])
            rep.writerow([og, r["name"], f"{rva:X}", f"{hit:X}", f"Signatur {length} Byte"])
            found += 1
print(f"{found} gefunden, {manual} offen -> {a.out}")

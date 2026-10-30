r"""Prüft graphport.py gegen bekannte Wahrheit: CommonLibF4-Paare (OG-ID, NG-ID) + NG-Address-Library.

1. Erzeugt aus allen RelocationID(og, ng)-Paaren eine ID-Liste mit 1.10.163-Adressen (ids-validate.csv).
2. Ruft graphport.py für 1.10.163 -> Ziel-exe auf.
3. Vergleicht das Ergebnis mit der Address Library der Zielversion.

Aufruf:
  python -I validate.py --clib <CommonLibF4> --og-bin version-1-10-163-0.bin --ng-bin version-1-11-191-0.bin
      --old Fallout4.analysis.exe --new Fallout4-1.11.191.exe
"""
import argparse
import csv
import re
import struct
import subprocess
import sys
from pathlib import Path

here = Path(__file__).parent
ap = argparse.ArgumentParser()
ap.add_argument("--clib", required=True)
ap.add_argument("--og-bin", required=True)
ap.add_argument("--ng-bin", required=True)
ap.add_argument("--old", required=True)
ap.add_argument("--new", required=True)
a = ap.parse_args()


def read_addrlib(path):
    raw = Path(path).read_bytes()
    (count,) = struct.unpack_from("<Q", raw, 0)
    return dict(struct.unpack_from("<QQ", raw, 8 + i * 16) for i in range(count))


og, ng = read_addrlib(a.og_bin), read_addrlib(a.ng_bin)
pairs = {}
for f in Path(a.clib).rglob("*"):
    if f.suffix in (".h", ".hpp", ".cpp", ".inl"):
        for o, n in re.findall(r"RelocationID\s*[\(\{]\s*(\d+)\s*,\s*(\d+)", f.read_text(encoding="utf-8", errors="replace")):
            pairs[int(o)] = int(n)

work = here / "out"
work.mkdir(exist_ok=True)
ids = work / "ids-validate.csv"
truth = {}
with ids.open("w", newline="", encoding="utf-8") as fh:
    w = csv.writer(fh)
    w.writerow(["id", "name", "type", "real_id", "rva_1_10_163", "file", "line"])
    for o, n in sorted(pairs.items()):
        if o in og and n in ng:
            w.writerow([o, f"clf4_{o}", "", o, f"0x{og[o]:X}", "", 0])
            truth[o] = ng[n]
print(f"{len(truth)} Paare mit Adresse in beiden Versionen")

out = work / "validate-result.csv"
subprocess.run([sys.executable, "-I", str(here / "graphport.py"), "--ids", str(ids), "--old", a.old, "--new", a.new, "--out", str(out)], check=True)

ok = wrong = 0
wrong_list = []
with out.open(encoding="utf-8") as fh:
    for r in csv.DictReader(fh):
        o, rva = int(r["og_id"]), int(r["rva"], 16)
        if truth.get(o) == rva:
            ok += 1
        else:
            wrong += 1
            wrong_list.append((o, rva, truth.get(o)))
total = len(truth)
print(f"Richtig: {ok}, falsch: {wrong}, nicht zugeordnet: {total - ok - wrong} (von {total})")
print(f"Trefferquote {ok / total:.1%}, Fehlerquote unter den Zuordnungen {wrong / max(1, ok + wrong):.1%}")
for o, rva, t in wrong_list[:15]:
    print(f"  falsch: OG {o}: {rva:X} statt {t:X}" if t is not None else f"  OG {o}: {rva:X} (keine Wahrheit)")

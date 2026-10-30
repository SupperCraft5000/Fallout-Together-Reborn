r"""Baut die Zuordnung Old-Gen-ID (1.10.163) -> Next-Gen-ID aus CommonLibF4 (RelocationID(og, ng)).

Aufruf: python -I build_map.py <CommonLibF4-Checkout> <ids-1.10.163.csv> <og_to_ng.csv>
"""
import csv
import re
import sys
from pathlib import Path

clib, ids_csv, out = map(Path, sys.argv[1:4])

pair = re.compile(r"RelocationID\s*[\(\{]\s*(\d+)\s*,\s*(\d+)")
mapping = {}
conflicts = set()
for f in clib.rglob("*"):
    if f.suffix not in (".h", ".hpp", ".cpp", ".inl"):
        continue
    for og, ng in pair.findall(f.read_text(encoding="utf-8", errors="replace")):
        og, ng = int(og), int(ng)
        if og in mapping and mapping[og] != ng:
            conflicts.add(og)
        mapping[og] = ng

with ids_csv.open(encoding="utf-8") as fh:
    rows = list(csv.DictReader(fh))
used = {}  # echte OG-ID -> Name
mod_id = {}
for r in rows:
    used.setdefault(int(r["real_id"]), r["name"])
    mod_id[int(r["real_id"])] = int(r["id"])

with out.open("w", newline="", encoding="utf-8") as fh:
    w = csv.writer(fh)
    w.writerow(["mod_id", "og_id", "ng_id", "name", "source"])
    for og in sorted(used):
        ng = mapping.get(og)
        src = "CommonLibF4" + (" (Konflikt)" if og in conflicts else "") if ng else ""
        w.writerow([mod_id[og], og, ng or "", used[og], src])

found = sum(1 for og in used if og in mapping)
print(f"{len(mapping)} Paare in CommonLibF4, {len(conflicts)} Konflikte")
print(f"Abgedeckt: {found} von {len(used)} IDs des Mods")

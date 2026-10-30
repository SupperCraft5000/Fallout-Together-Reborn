r"""Erzeugt Data\F4SE\Plugins\FalloutTogether\offsets-<version>.bin für eine Next-Gen-Version.

Die Datei hat das Layout der Old-Gen-Address-Library (u64 Anzahl, dann Paare u64 ID / u64 Offset),
ist aber nach Old-Gen-IDs (1.10.163) geschlüsselt, so wie sie im Code stehen.

Quellen, in dieser Reihenfolge (spätere überschreiben frühere):
  --ng-bin   Next-Gen-Address-Library dieser Version + og_to_ng.csv (OG-ID -> NG-ID)
  --manual   CSV mit og_id,rva (z. B. aus Signatur-Suche oder Handarbeit), rva hexadezimal

Aufruf:
  python -I make_offsets.py --ids ids-1.10.163.csv --map og_to_ng.csv --ng-bin version-1-11-191-0.bin --out offsets-1-11-191-0.bin
  python -I make_offsets.py --ids ids-1.10.163.csv --manual manual-1-11-240-0.csv --out offsets-1-11-240-0.bin
"""
import argparse
import csv
import struct
from pathlib import Path


def read_addrlib(path):
    raw = Path(path).read_bytes()
    (count,) = struct.unpack_from("<Q", raw, 0)
    return dict(struct.unpack_from("<QQ", raw, 8 + i * 16) for i in range(count))


ap = argparse.ArgumentParser()
ap.add_argument("--ids", required=True)
ap.add_argument("--map")
ap.add_argument("--ng-bin")
ap.add_argument("--manual", action="append", default=[])
ap.add_argument("--out", required=True)
a = ap.parse_args()

with open(a.ids, encoding="utf-8") as fh:
    needed = {int(r["real_id"]): r["name"] for r in csv.DictReader(fh)}

offsets, source = {}, {}
if a.ng_bin:
    ng = read_addrlib(a.ng_bin)
    with open(a.map, encoding="utf-8") as fh:
        for r in csv.DictReader(fh):
            if r["ng_id"] and int(r["ng_id"]) in ng:
                offsets[int(r["og_id"])] = ng[int(r["ng_id"])]
                source[int(r["og_id"])] = "ng-bin"
for m in a.manual:
    with open(m, encoding="utf-8") as fh:
        for r in csv.DictReader(fh):
            offsets[int(r["og_id"])] = int(r["rva"], 16)
            source[int(r["og_id"])] = Path(m).name

out = Path(a.out)
out.parent.mkdir(parents=True, exist_ok=True)
with out.open("wb") as fh:
    fh.write(struct.pack("<Q", len(offsets)))
    for og in sorted(offsets):
        fh.write(struct.pack("<QQ", og, offsets[og]))

missing = sorted(set(needed) - set(offsets))
print(f"{len(set(needed) & set(offsets))} von {len(needed)} IDs aufgelöst, {len(missing)} fehlen -> {out}")
report = out.with_suffix(".missing.csv")
with report.open("w", newline="", encoding="utf-8") as fh:
    w = csv.writer(fh)
    w.writerow(["og_id", "name"])
    for og in missing:
        w.writerow([og, needed[og]])

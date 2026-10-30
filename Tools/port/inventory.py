r"""Listet alle Address-Library-IDs, die Fallout Together nutzt, mit Adresse in 1.10.163.

Aufruf: python -I inventory.py <TiltedEvolution\Code> <version-1-10-163-0.bin> <ausgabe.csv>
"""
import csv
import re
import struct
import sys
from pathlib import Path

code, binfile, out = map(Path, sys.argv[1:4])

# Echtes Format der Old-Gen-Address-Library: u64 Anzahl, dann Paare (ID, Adresse).
# VersionDb::LoadFallout4 liest ab Byte 0 Paare (Adresse, ID) und ist damit um einen Eintrag verschoben:
# Mod-ID X bekommt die Adresse des Eintrags vor X. Die echte ID ist also die des Vorgängers.
raw = binfile.read_bytes()
(count,) = struct.unpack_from("<Q", raw, 0)
entries = [struct.unpack_from("<QQ", raw, 8 + i * 16) for i in range(count)]
real = {}  # Mod-ID -> (echte ID, Adresse)
prev = None
for id_, addr in entries:
    if prev is not None:
        real[id_] = prev
    prev = (id_, addr)
ids = {k: v[1] for k, v in real.items()}

patterns = [
    re.compile(r"POINTER_FALLOUT4\(\s*(?P<type>[^,]+?)\s*,\s*(?P<name>\w+)\s*,\s*(?P<id>\d+)\s*\)"),
    re.compile(r"VersionDbPtr<(?P<type>[^;]*?)>\s*(?P<name>[\w:]+)\s*\(\s*(?P<id>\d+)\s*\)"),
    re.compile(r"DECLARE_DISPATCHER\(\s*(?P<name>\w+)\s*,\s*(?P<id>\d+)\s*\)"),
]
rows = []
for f in sorted(code.rglob("*")):
    if f.suffix not in (".cpp", ".h", ".hpp", ".inl"):
        continue
    for n, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if "#define" in line:
            continue
        for p in patterns:
            for m in p.finditer(line):
                id_ = int(m["id"])
                addr = ids.get(id_)
                rows.append({
                    "id": id_,
                    "name": m["name"],
                    "type": m["type"].strip(),
                    "real_id": real[id_][0] if id_ in real else "FEHLT",
                "rva_1_10_163": f"0x{addr:X}" if addr is not None else "FEHLT",
                    "file": f.relative_to(code).as_posix(),
                    "line": n,
                })

rows.sort(key=lambda r: (r["file"], r["line"]))
with out.open("w", newline="", encoding="utf-8") as fh:
    w = csv.DictWriter(fh, fieldnames=list(rows[0]))
    w.writeheader()
    w.writerows(rows)
missing = sum(r["rva_1_10_163"] == "FEHLT" for r in rows)
print(f"{len(rows)} Verwendungen, {len({r['id'] for r in rows})} IDs, {missing} ohne Adresse, {count} Einträge in der Datenbank")
print(f"Kleinste/größte Adresse: 0x{min(ids.values()):X} / 0x{max(ids.values()):X}")

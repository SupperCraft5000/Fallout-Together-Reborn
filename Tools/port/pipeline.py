r"""Komplette Adress-Portierung für eine neue Fallout-4-Version in einem Aufruf.

Weg A: 1.10.163 -> Ziel direkt (Signaturen + Aufrufgraph + RTTI).
Weg B (optional): 1.10.163 -> Zwischenversion (mit Address Library + CommonLibF4-Ankern) -> Ziel.
Übernommen wird, was nur ein Weg findet oder worin beide Wege übereinstimmen. Abweichungen werden verworfen
und im Bericht aufgeführt.

Aufruf:
  python -I pipeline.py --ids ids-1.10.163.csv --og-exe Fallout4-1.10.163.analysis.exe --new Fallout4.exe
      [--mid-exe Fallout4-1.11.191.exe --mid-anchors anchors-163-191-func.csv]
      --version 1-11-240-0 --install "<Spielordner>"
"""
import argparse
import csv
import subprocess
import sys
from pathlib import Path

here = Path(__file__).parent
ap = argparse.ArgumentParser()
ap.add_argument("--ids", required=True)
ap.add_argument("--og-exe", required=True)
ap.add_argument("--new", required=True)
ap.add_argument("--mid-exe")
ap.add_argument("--mid-anchors")
ap.add_argument("--version", required=True, help="z. B. 1-11-240-0")
ap.add_argument("--work", default=str(here / "out"))
ap.add_argument("--install", help="Spielordner; schreibt Data\\F4SE\\Plugins\\FalloutTogether\\offsets-<version>.bin")
a = ap.parse_args()

work = Path(a.work)
work.mkdir(parents=True, exist_ok=True)
py = [sys.executable, "-I"]


def run(*args):
    print(">", " ".join(str(x) for x in args[2:4]), "...")
    subprocess.run([*py, *map(str, args)], check=True)


def load(path):
    with open(path, encoding="utf-8") as fh:
        return {r["og_id"]: r["rva"] for r in csv.DictReader(fh)}


v = a.version
run(here / "sigport.py", "--ids", a.ids, "--old", a.og_exe, "--new", a.new, "--out", work / f"sig-{v}.csv")
run(here / "graphport.py", "--ids", a.ids, "--old", a.og_exe, "--new", a.new, "--sig", work / f"sig-{v}.csv",
    "--gaps", "4", "--out", work / f"A-{v}.csv")
A = load(work / f"A-{v}.csv")
B = {}
if a.mid_exe:
    run(here / "graphport.py", "--ids", a.ids, "--old", a.og_exe, "--new", a.mid_exe, "--gaps", "4",
        *(["--anchors", a.mid_anchors] if a.mid_anchors else []), "--out", work / "B-mid.csv")
    mid = load(work / "B-mid.csv")
    with open(a.ids, encoding="utf-8") as fh:
        names = {r["real_id"]: r["name"] for r in csv.DictReader(fh)}
    with open(work / "ids-mid.csv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["id", "name", "real_id", "rva_old"])
        for k, rva in mid.items():
            w.writerow([k, names.get(k, ""), k, "0x" + rva])
    run(here / "sigport.py", "--ids", work / "ids-mid.csv", "--rva-col", "rva_old", "--old", a.mid_exe, "--new", a.new,
        "--out", work / f"Bsig-{v}.csv")
    run(here / "graphport.py", "--ids", work / "ids-mid.csv", "--rva-col", "rva_old", "--old", a.mid_exe, "--new", a.new,
        "--sig", work / f"Bsig-{v}.csv", "--gaps", "4", "--out", work / f"B-{v}.csv")
    B = load(work / f"B-{v}.csv")

conflicts = sorted(k for k in A.keys() & B.keys() if A[k] != B[k])
final = {k: r for k, r in {**A, **B}.items() if k not in conflicts}
out = here / f"offsets-{v}.csv"
with open(out, "w", newline="", encoding="utf-8") as fh:
    w = csv.writer(fh)
    w.writerow(["og_id", "rva"])
    for k in sorted(final, key=int):
        w.writerow([k, final[k]])
print(f"Weg A {len(A)}, Weg B {len(B)}, übereinstimmend {len(A.keys() & B.keys()) - len(conflicts)}, "
      f"Konflikte {len(conflicts)} (verworfen: {', '.join(conflicts) or '-'}), Ergebnis {len(final)} -> {out}")

if a.install:
    target = Path(a.install) / "Data" / "F4SE" / "Plugins" / "FalloutTogether" / f"offsets-{v}.bin"
    run(here / "make_offsets.py", "--ids", a.ids, "--manual", out, "--out", target)

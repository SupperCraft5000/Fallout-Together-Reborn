"""Packt die Laufzeitdateien und Adressdateien für die Einzel-exe.

python -I make-payload.py <bin-ordner> <dateiliste> <ordner mit offsets-*.bin> <ziel.zip>
"""
import sys
import zipfile
from pathlib import Path

bin_dir, file_list, offsets_dir, target = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3]), sys.argv[4]

with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for name in file_list.read_text(encoding="utf-8-sig").split():
        path = bin_dir / name
        for f in sorted(path.rglob("*")) if path.is_dir() else [path]:
            if f.is_file():
                z.write(f, f.relative_to(bin_dir).as_posix())
    for f in offsets_dir.glob("offsets-*.bin"):
        z.write(f, f.name)

r"""Liest die Ghidra-Dumps (DumpAll.java) und bietet Abfragen: Listing, Aufrufer, Aufgerufene, Daten, Strings,
Merkmale. Nur Standardbibliothek. Paare aus out/map-240-plus.csv und offsets-1-11-240-0.csv werden mitgeladen.
"""
import bisect
import csv
import importlib.util
import pickle
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

WORK = Path(__import__("os").environ["FTR_WORK"])
PORT = Path(__file__).resolve().parents[3] / "data"
DUMP = WORK / "ghidra" / "dump"
OLD_EXE = str(WORK / "exe-1.10.163" / "Fallout4.analysis.exe")
NEW_EXE = str(WORK / "exe-1.11.240" / "Fallout4.exe")


class Pe:
    def __init__(self, path):
        self.raw = Path(path).read_bytes()
        pe = struct.unpack_from("<I", self.raw, 0x3C)[0]
        n = struct.unpack_from("<H", self.raw, pe + 6)[0]
        o = struct.unpack_from("<H", self.raw, pe + 20)[0]
        self.sec = []
        for i in range(n):
            s = pe + 24 + o + i * 40
            name = self.raw[s:s + 8].rstrip(b"\0").decode()
            vs, va, rs, rp = struct.unpack_from("<IIII", self.raw, s + 8)
            self.sec.append((name, va, max(vs, rs), rp, rs))

    def off(self, rva):
        for _, va, _, rp, rs in self.sec:
            if va <= rva < va + rs:
                return rp + rva - va
        return None

    def section(self, rva):
        for name, va, size, _, _ in self.sec:
            if va <= rva < va + size:
                return name
        return None

    def bytes(self, rva, n):
        o = self.off(rva)
        return b"" if o is None else self.raw[o:o + n]

    def cstr(self, rva):
        o = self.off(rva)
        if o is None:
            return None
        e = self.raw.find(b"\0", o, o + 300)
        if e - o < 3:
            return None
        s = self.raw[o:e]
        if all(32 <= c < 127 for c in s):
            return s.decode()
        return None

    def wstr(self, rva):
        o = self.off(rva)
        if o is None:
            return None
        out = []
        while len(out) < 200:
            c = struct.unpack_from("<H", self.raw, o)[0]
            if c == 0:
                break
            if not 32 <= c < 127:
                return None
            out.append(chr(c))
            o += 2
        return "".join(out) if len(out) >= 3 else None


HEAD = bytes([94]) + b"F ([0-9A-F]+) ([0-9A-F]+)[" + bytes([13]) + b"]?" + bytes([10])
ABS = re.compile(r"0x(14[0-9a-f]{7})(?![0-9a-f])")
INS = re.compile(r"^([0-9A-F]+) (\S+(?: \S+)??)(?: (.*?))?((?: ;[0-9A-F]+)*)$")


class Dump:
    def __init__(self, which):
        self.which = which
        self.pe = Pe(OLD_EXE if which == "old" else NEW_EXE)
        self.blobs = {}
        self.ends = {}
        data = (DUMP / f"{which}.txt").read_bytes()
        pos = 0
        parts = data.split(b"F ")  # schnell, wird unten korrigiert
        # robust: zeilenweise Kopf suchen
        self.blobs = {}
        for m in re.finditer(rb"^F ([0-9A-F]+) ([0-9A-F]+)\r?\n", data, re.M):
            pass
        heads = [(m.start(), m.end(), int(m[1], 16), int(m[2], 16)) for m in re.finditer(rb"^F ([0-9A-F]+) ([0-9A-F]+)\r?\n", data, re.M)]
        for i, (s, e, b, en) in enumerate(heads):
            nxt = heads[i + 1][0] if i + 1 < len(heads) else len(data)
            self.blobs[b] = data[e:nxt]
            self.ends[b] = en
        self.starts = sorted(self.blobs)
        extra = DUMP / f"{which}-extra.txt"
        if extra.exists():
            d2 = extra.read_bytes()
            hs = [(m.start(), m.end(), int(m[1], 16), int(m[2], 16)) for m in re.finditer(HEAD, d2, re.M)]
            for i, (s, e, b, en) in enumerate(hs):
                if b not in self.blobs:
                    self.blobs[b] = d2[e:hs[i + 1][0] if i + 1 < len(hs) else len(d2)]
                    self.ends[b] = en
        self.chunks()
        self.starts = sorted(self.ends)
        self.leaf = None
        self.callees = defaultdict(list)  # f -> [(site, target)]
        self.callers = defaultdict(list)  # target -> [(site, f)]
        self.refs = defaultdict(list)     # f -> [(site, target, text)] Nicht-Aufruf-Referenzen
        self.refd = defaultdict(list)     # target -> [(site, f)]
        for c, blob in self.blobs.items():
            f = self.root.get(c, c)
            own = self.parts.get(f, [(f, self.ends[f])])
            for ln in blob.decode("latin1").splitlines():
                t = ln.split(" ;")
                if len(t) == 1 and "0x14" not in ln:
                    continue
                head = t[0]
                sp = head.index(" ")
                site = int(head[:sp], 16)
                body = head[sp + 1:]
                for r in t[1:]:
                    tg = self.root.get(int(r, 16), int(r, 16))
                    if body.startswith("CALL"):
                        self.callees[f].append((site, tg))
                        self.callers[tg].append((site, f))
                    elif body[0] == "J":
                        if not any(a <= int(r, 16) < e for a, e in own):
                            self.callees[f].append((site, tg))
                            self.callers[tg].append((site, f))
                    else:
                        self.refs[f].append((site, tg, body))
                        self.refd[tg].append((site, f))
                if "0x14" in head:
                    tg0 = {int(r, 16) for r in t[1:]}
                    for m in ABS.finditer(head):
                        tv = int(m[1], 16) - 0x140000000
                        if tv not in tg0:
                            tg = self.root.get(tv, tv)
                            self.refs[f].append((site, tg, body))
                            self.refd[tg].append((site, f))
        for k in self.callees: self.callees[k].sort()
        for k in self.refs: self.refs[k].sort()
        # Eingehende Zeiger aus Daten: VTables usw.
        self.ptrs = None

    def chunks(self):
        """Verkettete .pdata-Einträge (UNW_FLAG_CHAININFO) auf die Stammfunktion zurückführen."""
        pe = self.pe
        hdr = struct.unpack_from("<I", pe.raw, 0x3C)[0]
        rva, size = struct.unpack_from("<II", pe.raw, hdr + 24 + 112 + 3 * 8)
        off = pe.off(rva)
        ent = {}
        for i in range(size // 12):
            b, e, u = struct.unpack_from("<III", pe.raw, off + i * 12)
            if b:
                ent[b] = (e, u)
        self.root = {}
        self.parts = defaultdict(list)
        for b, (e, u) in ent.items():
            r = b
            for _ in range(32):
                uo = pe.off(u & ~1)
                fl = pe.raw[uo] >> 3
                if not fl & 4:
                    break
                n = pe.raw[uo + 2]
                p = uo + 4 + ((n + 1) & ~1) * 2
                r, _e, u = struct.unpack_from("<III", pe.raw, p)
            self.root[b] = r
            self.parts[r].append((b, e))
        self.parts = dict(self.parts)
        for k in self.parts:
            self.parts[k].sort()

    def func_of(self, rva):
        i = bisect.bisect_right(self.starts, rva) - 1
        if i >= 0 and rva < self.ends[self.starts[i]]:
            s = self.starts[i]
            return self.root.get(s, s)
        return None

    def listing(self, f):
        out = []
        for b, e in self.parts.get(f, [(f, 0)]):
            if b in self.blobs:
                out += self.blobs[b].decode("latin1").splitlines()
        return out

    def size(self, f):
        return sum(e - b for b, e in self.parts[f]) if f in self.parts else self.ends.get(f, f) - f

    def dataptrs(self):
        """Alle 8-Byte-Zeiger in .rdata/.data auf Funktionsanfänge: target -> [Adresse]."""
        if self.ptrs is not None:
            return self.ptrs
        self.ptrs = defaultdict(list)
        ss = set(self.starts)
        for name, va, size, rp, rs in self.pe.sec:
            if name not in (".rdata", ".data"):
                continue
            for i in range(0, rs - 8, 8):
                v = struct.unpack_from("<Q", self.pe.raw, rp + i)[0] - 0x140000000
                if v in ss:
                    self.ptrs[v].append(va + i)
        return self.ptrs

    def describe(self, t):
        s = self.pe.cstr(t)
        if s:
            return repr(s)
        s = self.pe.wstr(t)
        if s:
            return "L" + repr(s)
        return None


def load_map():
    f, d = {}, {}
    with open(PORT / "out" / "map-240-plus.csv", encoding="utf-8") as fh:
        for r in csv.DictReader(fh):
            (f if r["kind"] == "f" else d)[int(r["old_rva"], 16)] = int(r["new_rva"], 16)
    return f, d


def cached(which):
    return Dump(which)

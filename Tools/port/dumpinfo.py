"""Liest aus einem Windows-Minidump Ausnahme, Register und Stack-Kandidaten des fehlerhaften Threads.

python -I dumpinfo.py <dmp> <exe mit pdb> [Anzahl Stack-Treffer]
Stack-Treffer sind Werte im Stack, die in einem Modul liegen: keine echte Unwinding-Kette, aber meist genug.
"""
import struct
import subprocess
import sys
from pathlib import Path


def main():
    path, exe = sys.argv[1], sys.argv[2]
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    f = open(path, "rb")

    def rd(off, n):
        f.seek(off)
        return f.read(n)

    sig, ver, nstreams, dir_rva = struct.unpack("<IIII", rd(0, 16))
    streams = {}
    for i in range(nstreams):
        t, size, rva = struct.unpack("<III", rd(dir_rva + i * 12, 12))
        streams[t] = (size, rva)

    # Module
    mods = []
    size, rva = streams[4]
    n = struct.unpack("<I", rd(rva, 4))[0]
    for i in range(n):
        e = rd(rva + 4 + i * 108, 108)
        base, msize = struct.unpack_from("<QI", e, 0)
        name_rva = struct.unpack_from("<I", e, 20)[0]
        ln = struct.unpack("<I", rd(name_rva, 4))[0]
        mods.append((base, msize, rd(name_rva + 4, ln).decode("utf-16-le").split("\\")[-1]))
    mods.sort()

    def mod(a):
        for b, s, nm in mods:
            if b <= a < b + s:
                return nm, a - b
        return None

    # Speicher
    ranges = []
    if 9 in streams:
        size, rva = streams[9]
        cnt, base_rva = struct.unpack("<QQ", rd(rva, 16))
        pos = base_rva
        for i in range(cnt):
            st, sz = struct.unpack("<QQ", rd(rva + 16 + i * 16, 16))
            ranges.append((st, sz, pos))
            pos += sz
    if 5 in streams:
        size, rva = streams[5]
        cnt = struct.unpack("<I", rd(rva, 4))[0]
        for i in range(cnt):
            st, sz, loc = struct.unpack("<QII", rd(rva + 4 + i * 16, 16))
            ranges.append((st, sz, loc))

    def mem(a, n):
        for st, sz, p in ranges:
            if st <= a < st + sz:
                return rd(p + a - st, min(n, st + sz - a))
        return b""

    # Ausnahme
    size, rva = streams[6]
    e = rd(rva, size)
    tid = struct.unpack_from("<I", e, 0)[0]
    code, flags, rec, addr, np = struct.unpack_from("<IIQQI", e, 8)
    info = struct.unpack_from("<QQ", e, 8 + 32)
    ctx_size, ctx_rva = struct.unpack_from("<II", e, size - 8)
    print(f"Thread {tid:#x}, Code {code:#x}, Adresse {addr:#x} {mod(addr)}, Parameter {info[0]:#x} {info[1]:#x}")
    c = rd(ctx_rva, ctx_size)
    regs = dict(zip("rax rcx rdx rbx rsp rbp rsi rdi r8 r9 r10 r11 r12 r13 r14 r15".split(),
                    struct.unpack_from("<16Q", c, 0x78)))
    rip = struct.unpack_from("<Q", c, 0xf8)[0]
    print("rip", hex(rip), mod(rip))
    print(" ".join(f"{k}={v:#x}" for k, v in regs.items()))
    print("Code an rip:", mem(rip, 16).hex())

    hits = []
    data = mem(regs["rsp"], 0x4000)
    for i in range(0, len(data) - 8, 8):
        v = struct.unpack_from("<Q", data, i)[0]
        m = mod(v)
        if m:
            hits.append((regs["rsp"] + i, v, m))
    hits = hits[:limit]
    own = Path(exe).name
    offs = [f"{h[2][1]:x}" for h in hits if h[2][0].lower() == own.lower()]
    syms = {}
    if offs:
        r = subprocess.run([sys.executable, "-I", str(Path(__file__).with_name("addr2sym.py")), exe] + offs,
                           capture_output=True, text=True)
        for line in r.stdout.splitlines():
            k, _, v = line.partition(": ")
            syms[int(k, 16)] = v
    for sp, v, (nm, o) in hits:
        print(f"{sp:#x}: {nm}+{o:#x} {syms.get(o, '') if nm.lower() == own.lower() else ''}")


if __name__ == "__main__":
    main()

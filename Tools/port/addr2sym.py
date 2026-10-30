"""Bildet Offsets einer exe/dll mit PDB auf Symbol und Quellzeile ab (dbghelp).

python -I addr2sym.py <exe> <offset> [<offset> ...]    # Offsets hexadezimal, relativ zur Modulbasis
"""
import ctypes as C
import struct
import sys

SYMOPT_LOAD_LINES = 0x10
SYMOPT_UNDNAME = 0x2


class SymbolInfo(C.Structure):
    _fields_ = [("SizeOfStruct", C.c_ulong), ("TypeIndex", C.c_ulong), ("Reserved", C.c_ulonglong * 2),
                ("Index", C.c_ulong), ("Size", C.c_ulong), ("ModBase", C.c_ulonglong), ("Flags", C.c_ulong),
                ("Value", C.c_ulonglong), ("Address", C.c_ulonglong), ("Register", C.c_ulong),
                ("Scope", C.c_ulong), ("Tag", C.c_ulong), ("NameLen", C.c_ulong), ("MaxNameLen", C.c_ulong),
                ("Name", C.c_char * 512)]


class Line64(C.Structure):
    _fields_ = [("SizeOfStruct", C.c_ulong), ("Key", C.c_void_p), ("LineNumber", C.c_ulong),
                ("FileName", C.c_char_p), ("Address", C.c_ulonglong)]


def image_base(path):
    d = open(path, "rb").read(4096)
    pe = struct.unpack_from("<I", d, 0x3c)[0]
    return struct.unpack_from("<Q", d, pe + 24 + 24)[0]


def main():
    exe, offs = sys.argv[1], [int(x, 16) for x in sys.argv[2:]]
    h = C.windll.kernel32.GetCurrentProcess()
    h = C.c_void_p(h)
    dbg = C.windll.dbghelp
    dbg.SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME)
    dbg.SymInitialize(h, None, False)
    dbg.SymLoadModuleEx.restype = C.c_ulonglong
    dbg.SymLoadModuleEx.argtypes = [C.c_void_p, C.c_void_p, C.c_char_p, C.c_char_p, C.c_ulonglong, C.c_ulong,
                                    C.c_void_p, C.c_ulong]
    base = image_base(exe)
    if not dbg.SymLoadModuleEx(h, None, exe.encode("mbcs"), None, base, 0, None, 0):
        sys.exit(f"Modul nicht geladen: {C.GetLastError()}")
    dbg.SymFromAddr.argtypes = [C.c_void_p, C.c_ulonglong, C.POINTER(C.c_ulonglong), C.POINTER(SymbolInfo)]
    dbg.SymGetLineFromAddr64.argtypes = [C.c_void_p, C.c_ulonglong, C.POINTER(C.c_ulong), C.POINTER(Line64)]
    for o in offs:
        a = base + o
        s = SymbolInfo(SizeOfStruct=88, MaxNameLen=500)
        disp = C.c_ulonglong()
        name = f"{s.Name.decode()}+{disp.value:#x}" if dbg.SymFromAddr(h, a, disp, s) else "?"
        ln = Line64(SizeOfStruct=C.sizeof(Line64))
        d2 = C.c_ulong()
        where = f"{ln.FileName.decode()}:{ln.LineNumber}" if dbg.SymGetLineFromAddr64(h, a, d2, ln) else ""
        print(f"{o:#x}: {name}  {where}")


if __name__ == "__main__":
    main()

"""Read meh321's AddressLibraryDatabase .relib (Skyrim AE ID -> offset per game version).

Format mirrors AddressLibraryManager/Manager.cs (Database.LoadOffsets, Library.ReadFromStream).
Usage: python3 relib.py <skyrimae.relib> [--versions] [--ids 37673 38627 ...]
"""
import struct
import sys


class Reader:
    def __init__(self, data):
        self.d, self.p = data, 0

    def take(self, fmt):
        v = struct.unpack_from("<" + fmt, self.d, self.p)
        self.p += struct.calcsize("<" + fmt)
        return v[0] if len(v) == 1 else v

    def string(self):  # .NET BinaryWriter 7-bit length prefix
        n, shift = 0, 0
        while True:
            b = self.take("B")
            n |= (b & 0x7F) << shift
            shift += 7
            if b < 0x80:
                break
        s = self.d[self.p:self.p + n].decode("utf-8")
        self.p += n
        return s


def load(path):
    r = Reader(open(path, "rb").read())
    version = r.take("i")
    r.take("Q")  # HighVID
    r.take("i")  # PointerSize
    if r.take("B"):
        r.string()  # TargetModuleName
    libs = {}
    for _ in range(r.take("i")):
        nver = r.take("i")
        ver = tuple(r.take("I") for _ in range(nver))
        if r.take("B"):
            r.string()
        base = r.take("q")
        values = {}
        for _ in range(r.take("i")):
            k = r.take("Q")
            values[k] = r.take("I")
        for _ in range(r.take("i")):
            r.take("Q"), r.take("Q")
        libs[ver] = (base, values)
    return version, libs


if __name__ == "__main__":
    fmtv = lambda v: ".".join(map(str, v))
    _, libs = load(sys.argv[1])
    if "--versions" in sys.argv:
        for v, (base, vals) in sorted(libs.items()):
            print(fmtv(v), hex(base), len(vals))
    if "--ids" in sys.argv:
        ids = [int(x) for x in sys.argv[sys.argv.index("--ids") + 1:]]
        for v, (base, vals) in sorted(libs.items()):
            print(fmtv(v), " ".join(f"{i}:{hex(vals[i]) if i in vals else '-'}" for i in ids))

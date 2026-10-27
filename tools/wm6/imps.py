#!/usr/bin/env python3
"""List the coredll (or other) imports of a PE by name.  imps.py module.exe [exporter.dll]"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE
def imports(p):
    rva = p.dd[2]; res = {}
    o = 0
    while rva:
        d = p.read(rva + o, 20)
        ilt, _, _, name, iat = struct.unpack('<IIIII', d)
        if not name: break
        dll = p.cstr(name).lower(); lst = []
        t = ilt or iat
        while True:
            v, = struct.unpack('<I', p.read(t, 4))
            if not v: break
            lst.append(v & 0xffff if v & 0x80000000 else p.cstr((v & 0x7fffffff) + 2))
            t += 4
        res[dll] = lst; o += 20
    return res
if __name__ == '__main__':
    p = PE(sys.argv[1]); exp = {}
    if len(sys.argv) > 2:
        exp = {o: n for o, n, r in PE(sys.argv[2]).exports()}
    for dll, l in imports(p).items():
        print('==', dll, len(l))
        print(' '.join(exp.get(x, str(x)) if isinstance(x, int) else x for x in l))

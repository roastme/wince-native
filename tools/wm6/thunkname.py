#!/usr/bin/env python3
"""thunkname.py module.exe vma... : name the import behind each import thunk (VMA = RVA+0x10000 as in dis.py output)"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE
R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
def build(path):
    p = PE(path); names = {}
    rva = p.dd[2]; o = 0
    while True:
        ilt, _, _, name, iat = struct.unpack('<IIIII', p.read(rva + o, 20))
        if not name: break
        dll = p.cstr(name); t = ilt or iat; i = 0
        exp = {}
        dp = os.path.join(R, 'ce5-ref/work/wm6rom/flat', dll.lower())
        if os.path.exists(dp): exp = {oo: n for oo, n, f in PE(dp).exports()}
        while True:
            v, = struct.unpack('<I', p.read(t, 4))
            if not v: break
            nm = (exp.get(v & 0xffff, '#%d' % (v & 0xffff)) if v & 0x80000000 else p.cstr((v & 0x7fffffff) + 2))
            names[iat + 4 * i] = dll + '!' + nm
            i += 1; t += 4
        o += 20
    return p, names
if __name__ == '__main__':
    p, names = build(sys.argv[1])
    for a in sys.argv[2:]:
        v = int(a, 0) - p.base
        w = struct.unpack('<4I', p.read(v, 16))
        lit = w[3]
        print(hex(int(a, 0)), names.get(lit - p.base, '?%x' % lit))

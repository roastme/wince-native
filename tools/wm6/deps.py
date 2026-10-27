#!/usr/bin/env python3
"""Transitive import closure of a module inside an extracted WM6 tree.  deps.py <tree/fs/Windows> name [name...]"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE

def imports(p):
    rva = p.dd[2]
    out = {}
    if not rva: return out
    o = 0
    while True:
        d = p.read(rva + o, 20)
        if len(d) < 20: break
        ilt, _, _, name, iat = struct.unpack('<IIIII', d)
        if not name: break
        out[p.cstr(name).lower()] = (ilt or iat, iat)
        o += 20
    return out

def closure(root, names):
    seen, todo, missing = {}, list(names), set()
    while todo:
        n = todo.pop().lower()
        if n in seen: continue
        f = os.path.join(root, n)
        if not os.path.exists(f): missing.add(n); continue
        seen[n] = sorted(imports(PE(f)))
        todo += seen[n]
    return seen, missing

if __name__ == '__main__':
    seen, missing = closure(sys.argv[1], sys.argv[2:])
    for n, d in sorted(seen.items()): print(n, '->', ' '.join(d))
    print('missing:', ' '.join(sorted(missing)))

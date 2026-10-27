#!/usr/bin/env python3
"""Summarise an ARM9 PC trace (dsirun -T) as a call-flow: tsym.py trace.txt name=base[:file] ... [--max N]
Collapses runs of sequential PCs, shows module+export-relative names for the given flat-loaded modules."""
import sys, os, bisect
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE
args = sys.argv[1:]; mx = 400
if '--max' in args:
    i = args.index('--max'); mx = int(args[i+1]); del args[i:i+2]
tr = [int(x, 16) for x in open(args[0])]
mods = []
for a in args[1:]:
    nm, rest = a.split('=', 1)
    base, _, f = rest.partition(':')
    base = int(base, 16)
    p = PE(f) if f else None
    ex = sorted((r, n or ('#%d' % o)) for o, n, r in p.exports()) if p else []
    mods.append((nm, base, p.size if p else 0x100000, ex))
def name(pc):
    for nm, base, size, ex in mods:
        if base <= pc < base + size:
            rva = pc - base
            keys = [r for r, n in ex]
            i = bisect.bisect_right(keys, rva) - 1
            if i >= 0 and rva - ex[i][0] < 0x2000: return '%s!%s+%x' % (nm, ex[i][1], rva - ex[i][0])
            return '%s+%x' % (nm, rva)
    return None
out = []; cur = None
for pc in tr:
    if not any(base <= pc < base + size for nm, base, size, ex in mods): continue
    if cur and pc == cur[1] + 4: cur[1] = pc
    else:
        cur = [pc, pc]; out.append(cur)
for a, b in out[:mx]:
    print('%s .. %x' % (name(a), b))
print(len(out), 'blocks')

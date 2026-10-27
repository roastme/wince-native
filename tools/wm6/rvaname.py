#!/usr/bin/env python3
"""rvaname.py module.dll rva... : nearest export at or below each RVA"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE
p = PE(sys.argv[1])
ex = sorted(((f & ~1) - p.base if (f & ~1) >= p.base else (f & ~1), n) for o, n, f in p.exports())
for a in sys.argv[2:]:
    t = int(a, 0)
    c = [e for e in ex if e[0] <= t]
    print(hex(t), (c[-1][1], hex(t - c[-1][0])) if c else None)

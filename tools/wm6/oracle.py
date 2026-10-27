#!/usr/bin/env python3
"""Compare the reloc heuristic against a two-image differential oracle: same-named DLLs of equal size linked at different ROM bases
in two WM6 images (ppc vs std). A word that differs by exactly (baseA-baseB) is a real image-relative pointer."""
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE, cerom_objs
A, B = sys.argv[1], sys.argv[2]

def words(p, skip=('.rsrc', '.cerom')):
    """yield (rva, value, section) for 4-byte aligned words of initialised section data"""
    for n, va, vs, ro, rs, fl in p.secs:
        if n in skip: continue
        for off in range(0, rs - 3, 4):
            yield va + off, struct.unpack_from('<I', p.d, ro + off)[0], n

def truth(pa, pb):
    wa = {r: v for r, v, n in words(pa)}; wb = {r: v for r, v, n in words(pb)}
    ds = {(pa.base - pb.base) & 0xffffffff}
    for oa, ob in zip(cerom_objs(pa), cerom_objs(pb)):
        ds.add((oa['realaddr'] - ob['realaddr']) & 0xffffffff)
    ds.discard(0)
    T = set(); diff = set()
    for r, v in wa.items():
        w = wb.get(r)
        if w is None or w == v: continue
        if (v - w) & 0xffffffff in ds: T.add(r)
        else: diff.add(r)
    return T, diff

def ranges(p):
    """[(lo, hi)] of VA windows a pointer can fall in: the image and the split RW data window(s)"""
    lo = p.base + min(s[1] for s in p.secs if s[0] not in ('.cerom',)); hi = p.base + p.size
    r = [(lo, hi)]
    for o in cerom_objs(p):
        if o['realaddr'] and o['realaddr'] != p.base + o['rva'] and o['vsize']:
            r.append((o['realaddr'], o['realaddr'] + o['vsize']))
    return r

def heur(p):
    rg = ranges(p)
    return {r for r, v, n in words(p) if any(lo <= v < hi for lo, hi in rg)}

if __name__ == '__main__':
    tot = [0, 0, 0, 0]
    rows = []
    for f in sorted(os.listdir(A)):
        a = os.path.join(A, f); b = os.path.join(B, f)
        if not os.path.isfile(b) or not f.lower().endswith('.dll'): continue
        try: pa = PE(a); pb = PE(b)
        except Exception: continue
        if pa.size != pb.size or pa.base == pb.base or pa.base < 0x100000 or len(pa.d) != len(pb.d): continue
        T, diff = truth(pa, pb)
        if len(diff) > 0.2 * max(1, len(T)): continue    # content differs between images: not usable as an oracle
        H = heur(pa)
        tp = len(H & T); fp = len(H - T); fn = len(T - H)
        rows.append((f, len(T), tp, fp, fn, len(diff)))
        tot[0] += len(T); tot[1] += tp; tot[2] += fp; tot[3] += fn
    for r in sorted(rows, key=lambda r: -r[3])[:25]: print('%-32s truth %6d tp %6d fp %5d fn %5d diffother %d' % r)
    print(len(rows), 'modules  truth', tot[0], 'tp', tot[1], 'fp', tot[2], 'fn', tot[3])

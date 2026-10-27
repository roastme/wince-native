#!/usr/bin/env python3
"""Turn a ROM-extracted CE module (extract-wince-rom --fs=raw output) into an ordinary relocatable PE.

ROM modules lost their .reloc and EXEs are fixed at 0x10000, so the relocation table is synthesised by scanning for words that point
into the module's own address window (measured against a two-image oracle in oracle.py: no missed pointers, ~1-3% extra hits).
Pointers into the split RW data window (o32_realaddr != vbase+rva, ROM DLL data) are rewritten to the normal in-image address first,
so the result has a single ImageBase and one delta relocates everything.

usage: mkpe.py in.dll out.dll
"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE, cerom_objs

SKIP = ('.rsrc', '.cerom', '.reloc')

def align(x, a): return (x + a - 1) & ~(a - 1)

def excludes(p):
    """[(rva_lo, rva_hi)] of metadata that holds RVAs, not absolute pointers"""
    ex = []
    er, es = p.dd[0], p.dd[1]
    if er:
        ex.append((er, er + es))
    ir, isz = p.dd[2], p.dd[3]
    if ir:
        ex.append((ir, ir + isz))
        o = 0
        while True:
            ilt, ts, fc, name, iat = struct.unpack('<5I', p.read(ir + o, 20))
            if not name: break
            for t in (ilt, iat):
                n = 0
                while struct.unpack('<I', p.read(t + 4 * n, 4))[0]: n += 1
                ex.append((t, t + 4 * (n + 1)))
            o += 20
    for i in (4, 6, 14):                       # resources, debug, CLR
        if p.dd[2 * i]: ex.append((p.dd[2 * i], p.dd[2 * i] + p.dd[2 * i + 1]))
    return ex

def scan(p):
    vbase = p.base
    first = min(s[1] for s in p.secs if s[0] not in SKIP)
    wins = [(vbase + first, vbase + align(p.size, 0x1000), 0)]          # (lo, hi, add)  v -> v + add
    for o in cerom_objs(p):
        if o['realaddr'] and o['realaddr'] != vbase + o['rva'] and o['vsize']:
            wins.append((o['realaddr'], o['realaddr'] + o['vsize'], vbase + o['rva'] - o['realaddr']))
    ex = excludes(p)
    pr, ps = p.dd[6], p.dd[7]                  # exception directory
    hits = []                                  # (rva, new_value)
    for n, va, vs, ro, rs, fl in p.secs:
        if n in SKIP: continue
        stride = 8 if (ps and va <= pr < va + max(vs, rs)) else 4
        for off in range(0, rs - 3, stride):
            rva = va + off
            if any(a <= rva < b for a, b in ex): continue
            v, = struct.unpack_from('<I', p.d, ro + off)
            for lo, hi, add in wins:
                if lo <= v < hi:
                    hits.append((rva, (v + add) & 0xffffffff)); break
    return hits

def build(p, hits):
    cer = {s[0]: s for s in p.secs}
    keep = [s for s in p.secs if s[0] not in ('.cerom', '.reloc')]
    secdata = {}
    for n, va, vs, ro, rs, fl in keep:
        secdata[n] = bytearray(p.d[ro:ro + rs])
    hits = [h for h in hits if any(va <= h[0] and h[0] + 4 <= va + len(p.d[ro:ro + rs]) for n, va, vs, ro, rs, fl in keep)]     # drop hits straddling a section end
    for rva, nv in hits:
        for n, va, vs, ro, rs, fl in keep:
            if va <= rva and rva + 4 <= va + len(secdata[n]):
                struct.pack_into('<I', secdata[n], rva - va, nv); break
    # reloc blocks (HIGHLOW)
    rel = bytearray(); i = 0
    rv = sorted(r for r, _ in hits)
    while i < len(rv):
        page = rv[i] & ~0xfff; ent = []
        while i < len(rv) and (rv[i] & ~0xfff) == page:
            ent.append((3 << 12) | (rv[i] & 0xfff)); i += 1
        if len(ent) & 1: ent.append(0)
        rel += struct.pack('<II', page, 8 + 2 * len(ent)) + b''.join(struct.pack('<H', e) for e in ent)
    last_end = max(va + max(vs, rs) for n, va, vs, ro, rs, fl in keep)
    relrva = align(last_end, 0x1000)
    nsec = len(keep) + 1
    hdr = bytearray(p.d[:p.opt + struct.unpack_from('<H', p.d, p.pe + 20)[0] + 40 * p.nsec])
    hsize = align(len(hdr) + 40, 0x200)
    hdr = bytearray(hdr.ljust(hsize, b'\0'))
    pe = p.pe
    struct.pack_into('<H', hdr, pe + 6, nsec)
    ch, = struct.unpack_from('<H', hdr, pe + 22); struct.pack_into('<H', hdr, pe + 22, ch & ~1)     # RELOCS_STRIPPED off
    struct.pack_into('<I', hdr, p.opt + 60, hsize)                                                    # SizeOfHeaders
    struct.pack_into('<I', hdr, p.opt + 56, align(relrva + len(rel), 0x1000))                         # SizeOfImage
    struct.pack_into('<II', hdr, p.opt + 96 + 5 * 8, relrva, len(rel))                                # DD[5]
    osz = struct.unpack_from('<H', hdr, pe + 20)[0]
    sect = p.opt + osz
    body = bytearray(); raw = hsize
    allsec = []
    for n, va, vs, ro, rs, fl in keep:
        data = bytes(secdata[n]); rsz = align(len(data), 0x200)
        allsec.append((n.encode(), vs, va, rsz, raw, fl, data)); raw += rsz
    rsz = align(len(rel), 0x200)
    allsec.append((b'.reloc', len(rel), relrva, rsz, raw, 0x42000040, bytes(rel)))
    for i, (n, vs, va, rsz, ro, fl, data) in enumerate(allsec):
        struct.pack_into('<8sIIIIIIHHI', hdr, sect + 40 * i, n, vs, va, rsz, ro, 0, 0, 0, 0, fl)
        body += data.ljust(rsz, b'\0')
    return bytes(hdr) + bytes(body)

if __name__ == '__main__':
    p = PE(sys.argv[1]); hits = scan(p)
    out = build(p, hits)
    open(sys.argv[2], 'wb').write(out)
    print('%s: base %x, %d relocs, %d bytes' % (os.path.basename(sys.argv[1]), p.base, len(hits), len(out)))

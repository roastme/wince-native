"""Minimal PE reader for CE (ARM) modules extracted by extract-wince-rom (read-only helper)."""
import struct

class PE:
    def __init__(self, path):
        self.d = d = open(path, 'rb').read()
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        assert d[pe:pe+4] == b'PE\0\0', path
        self.pe = pe
        self.nsec, = struct.unpack_from('<H', d, pe + 6)
        osz, = struct.unpack_from('<H', d, pe + 20)
        self.opt = pe + 24
        self.entry, = struct.unpack_from('<I', d, self.opt + 16)
        self.base, = struct.unpack_from('<I', d, self.opt + 28)
        self.size, = struct.unpack_from('<I', d, self.opt + 56)
        self.dd = struct.unpack_from('<32I', d, self.opt + 96)
        self.secs = []
        o = self.opt + osz
        for i in range(self.nsec):
            n, vs, va, rs, ro, _, _, _, _, fl = struct.unpack_from('<8sIIIIIIHHI', d, o + 40 * i)
            self.secs.append((n.rstrip(b'\0').decode(), va, vs, ro, rs, fl))
    def rva2off(self, rva):
        for n, va, vs, ro, rs, fl in self.secs:
            if va <= rva < va + max(vs, rs):
                return ro + rva - va if rva - va < rs else None
        return None
    def read(self, rva, n):
        o = self.rva2off(rva)
        return self.d[o:o+n] if o is not None else b'\0' * n
    def cstr(self, rva):
        o = self.rva2off(rva); e = self.d.index(b'\0', o); return self.d[o:e].decode('latin1')
    def exports(self):
        r, sz = self.dd[0], self.dd[1]
        if not r: return []
        _, _, _, _, ordbase, nfun, nnames, afun, anames, aords = struct.unpack_from('<IIIIIIIIII', self.read(r, 40))
        funcs = struct.unpack('<%dI' % nfun, self.read(afun, 4 * nfun))
        names = {}
        if nnames:
            nn = struct.unpack('<%dI' % nnames, self.read(anames, 4 * nnames))
            oo = struct.unpack('<%dH' % nnames, self.read(aords, 2 * nnames))
            for a, b in zip(nn, oo): names[b] = self.cstr(a)
        return [(ordbase + i, names.get(i), f) for i, f in enumerate(funcs) if f]

def cerom_objs(p):
    """o32 records carried in the .cerom section: list of dict(vsize,rva,psize,dataptr,realaddr,flags)"""
    import struct
    for n, va, vs, ro, rs, fl in p.secs:
        if n == '.cerom':
            b = p.d[ro:ro + rs]
            magic, ver, hs, nobj, osz, toc = struct.unpack_from('<6I', b, 0)
            out = []
            for i in range(nobj):
                v = struct.unpack_from('<6I', b, hs + osz * i)
                out.append(dict(vsize=v[0], rva=v[1], psize=v[2], dataptr=v[3], realaddr=v[4], flags=v[5]))
            return out
    return []

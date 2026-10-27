#!/usr/bin/env python3
"""Binary patches that make MMU-era WM6 code run in the DSi's flat address space (VA == PA). In place, idempotent.
usage: patchflat.py file...
 - handle -> HDATA: `bic Rd,Rm,#0xe0000003 ; orr Rx,Rd,#0x80000000` (user-visible alias of the kernel handle table at 0x80000000)
   becomes `bic ... ; mov Rx,Rd`, since our handles already hold the HDATA address."""
import struct, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE

def patch(path):
    p = PE(path); d = bytearray(p.d); n = 0
    for name, va, vs, ro, rs, fl in p.secs:
        if fl & 0x20000000 == 0 and not name.startswith('.text'): continue
        for i in range(ro, ro + rs - 8, 4):
            w, = struct.unpack_from('<I', d, i)
            if (w & 0x0ff00fff) == 0x03c0023e:
                rd = (w >> 12) & 15; cond = w >> 28
                for j in (4, 8):
                    w2, = struct.unpack_from('<I', d, i + j)
                    # orr Rx, Rd, #0x80000000 -> mov Rx, Rd
                    if (w2 & 0x0ff00fff) == 0x03800102 and ((w2 >> 16) & 15) == rd:
                        rx = (w2 >> 12) & 15
                        struct.pack_into('<I', d, i + j, (w2 & 0xf0000000) | 0x01a00000 | (rx << 12) | rd)
                        n += 1
    if n: open(path, 'wb').write(d)
    return n

if __name__ == '__main__':
    for f in sys.argv[1:]:
        try: n = patch(f)
        except Exception: continue
        if n: print('%s: %d patch(es)' % (f, n))

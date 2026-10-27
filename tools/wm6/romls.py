#!/usr/bin/env python3
# List modules/files of a Windows CE .nb0/.bin ROM (read-only analysis helper). usage: romls.py image.bin [mods|files|-] [file_offset]
import struct, sys
FO = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0
d = open(sys.argv[1], 'rb').read()[FO:]
assert d[0x40:0x44] == b'ECEC'
ptoc, = struct.unpack_from('<I', d, 0x44)
# the image's physical start is the va whose file offset is 0; derive from a known TOC offset in word 0x48
tocoff, = struct.unpack_from('<I', d, 0x48)
base = ptoc - tocoff
print('ptoc %08x tocoff %x base %08x' % (ptoc, tocoff, base))
def off(va): return va - base
H = struct.unpack_from('<21I', d, tocoff)
(dllfirst, dlllast, physfirst, physlast, nummods, ramstart, ramfree, ramend, copyent, copyoff,
 proflen, profoff, numfiles, kflags, fsram, drvstart, drvlen, cpumisc, pext, trkstart, trklen) = H[:21]
print('mods %d files %d dll %08x-%08x phys %08x-%08x ram %08x-%08x-%08x cpu %x' % (nummods, numfiles, dllfirst, dlllast, physfirst, physlast, ramstart, ramfree, ramend, cpumisc & 0xffff))
def cstr(va):
    o = off(va); e = d.index(b'\0', o); return d[o:e].decode('latin1')
mods = []
p = tocoff + 84
for i in range(nummods):
    attr, = struct.unpack_from('<I', d, p)
    size, nameva, e32, o32, load = struct.unpack_from('<5I', d, p + 12)
    mods.append((cstr(nameva), size, e32, o32, load))
    p += 32
files = []
for i in range(numfiles):
    attr, = struct.unpack_from('<I', d, p)
    real, comp, nameva, load = struct.unpack_from('<4I', d, p + 12)
    files.append((cstr(nameva), real, comp, load))
    p += 28
if len(sys.argv) > 2 and sys.argv[2] == 'mods':
    for m in mods: print('MOD %-28s size %8d e32 %08x o32 %08x' % m[:4])
elif len(sys.argv) > 2 and sys.argv[2] == 'files':
    for f in files: print('FILE %-40s real %8d comp %8d' % f[:3])
else:
    print('first mods:', [m[0] for m in mods[:40]])

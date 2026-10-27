#!/usr/bin/env python3
"""Disassemble bytes at an RVA of an extracted CE module: dis.py module.dll rva|exportname [count_words]  (uses the Wonderful arm-none-eabi-objdump via wfenter.sh)"""
import sys, os, subprocess, struct
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE
R = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
p = PE(sys.argv[1]); a = sys.argv[2]; n = int(sys.argv[3]) if len(sys.argv) > 3 else 12
if not a.startswith('0x') and not a.isdigit():
    a = {nm: f for o, nm, f in p.exports()}[a]
else:
    a = int(a, 0)
tmp = os.path.join(R, 'ce5-ref/work/wm6rom/_dis.bin')
open(tmp, 'wb').write(p.read(a & ~1, 4 * n))
thumb = (a & 1)
out = subprocess.run([R + '/tools/wfenter.sh', '/opt/wonderful/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-objdump', '-D', '-b', 'binary', '-marm', '-M', 'force-thumb' if thumb else 'no-force-thumb', '--adjust-vma=0x%x' % (p.base + (a & ~1)), tmp], capture_output=True, text=True).stdout
print('\n'.join(out.splitlines()[7:]))
os.remove(tmp)

#!/usr/bin/env python3
"""Stage a WM6 ROM tree (extract-wince-rom output) onto the SD folder served to the DSi: ROM modules become relocatable PEs (mkpe),
everything else is copied as is.   usage: mksd.py <rom_fs_dir> <out_dir> [-j N]   e.g. mksd.py ppc/fs sdroot/wm6"""
import sys, os, shutil, traceback
sys.path.insert(0, os.path.dirname(__file__))
from pe import PE
import mkpe
import patchflat

def convert(src, dst):
    d = open(src, 'rb').read(2)
    if d == b'MZ':
        try:
            p = PE(src)
            if any(s[0] == '.cerom' for s in p.secs):
                open(dst, 'wb').write(mkpe.build(p, mkpe.scan(p)))
                patchflat.patch(dst)
                return 'pe'
        except ValueError:
            pass            # resource-only module (just .rsrc): already a usable PE
        except Exception:
            traceback.print_exc(); return 'fail'
    shutil.copyfile(src, dst)
    return 'raw'

if __name__ == '__main__':
    src, out = sys.argv[1], sys.argv[2]
    n = {'pe': 0, 'raw': 0, 'fail': 0}
    for root, dirs, files in os.walk(src):
        rel = os.path.relpath(root, src)
        os.makedirs(os.path.join(out, rel), exist_ok=True)
        for f in files:
            s = os.path.join(root, f); t = os.path.join(out, rel, f)
            if os.path.exists(t) and os.path.getmtime(t) >= os.path.getmtime(s): continue
            r = convert(s, t); n[r] += 1
            if r == 'fail': print('FAIL', s)
    print(n)

#!/usr/bin/env python3
"""Make a case-insensitive include overlay: for every #include "x"/<x> in the given source trees,
symlink compat/ci/<x as written> to the real file found (case-insensitively) in the header trees."""
import os, re, sys
roots = sys.argv[2:]
out = sys.argv[1]
idx = {}
for r in roots:
    for d, _, fs in os.walk(r):
        for f in fs:
            idx.setdefault(f.lower(), os.path.join(d, f))
pat = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)
made = 0
for r in roots:
    for d, _, fs in os.walk(r):
        for f in fs:
            if not f.lower().endswith(('.c', '.h', '.cpp', '.hpp', '.hxx', '.inc')):
                continue
            try:
                txt = open(os.path.join(d, f), encoding='latin-1').read()
            except OSError:
                continue
            for m in pat.finditer(txt):
                name = m.group(1).replace('\\', '/')
                base = os.path.basename(name)
                real = idx.get(base.lower())
                if real and os.path.basename(real) != base:
                    link = os.path.join(out, base)
                    if not os.path.lexists(link):
                        os.symlink(os.path.abspath(real), link)
                        made += 1
print("symlinks:", made)

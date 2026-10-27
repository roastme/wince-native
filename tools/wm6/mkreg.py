#!/usr/bin/env python3
"""Merge the WM6 ROM registry sources (*.reg system, *.rgu user; REGEDIT4 text, ASCII or UTF-16) into one compact binary blob that the DSi
kernel's registry loads at boot.   usage: mkreg.py <Registry dir> <rgu dir or -> <out.reg>

Blob (little endian): 'DREG', u32 nnodes, u32 nvals, u32 off_nodes, off_vals, off_strs, off_data, total
  node (24 B): u32 name_off, parent, child, sibling, val_first, nvals     (indices; strings are UTF-16LE NUL terminated in the pool)
  val  (16 B): u32 name_off, type, data_off, size
"""
import sys, os, re, struct

class Node:
    def __init__(self, name, parent):
        self.name, self.parent, self.kids, self.vals = name, parent, {}, {}

def read_text(path):
    b = open(path, 'rb').read()
    if b[:2] == b'\xff\xfe': return b[2:].decode('utf-16le')
    if b[:2] == b'\xfe\xff': return b[2:].decode('utf-16be')
    return b.decode('latin1')

ROOT = Node('', None)
def key(path, create=True):
    n = ROOT
    for part in path.split('\\'):
        if not part: continue
        k = part.lower()
        if k not in n.kids:
            if not create: return None
            n.kids[k] = Node(part, n)
        n = n.kids[k]
    return n

def unq(s):
    out = []; i = 0
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s): i += 1; c = s[i]
        out.append(c); i += 1
    return ''.join(out)

def parse_str(s):
    """s begins with a quote; returns (string, rest)"""
    i = 1; out = []
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s): out.append(s[i+1]); i += 2; continue
        if c == '"': return ''.join(out), s[i+1:]
        out.append(c); i += 1
    return ''.join(out), ''

def wz(s): return (s + '\0').encode('utf-16le')

def parse_value(v):
    v = v.strip()
    if v == '-': return None
    if v.startswith('"'): return (1, wz(parse_str(v)[0]))
    m = re.match(r'dword:([0-9a-fA-F]+)', v)
    if m: return (4, struct.pack('<I', int(m.group(1), 16)))
    m = re.match(r'hex(?:\((\w+)\))?:(.*)', v, re.S)
    if m:
        t = int(m.group(1), 16) if m.group(1) else 3
        h = [x for x in m.group(2).replace('\\', '').replace('\n', '').replace('\r', '').split(',') if x.strip()]
        return (t, bytes(int(x, 16) for x in h))
    m = re.match(r'(multi_sz|mui_sz|expand_sz):(.*)', v, re.S)
    if m:
        kind = m.group(1); strs = []; rest = m.group(2).strip()
        while rest.startswith('"'):
            s, rest = parse_str(rest); strs.append(s); rest = rest.lstrip(', \r\n')
        if kind == 'multi_sz': return (7, b''.join(wz(s) for s in strs) + b'\0\0')
        return (21 if kind == 'mui_sz' else 2, wz(strs[0] if strs else ''))
    raise ValueError('value ' + v[:40])

def merge(path):
    txt = read_text(path).replace('\r\n', '\n')
    # join continuation lines (backslash at EOL)
    txt = re.sub(r'\\\n\s*', '', txt)
    cur = None
    lines = []; acc = ''
    for l in txt.split('\n'):
        l = l.strip()
        m = None if l.startswith('[') else re.search(r';', l)
        while m:
            if l[:m.start()].count('"') % 2 == 0 and m.start() > 0: l = l[:m.start()].rstrip(); break
            m = re.search(r';', l[m.end():]) and re.compile(r';').search(l, m.end())
        if acc and (l.startswith('[') or re.match(r'^("([^"\\]|\\.)*"|@)=', l)):
            lines.append(acc.rstrip(',')); acc = ''       # trailing comma but no continuation
        if acc or l.endswith(','):
            acc += l
            if l.endswith(','): continue
            l, acc = acc, ''
        lines.append(l)
    for line in lines:
        if not line or line.startswith(';') or line.startswith('REGEDIT'): continue
        if line.startswith('['):
            p = line.rstrip()
            assert p.endswith(']'), (path, p)
            p = p[1:-1]
            if p.startswith('-'):
                n = key(p[1:], False)
                if n and n.parent: del n.parent.kids[n.name.lower()]
                cur = None
            else:
                cur = key(p)
            continue
        if cur is None: continue
        if line.startswith('@='):
            name, v = '', line[2:]
        elif line.startswith('"'):
            name, rest = parse_str(line)
            rest = rest.lstrip()
            assert rest.startswith('='), (path, line)
            v = rest[1:]
        else:
            continue
        pv = parse_value(v)
        if pv is None: cur.vals.pop(name.lower(), None)
        else: cur.vals[name.lower()] = (name, pv[0], pv[1])

def serialize():
    nodes = []; vals = []; strs = bytearray(); data = bytearray(); sidx = {}
    def sname(s):
        if s not in sidx:
            sidx[s] = len(strs); strs.extend(wz(s))
        return sidx[s]
    def walk(n, parent):
        idx = len(nodes); nodes.append([sname(n.name), parent, 0, 0, 0, 0]); me = nodes[idx]
        me[4] = len(vals); me[5] = len(n.vals)
        for _, (nm, t, d) in sorted(n.vals.items()):
            while len(data) & 3: data.append(0)
            vals.append((sname(nm), t, len(data), len(d))); data.extend(d)
        prev = 0
        for _, k in sorted(n.kids.items()):
            ci = walk(k, idx)
            if prev: nodes[prev][3] = ci
            else: me[2] = ci
            prev = ci
        return idx
    walk(ROOT, 0xFFFFFFFF)
    hdr = 36
    o_nodes = hdr; o_vals = o_nodes + 24 * len(nodes); o_strs = o_vals + 16 * len(vals); o_data = (o_strs + len(strs) + 3) & ~3
    total = o_data + len(data)
    out = bytearray(b'DREG' + struct.pack('<8I', len(nodes), len(vals), o_nodes, o_vals, o_strs, o_data, total, 0))
    for n in nodes: out += struct.pack('<6I', *n)
    for v in vals: out += struct.pack('<4I', *v)
    out += strs
    while len(out) < o_data: out.append(0)
    out += data
    return bytes(out), len(nodes), len(vals)

if __name__ == '__main__':
    regdir, rgudir, outp = sys.argv[1:4]
    files = sorted(f for f in os.listdir(regdir) if f.lower().endswith('.reg'))
    for f in files: merge(os.path.join(regdir, f))
    if rgudir != '-':
        for f in sorted(os.listdir(rgudir)):
            if f.lower().endswith('.rgu'): merge(os.path.join(rgudir, f))
    blob, nn, nv = serialize()
    open(outp, 'wb').write(blob)
    print('%d keys, %d values, %d bytes' % (nn, nv, len(blob)))

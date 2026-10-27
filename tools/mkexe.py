#!/usr/bin/env python3
"""mkexe.py - turn a GCC ARM ELF (linked by ceapp/ceapp.ld with -q) into a Windows CE PE executable.

usage: mkexe.py in.elf imports.txt out.exe

imports.txt lists one coredll export name per line, in the order of the .iat words the stub file
(made by ceapp/build.sh) defines. R_ARM_ABS32 relocations become PE HIGHLOW base relocations, so the
image can be loaded anywhere, as the DSi flat loader requires.
"""
import struct, sys
from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

BASE, SALIGN, FALIGN = 0x10000, 0x1000, 0x200
R_ARM_ABS32, R_ARM_TARGET1 = 2, 38


def align(v, a):
    return (v + a - 1) & ~(a - 1)


def main(elf_path, imp_path, out_path):
    elf = ELFFile(open(elf_path, 'rb'))
    text, data, bss = (elf.get_section_by_name(n) for n in ('.text', '.data', '.bss'))
    syms = elf.get_section_by_name('.symtab')
    iat = syms.get_symbol_by_name('__iat_start')
    entry = syms.get_symbol_by_name('WinMainCRTStartup')[0]['st_value']
    names = [l.strip() for l in open(imp_path) if l.strip()]

    # Base relocations: every absolute word in the loaded sections.
    loaded = {s.name: s for s in (text, data) if s}
    fix = set()
    for rs in elf.iter_sections():
        if not isinstance(rs, RelocationSection):
            continue
        tgt = elf.get_section(rs['sh_info'])
        if tgt.name not in loaded:
            continue
        for r in rs.iter_relocations():
            if r['r_info_type'] in (R_ARM_ABS32, R_ARM_TARGET1):
                off = r['r_offset']
                # -q keeps section-relative offsets in relocatable form; the final link makes them addresses.
                fix.add(off if off >= tgt['sh_addr'] else tgt['sh_addr'] + off)

    t_rva = text['sh_addr'] - BASE
    t_raw = text.data()
    d_rva = data['sh_addr'] - BASE if data else align(t_rva + len(t_raw), SALIGN)
    d_raw = bytearray(data.data() if data else b'')
    d_vsize = len(d_raw)
    if bss and bss['sh_size']:
        d_vsize = bss['sh_addr'] + bss['sh_size'] - BASE - d_rva

    # Import section after the data: descriptor, ILT, hint/name table, dll name.
    i_rva = align(d_rva + max(d_vsize, 1), SALIGN)
    n = len(names)
    desc_size, ilt_off = 40, 40
    hn_off = ilt_off + 4 * (n + 1)
    hn = bytearray()
    hn_rvas = []
    for nm in names:
        hn_rvas.append(i_rva + hn_off + len(hn))
        hn += struct.pack('<H', 0) + nm.encode() + b'\0'
        if len(hn) & 1:
            hn += b'\0'
    dll_off = hn_off + len(hn)
    idata = bytearray(hn_off) + hn + b'coredll.dll\0'
    iat_rva = (iat[0]['st_value'] - BASE) if iat and n else 0
    struct.pack_into('<5I', idata, 0, i_rva + ilt_off, 0, 0, i_rva + dll_off, iat_rva)
    for k, r in enumerate(hn_rvas):
        struct.pack_into('<I', idata, ilt_off + 4 * k, r)
        struct.pack_into('<I', d_raw, iat_rva - d_rva + 4 * k, r)   # IAT holds the same name RVAs until bound

    # .reloc: one block per 4K page.
    r_rva = align(i_rva + len(idata), SALIGN)
    reloc = bytearray()
    pages = {}
    for a in sorted(fix):
        pages.setdefault((a - BASE) & ~0xfff, []).append((a - BASE) & 0xfff)
    for pg, offs in sorted(pages.items()):
        ents = [(3 << 12) | o for o in offs]
        if len(ents) & 1:
            ents.append(0)
        reloc += struct.pack('<II', pg, 8 + 2 * len(ents)) + struct.pack('<%dH' % len(ents), *ents)

    secs = [(b'.text', t_rva, len(t_raw), bytes(t_raw), 0x60000020),
            (b'.data', d_rva, d_vsize, bytes(d_raw), 0xC0000040),
            (b'.idata', i_rva, len(idata), bytes(idata), 0xC0000040),
            (b'.reloc', r_rva, len(reloc), bytes(reloc), 0x42000040)]
    hdr_size = FALIGN * 2
    out = bytearray(hdr_size)
    raw_ptr = hdr_size
    shdrs = bytearray()
    for name, rva, vs, raw, ch in secs:
        rsz = align(len(raw), FALIGN)
        shdrs += struct.pack('<8sIIIIIIHHI', name, vs, rva, rsz, raw_ptr if rsz else 0, 0, 0, 0, 0, ch)
        if rsz:
            out += raw + bytes(rsz - len(raw))
            raw_ptr += rsz
    size_image = align(r_rva + len(reloc), SALIGN)

    struct.pack_into('<HH', out, 0, 0x5a4d, 0)
    struct.pack_into('<I', out, 0x3c, 0x40)
    pe = 0x40
    struct.pack_into('<4sHHIIIHH', out, pe, b'PE\0\0', 0x1c0, len(secs), 0, 0, 0, 224, 0x0102)
    opt = pe + 24
    dirs = [(0, 0)] * 16
    dirs[1] = (i_rva, len(idata))
    dirs[5] = (r_rva, len(reloc))
    struct.pack_into('<HBBIIIIIIIIIHHHHHHIIIIHHIIIIII', out, opt,
                     0x10b, 6, 0, len(t_raw), len(d_raw), 0, entry - BASE, t_rva, d_rva,
                     BASE, SALIGN, FALIGN, 5, 0, 0, 0, 5, 0, 0, size_image, hdr_size, 0,
                     9, 0, 0x10000, 0x1000, 0x100000, 0x1000, 0, 16)
    for k, (a, s) in enumerate(dirs):
        struct.pack_into('<II', out, opt + 96 + 8 * k, a, s)
    out[opt + 224:opt + 224 + len(shdrs)] = shdrs
    open(out_path, 'wb').write(out)
    print('%s: %d imports, %d relocs, image %#x' % (out_path, n, len(fix), size_image))


if __name__ == '__main__':
    main(*sys.argv[1:4])

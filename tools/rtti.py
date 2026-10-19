#!/usr/bin/env python3
"""Recover MSVC RTTI from an XBE: classes, base classes and vtables.

  rtti.py <xbe> [--json out.json]

Layout (32-bit MSVC):
  TypeDescriptor        { void *pVFTable; void *spare; char name[]; }      name = ".?AV<Class>@@"
  CompleteObjectLocator { u32 sig; u32 offset; u32 cdOffset; TypeDescriptor *td; ClassHierarchyDescriptor *chd; }
  ClassHierarchyDesc    { u32 sig; u32 attrs; u32 numBases; BaseClassDescriptor **bases; }
  BaseClassDescriptor   { TypeDescriptor *td; u32 numContained; PMD where(3*u32); u32 attrs; }
  vtable[-1] = &CompleteObjectLocator
"""
import json
import re
import struct
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from xbe import Xbe


def demangle_td(name):
    # Only the simple, non-template ".?AVFoo@@" / ".?AVInner@Outer@@" forms are
    # pretty-printed; anything else is returned raw.
    m = re.fullmatch(r'\.\?A[VU]([A-Za-z0-9_@]+?)@@', name)
    if not m:
        return name
    return '::'.join(reversed(m.group(1).split('@')))


def recover(x):
    img = x.flat_image()
    base = x.base
    u32 = lambda va: struct.unpack_from('<I', img, va - base)[0]
    inimg = lambda va: base <= va < base + len(img) - 4
    rdata = next(s for s in x.sections if s.name == '.rdata')
    text = next(s for s in x.sections if s.name == '.text')

    tds = {}
    for m in re.finditer(rb'\.\?A[VU][A-Za-z0-9_@?$]+@@\0', img):
        va = base + m.start() - 8
        tds[va] = m.group()[:-1].decode()

    def bases_of(chd):
        n, arr = u32(chd + 8), u32(chd + 12)
        out = []
        for i in range(1, n):  # entry 0 is the class itself
            bcd = u32(arr + 4 * i)
            out.append({'name': demangle_td(tds.get(u32(bcd), '?')), 'mdisp': u32(bcd + 8)})
        return out

    # Locate COLs, then the vtable whose [-1] slot points at each COL.
    cols = {}
    for off in range(rdata.va - base, rdata.va - base + rdata.vsize - 20, 4):
        va = base + off
        sig, offset, cd, td, chd = struct.unpack_from('<5I', img, off)
        if sig == 0 and td in tds and inimg(chd):
            cols[va] = (offset, td, chd)

    ptrs = {}
    for off in range(rdata.va - base, rdata.va - base + rdata.vsize - 4, 4):
        v = struct.unpack_from('<I', img, off)[0]
        if v in cols:
            ptrs.setdefault(v, []).append(base + off + 4)

    classes = {}
    for col, (offset, td, chd) in cols.items():
        name = demangle_td(tds[td])
        c = classes.setdefault(name, {'name': name, 'mangled': tds[td], 'typedesc': td,
                                       'bases': bases_of(chd), 'vtables': []})
        for vt in ptrs.get(col, []):
            slots = []
            a = vt
            while True:
                f = u32(a)
                if not (text.va <= f < text.va + text.vsize):
                    break
                slots.append(f)
                a += 4
            c['vtables'].append({'address': vt, 'offset': offset, 'slots': slots})
    for c in classes.values():
        c['vtables'].sort(key=lambda v: v['offset'])
    return sorted(classes.values(), key=lambda c: c['name'])



def main():
    x = Xbe(sys.argv[1])
    classes = recover(x)
    if '--json' in sys.argv:
        json.dump(classes, open(sys.argv[sys.argv.index('--json') + 1], 'w'), indent=1)
    for c in classes:
        bases = ', '.join(b['name'] for b in c['bases'])
        vts = ' '.join(f"{v['address']:#x}+{v['offset']}[{len(v['slots'])}]" for v in c['vtables'])
        print(f"{c['name']:32} : {bases:40} {vts}")


if __name__ == '__main__':
    main()

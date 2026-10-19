#!/usr/bin/env python3
"""Disassemble functions of Default.xbe with symbol names.

  disasm.py <addr|name> [...]      function(s) by start address or symbol name

Uses build/export/functions.tsv for function bounds and names, and
symbols/*.txt for extra labels.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32  # noqa: E402

from xbe import Xbe  # noqa: E402


def load():
    funcs, names = {}, {}
    with open(os.path.join(ROOT, 'build/export/functions.tsv')) as f:
        next(f)
        for line in f:
            a, size, _, name = line.split('\t')[:4]
            funcs[int(a, 16)] = (int(size), name)
            names[int(a, 16)] = name
    for fn in ('rtti.txt', 'names.txt'):
        with open(os.path.join(ROOT, 'symbols', fn)) as f:
            for line in f:
                if line.strip() and not line.startswith('#'):
                    a, n = line.split()[:2]
                    names[int(a, 16)] = n
    return funcs, names


def main():
    x = Xbe(os.path.join(ROOT, 'UnleashX_0_39_0528A/Default.xbe'))
    img = x.flat_image()
    funcs, names = load()
    by_name = {v: k for k, v in names.items()}
    md = Cs(CS_ARCH_X86, CS_MODE_32)

    def label(v):
        if v in names:
            return names[v]
        if x.base <= v < x.base + len(img) - 4:
            # string preview for data references
            o = v - x.base
            s = img[o:o + 48].split(b'\0')[0]
            if len(s) >= 3 and all(32 <= c < 127 for c in s):
                return repr(s.decode())
        return None

    for arg in sys.argv[1:]:
        a = by_name[arg] if arg in by_name else int(arg, 16)
        size = funcs.get(a, (0x100, None))[0]
        print(f'; ---- {names.get(a, hex(a))} @ {a:08x} ({size} bytes)')
        for ins in md.disasm(img[a - x.base:a - x.base + size], a):
            ops = ins.op_str
            notes = []
            for tok in ops.replace('[', ' ').replace(']', ' ').replace(',', ' ').replace('+', ' ').split():
                if tok.startswith('0x'):
                    v = int(tok, 16)
                    lab = label(v)
                    if lab:
                        notes.append(lab)
            note = ('    ; ' + ', '.join(notes)) if notes else ''
            print(f'  {ins.address:08x}  {ins.mnemonic:6} {ops}{note}')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Find instructions in .text that reference an address (as an immediate or a
memory displacement), grouped by containing function.

  xrefs.py <addr> [--write]       --write: only instructions that store to it
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32  # noqa: E402
from capstone.x86 import X86_OP_IMM, X86_OP_MEM  # noqa: E402

from xbe import Xbe  # noqa: E402


def load_funcs():
    funcs = []
    with open(os.path.join(ROOT, 'build/export/functions.tsv')) as f:
        next(f)
        for line in f:
            a, size, block, name = line.split('\t')[:4]
            funcs.append((int(a, 16), int(size), name))
    funcs.sort()
    return funcs


def main():
    target = int(sys.argv[1], 16)
    only_writes = '--write' in sys.argv
    x = Xbe(os.path.join(ROOT, 'UnleashX_0_39_0528A/Default.xbe'))
    img = x.flat_image()
    funcs = load_funcs()
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    for addr, size, name in funcs:
        if size <= 0:
            continue
        code = img[addr - x.base:addr - x.base + size]
        for ins in md.disasm(code, addr):
            hit = False
            for i, op in enumerate(ins.operands):
                if op.type == X86_OP_MEM and (op.mem.disp & 0xFFFFFFFF) == target and op.mem.base == 0:
                    hit = not only_writes or (i == 0 and ins.mnemonic.startswith(('mov', 'and', 'or', 'add', 'sub', 'inc', 'dec')))
                elif op.type == X86_OP_IMM and (op.imm & 0xFFFFFFFF) == target and not only_writes:
                    hit = True
            if hit:
                print(f'{ins.address:08x} {name:50} {ins.mnemonic} {ins.op_str}')


if __name__ == '__main__':
    main()

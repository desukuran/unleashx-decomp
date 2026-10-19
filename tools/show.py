#!/usr/bin/env python3
"""Print raw Ghidra decompiler output from build/export/decomp.

  show.py <addr|name> [...]        single functions
  show.py <start>-<end>            every function whose address is in [start, end)
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    funcs = []
    for path in sorted(glob.glob(os.path.join(ROOT, 'build/export/decomp/*.c'))):
        for part in re.split(r'(?m)^// ==== ', open(path).read())[1:]:
            name, addr = re.match(r'(.+?) @ ([0-9a-f]+)', part).groups()
            funcs.append((int(addr, 16), name, '// ==== ' + part.strip()))
    for arg in sys.argv[1:]:
        if '-' in arg:
            lo, hi = (int(v, 16) for v in arg.split('-'))
            sel = [f for f in funcs if lo <= f[0] < hi]
        else:
            sel = [f for f in funcs if f[1] == arg or (re.fullmatch(r'[0-9a-fA-F]+', arg) and f[0] == int(arg, 16))]
        for f in sorted(sel):
            print(f[2] + '\n')


if __name__ == '__main__':
    main()

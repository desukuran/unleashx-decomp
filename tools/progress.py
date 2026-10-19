#!/usr/bin/env python3
"""Report decomp progress: functions named in symbols/ and functions with
source in src/ (found via '// 0x........' address comments)."""
import glob
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Code ranges in .text that are UnleashX (and its bundled third-party code)
# rather than XAPI/CRT. Approximate; refine as library boundaries are found.
APP_RANGES = [(0x00011000, 0x00034000), (0x0006A400, 0x0013FD38)]


def main():
    funcs = {}
    with open(os.path.join(ROOT, 'build/export/functions.tsv')) as f:
        next(f)
        for line in f:
            a, size, block, name = line.split('\t')[:4]
            funcs[int(a, 16)] = (block, name, int(size))

    in_src = set()
    for path in glob.glob(os.path.join(ROOT, 'src/**/*.cpp'), recursive=True) + \
            glob.glob(os.path.join(ROOT, 'src/**/*.h'), recursive=True):
        for line in open(path):
            if '//' in line and 'TODO: decompile' not in line:
                for m in re.finditer(r'0x([0-9A-Fa-f]{8})', line.split('//', 1)[1]):
                    in_src.add(int(m.group(1), 16))

    text = {a: v for a, v in funcs.items() if v[0] == '.text'}
    app = {a: v for a, v in text.items() if any(lo <= a < hi for lo, hi in APP_RANGES)}
    named = lambda d: [a for a, v in d.items() if not v[1].startswith(('FUN_', 'thunk_FUN_'))]
    done = [a for a in app if a in in_src]
    size = lambda d: sum(v[2] for v in d.values())

    print(f'.text functions        : {len(text)}')
    print(f'  named                : {len(named(text))}')
    print(f'app range functions    : {len(app)} ({size(app)} bytes)')
    print(f'  named                : {len(named(app))}')
    print(f'  in src/              : {len(done)} ({sum(app[a][2] for a in done)} bytes, '
          f'{100 * sum(app[a][2] for a in done) / max(1, size(app)):.1f}%)')


if __name__ == '__main__':
    main()

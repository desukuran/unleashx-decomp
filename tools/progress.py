#!/usr/bin/env python3
"""Report decomp progress: functions named in symbols/ and functions with
source in src/ (found via '// 0x........' address comments)."""
import glob
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Code ranges in .text that are UnleashX (and its bundled third-party code)
# rather than XAPI/CRT. Approximate; refine as library boundaries are found.
# Functions inside them that tools/libmatch.py found in the XDK libraries
# (STL instantiations, CRT helpers) don't count as app code.
APP_RANGES = [(0x00011000, 0x00034000), (0x0006A400, 0x0013FD38)]


def library_functions():
    path = os.path.join(ROOT, 'symbols/xdklib.txt')
    if not os.path.exists(path):
        return set()
    found = set()
    for line in open(path):
        fields = line.split()
        if len(fields) == 2 and not line.startswith('#'):
            found.add(int(fields[0], 16))
    return found


def main():
    funcs = {}
    with open(os.path.join(ROOT, 'build/export/functions.tsv')) as f:
        next(f)
        for line in f:
            a, size, block, name = line.split('\t')[:4]
            funcs[int(a, 16)] = (block, name, int(size))

    # Implemented: an address comment in a .cpp, or on a header line with an
    # inline body. Declared: only on a header declaration (interfaces, and
    # library code that is named but not reconstructed).
    in_src = set()
    declared = set()
    for path in glob.glob(os.path.join(ROOT, 'src/**/*.cpp'), recursive=True) + \
            glob.glob(os.path.join(ROOT, 'src/**/*.h'), recursive=True):
        header = path.endswith('.h')
        for line in open(path):
            if '//' in line and 'TODO: decompile' not in line:
                code, comment = line.split('//', 1)
                target = declared if header and '{' not in code else in_src
                for m in re.finditer(r'0x([0-9A-Fa-f]{8})', comment):
                    target.add(int(m.group(1), 16))
    declared -= in_src

    text = {a: v for a, v in funcs.items() if v[0] == '.text'}
    library = library_functions() - in_src
    in_ranges = {a: v for a, v in text.items() if any(lo <= a < hi for lo, hi in APP_RANGES)}
    app = {a: v for a, v in in_ranges.items() if a not in library}
    named = lambda d: [a for a, v in d.items() if not v[1].startswith(('FUN_', 'thunk_FUN_'))]
    done = [a for a in app if a in in_src]
    size = lambda d: sum(v[2] for v in d.values())

    print(f'.text functions        : {len(text)}')
    print(f'  named                : {len(named(text))}')
    print(f'app range functions    : {len(app)} ({size(app)} bytes; '
          f'{len(in_ranges) - len(app)} library functions left out)')
    print(f'  named                : {len(named(app))}')
    print(f'  in src/              : {len(done)} ({sum(app[a][2] for a in done)} bytes, '
          f'{100 * sum(app[a][2] for a in done) / max(1, size(app)):.1f}%)')
    decl = [a for a in app if a in declared]
    print(f'  declared only        : {len(decl)} ({sum(app[a][2] for a in decl)} bytes, '
          f'{100 * sum(app[a][2] for a in decl) / max(1, size(app)):.1f}%; '
          'interfaces and library code)')


if __name__ == '__main__':
    main()

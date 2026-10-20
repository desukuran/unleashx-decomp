#!/usr/bin/env python3
"""Generate symbols/thirdparty.txt: names for the bundled third-party code.

  gen_thirdparty_symbols.py [Default.xbe] [-o symbols/thirdparty.txt]

Two kinds of lines come out of it:

  * Real names, found by tools/srcmatch.py from each library's public
    source (see LIBRARIES for where to get it; it goes in the gitignored
    reference/thirdparty/ folder).
  * <library>_sub_<address> labels for the rest of a library's code: a
    function that is only ever reached from that library's functions. They
    say where code comes from, not what it is.

Names in symbols/names.txt that match a source function are used as
seeds, so hand-identified functions help the matcher along. Run
tools/apply.sh afterwards.
"""
import argparse
import glob
import os
import pickle
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import srcmatch  # noqa: E402

REF = os.path.join(ROOT, 'reference/thirdparty')

# name: (description, source globs under reference/thirdparty, ranges, alias seeds, name prefixes of its
# functions already named elsewhere)
LIBRARIES = [
    ('unrar', 'XBMC\'s UnrarXLib (unrar 3.x with the urarlib_* entry points); xbmc4xbox: lib/UnrarXLib '
              '(github.com/antonic901/xbmc4xbox-redux)',
     ['xbmc4xbox/lib/UnrarXLib/*.cpp', 'xbmc4xbox/lib/UnrarXLib/*.hpp'],
     [(0x6F900, 0x7F000), (0x136600, 0x139A00)], [],
     ('urarlib_', 'CmdExtract::', 'Archive::', 'File::', 'FindFile::', 'CommandData::', 'ErrorHandler::',
      'ScanTree::', 'Unpack::', 'ComprDataIO::', 'RawRead::', 'StringList::', 'Array::')),
    ('xunzip', 'Hans Dietrich\'s XUnzip (Lucian Wischik\'s unzip, zlib inflate); XUnzip.cpp as shipped in '
               'Valve\'s SDK (github.com/alliedmodders/hl2sdk, episode1: public/XUnzip.cpp)',
     ['zip_utils/XUnzip.cpp'], [(0x8B300, 0x8EC00)],
     [(0x8E753, 'OpenZipU'), (0x8E725, 'GetZipItemA'), (0x8EBD8, 'FindZipItemA'), (0x8E5F2, 'CloseZipU'),
      (0x8EAE0, 'UnzipItem')], ('TUnzip::',)),
    ('xzip', 'Hans Dietrich\'s XZip (Lucian Wischik\'s zip, Info-ZIP deflate); github.com/ValveSoftware/'
             'source-sdk-2013: src/public/XZip.cpp',
     ['zip_utils/XZip.cpp'], [(0x87A00, 0x8B300)], [(0x88BB1, 'CreateZipZ')], ('TZip::', 'ZipAdd', 'CloseZipZ')),
    ('xkutils', 'Team Assembly\'s XKUtils; xbmc4xbox: xbmc/platform/xbox/XK*',
     ['xbmc4xbox/xbmc/platform/xbox/XK*.cpp', 'xbmc4xbox/xbmc/platform/xbox/XK*.h'], [(0x8F900, 0x93000)], [],
     ('XKUtils', 'XKEEPROM', 'XKHDD', 'XKRC4', 'XKCRC', 'XKSHA1', 'XKGeneral')),
    ('iso9660', 'XBMC\'s iso9660 reader and CIoSupport; xbmc4xbox: xbmc/filesystem/iso9660.*, '
                'xbmc/platform/xbox/storage/IoSupport.cpp',
     ['xbmc4xbox/xbmc/filesystem/iso9660.cpp', 'xbmc4xbox/xbmc/filesystem/iso9660.h',
      'xbmc4xbox/xbmc/platform/xbox/storage/IoSupport.cpp'], [(0xB1000, 0xB3500)], [],
     ('iso9660::', 'CIoSupport::', 'MD5')),
    # Attribution only (no source to match).
    ('fat32', 'the FAT32 USB driver (src/thirdparty/Fat32.h)', [], [(0x6A400, 0x6E184)], [], ('Fat32',)),
    ('xdksample', 'the XDK samples framework (XBApp, XBFont, XBMesh, XBMemUnit, XBStorageDevice, XBSound, '
                  'XBUtil, XBInput); the XDK\'s Samples/Common/Src, a later version than the one linked in '
                  '(in reference/Samples)',
     ['../Samples/Common/Src/xb*.cpp', '../Samples/Common/Include/xb*.h'], [(0x93000, 0x9A000)], [],
     ('CXBApplicationEx::', 'CXBMesh::', 'CXBMemUnit::', 'CXBStorageDevice::', 'CXBFont::', 'CSound::',
      'XBUtil_', 'XBInput_', 'CXBPackedResource::', 'CXBStopWatch::', 'CWaveFile::', 'CRiffChunk::',
      'XBSound_', 'XBResource_')),
]

APP_RANGES = [(0x11000, 0x34000), (0x6A400, 0x13FD38)]


def address_comments():
    """Functions with source (address comments in src/)."""
    done = set()
    for path in glob.glob(os.path.join(ROOT, 'src/**/*.cpp'), recursive=True) + \
            glob.glob(os.path.join(ROOT, 'src/**/*.h'), recursive=True):
        for line in open(path):
            if '//' in line and 'TODO: decompile' not in line:
                for m in re.finditer(r'0x([0-9A-Fa-f]{8})', line.split('//', 1)[1]):
                    done.add(int(m.group(1), 16))
    return done


def symbol_file(name):
    out = {}
    path = os.path.join(ROOT, 'symbols', name)
    for line in open(path):
        f = line.split()
        if len(f) >= 2 and not line.startswith('#'):
            out[int(f[0], 16)] = f[1]
    return out


def call_graph(xbe_path):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    from capstone.x86 import X86_OP_IMM
    from xbe import Xbe
    x = Xbe(xbe_path)
    img = x.flat_image()
    funcs = []
    for line in list(open(os.path.join(ROOT, 'build/export/functions.tsv')))[1:]:
        f = line.rstrip('\n').split('\t')
        funcs.append((int(f[0], 16), int(f[1]), f[3]))
    starts = {a for a, s, n in funcs}
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    callers = {}
    for a, s, n in funcs:
        if s <= 0 or not any(lo <= a < hi for lo, hi in APP_RANGES):
            continue
        for ins in md.disasm(img[a - x.base:a - x.base + s], a):
            for op in ins.operands:
                if op.type == X86_OP_IMM and op.imm in starts and op.imm != a:
                    callers.setdefault(op.imm, set()).add(a)
    return funcs, callers


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('xbe', nargs='?', default=os.path.join(ROOT, 'UnleashX_0_39_0528A/Default.xbe'))
    ap.add_argument('-o', '--out', default=os.path.join(ROOT, 'symbols/thirdparty.txt'))
    args = ap.parse_args()

    curated = symbol_file('names.txt')
    # This generator's own earlier output (already applied) counts as unnamed.
    previous = set(symbol_file('thirdparty.txt')) if os.path.exists(args.out) else set()
    lib_names = {}            # address -> (library, name)
    reports = []
    for lib, desc, globs, ranges, aliases, prefixes in LIBRARIES:
        if not globs:
            continue
        paths = [os.path.join(REF, g) for g in globs]
        counts = {}
        for f in srcmatch.parse_sources(sorted({p for g in paths for p in glob.glob(g)})).values():
            counts[f['name']] = counts.get(f['name'], 0) + 1
        src_names = {n for n, c in counts.items() if c == 1}
        seeds = list(aliases)
        seeded = {a for a, n in seeds}
        for a, n in curated.items():
            if a not in seeded and any(lo <= a < hi for lo, hi in ranges) and n in src_names:
                seeds.append((a, n))
        found, report = srcmatch.match_library(args.xbe, paths, ranges, seeds, reset=previous)
        reports.append('%s: %s' % (lib, report))
        for a, n in found.items():
            if a not in curated:
                lib_names[a] = (lib, n)

    # Attribution: walk up from each unnamed function to the named ones.
    funcs, callers = call_graph(args.xbe)
    funcs = [(a, s_, 'FUN_%08x' % a if a in previous else n) for a, s_, n in funcs]
    names = {a: n for a, s, n in funcs}
    names.update(curated)
    names.update({a: n for a, (l, n) in lib_names.items()})
    library_code = symbol_file('xdklib.txt')
    done = address_comments()

    def library_of(a):
        if a in lib_names:
            return lib_names[a][0]
        n = names.get(a, '')
        for lib, desc, globs, ranges, aliases, prefixes in LIBRARIES:
            if any(lo <= a < hi for lo, hi in ranges) and (n.startswith(prefixes) or a in dict(aliases)):
                return lib
        return None

    def unnamed(a):
        return names.get(a, '').startswith(('FUN_', 'thunk_FUN_')) and a not in done and a not in library_code

    labels = {}
    for a, s, n in funcs:
        if not any(lo <= a < hi for lo, hi in APP_RANGES) or not unnamed(a):
            continue
        seen, roots, stack = {a}, set(), [a]
        while stack:
            c = stack.pop()
            for p in callers.get(c, ()):
                if p in seen:
                    continue
                seen.add(p)
                if unnamed(p):
                    stack.append(p)
                else:
                    roots.add(library_of(p))
        if len(roots) == 1 and None not in roots:
            labels[a] = next(iter(roots))

    # Functions nothing calls (dead code the linker kept) or that UnleashX
    # calls directly: inside a library's ranges and between two of its
    # functions, they're the library's too.
    owner = {a: l for a, (l, n) in lib_names.items()}
    owner.update(labels)
    for a, s_, n in funcs:
        lib = library_of(a)
        if lib:
            owner.setdefault(a, lib)
    ordered = sorted(a for a, s_, n in funcs if any(lo <= a < hi for lo, hi in APP_RANGES))
    for _ in range(3):
        for i, a in enumerate(ordered):
            if a in owner or not unnamed(a):
                continue
            prev = next((owner.get(b) for b in reversed(ordered[:i]) if b in owner or not unnamed(b)), None)
            nxt = next((owner.get(b) for b in ordered[i + 1:] if b in owner or not unnamed(b)), None)
            if prev and prev == nxt and any(lo <= a < hi for l, d, g, ranges, al, pf in LIBRARIES if l == prev
                                            for lo, hi in ranges):
                owner[a] = prev
                labels[a] = prev

    with open(args.out, 'w') as f:
        f.write('# Generated by tools/gen_thirdparty_symbols.py -- do not edit; put overrides in symbols/names.txt\n')
        f.write('# Bundled third-party code: names matched against the public source (tools/srcmatch.py), and\n')
        f.write('# <library>_sub_<address> labels for the rest of each library\'s code.\n')
        for lib, desc, globs, ranges, aliases, prefixes in LIBRARIES:
            named = sorted(a for a, (l, n) in lib_names.items() if l == lib)
            # Hand-named functions of the library (names.txt wins anyway); listed so
            # tools/progress.py counts them as library code, unless src/ has them.
            hand = sorted(a for a in curated if a not in lib_names and a not in done and library_of(a) == lib)
            subs = sorted(a for a, l in labels.items() if l == lib)
            f.write('#\n# %s: %s\n' % (lib, desc))
            for a in sorted(named + subs + hand):
                f.write('%08x %s\n' % (a, lib_names[a][1] if a in lib_names else
                                       curated[a] if a in curated else '%s_sub_%08x' % (lib, a)))
    for r in reports:
        print(r, file=sys.stderr)
    by_lib = {}
    for l in labels.values():
        by_lib[l] = by_lib.get(l, 0) + 1
    print('names %d, labels %s -> %s' % (len(lib_names), by_lib, args.out), file=sys.stderr)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Name statically linked library code by matching it against the XDK's .lib files.

  libmatch.py Default.xbe LIBDIR [-o symbols/xdklib.txt] [-v]

Default.xbe was linked against XDK 5933's static libraries (xapilib, libcmt,
libcpmt, d3d8, dsound, xacteng, ...). Each .lib is a COFF archive; every
function in it is compared byte for byte with the XBE, with the bytes its
relocations patch masked out. A function that matches in exactly one place
is named. Then the relocations of every matched function name what they
point at (callees, globals, vtables), which also names small functions that
are too short or too common to match on their own.

Only names are written out (an address and a name per line, like
symbols/xdk.txt); nothing from the libraries' code is kept. LIBDIR is a
local XDK copy (reference/5933/lib), which must never be committed.
"""
import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from msvcname import demangle  # noqa: E402
from xbe import Xbe  # noqa: E402

# The libraries that went into Default.xbe (its library versions list), with
# their 5933 file names. LTCG and debug variants are skipped.
LIBS = ['xapilib.lib', 'libcmt.lib', 'libcpmt.lib', 'd3d8.lib', 'd3dx8.lib', 'xgraphics.lib', 'dsound.lib',
        'xkbd.lib', 'xmv.lib', 'xnet.lib', 'xacteng.lib']

IMAGE_REL_I386_DIR32 = 0x06
IMAGE_REL_I386_DIR32NB = 0x07
IMAGE_REL_I386_SECTION = 0x0A
IMAGE_REL_I386_SECREL = 0x0B
IMAGE_REL_I386_REL32 = 0x14
RELOC_SIZE = {IMAGE_REL_I386_DIR32: 4, IMAGE_REL_I386_DIR32NB: 4, IMAGE_REL_I386_SECTION: 2,
              IMAGE_REL_I386_SECREL: 4, IMAGE_REL_I386_REL32: 4}

IMAGE_SCN_CNT_CODE = 0x20
IMAGE_SCN_LNK_COMDAT = 0x1000
SYM_CLASS_EXTERNAL = 2
SYM_CLASS_STATIC = 3

# Shorter patterns match too much by chance; they're named from callers.
MIN_PATTERN = 12
MIN_FIXED = 10
# A pattern found in more places than this isn't worth sorting out.
MAX_SHARED_HITS = 512


class Function:
    __slots__ = ('lib', 'obj', 'name', 'section', 'code', 'mask', 'relocs', 'external', 'is_data')

    def __init__(self, lib, obj, name, section, code, mask, relocs, external, is_data=False):
        self.lib, self.obj, self.name, self.section = lib, obj, name, section
        self.is_data = is_data   # a table in a code section (not typed as a function)
        self.code, self.mask = code, mask        # mask: 1 where the byte is fixed
        self.relocs = relocs                      # [(offset, type, target name, target is code)]
        self.external = external


def read_archive(path):
    """(member name, data) for each object in a COFF archive."""
    d = open(path, 'rb').read()
    if d[:8] != b'!<arch>\n':
        raise ValueError(path + ': not an archive')
    pos = 8
    longnames = b''
    while pos + 60 <= len(d):
        header = d[pos:pos + 60]
        name = header[:16].decode('latin-1').rstrip()
        size = int(header[48:58].decode().strip())
        data = d[pos + 60:pos + 60 + size]
        pos += 60 + size + (size & 1)
        if name == '/' or name == '/SYM64/':
            continue
        if name == '//':
            longnames = data
            continue
        if name.startswith('/') and name[1:].isdigit():
            o = int(name[1:])
            name = longnames[o:longnames.index(b'\0', o)].decode('latin-1')
        yield name.rstrip('/'), data


def parse_object(lib, objname, d):
    """The functions of one COFF object."""
    if len(d) < 20:
        return []
    machine, nsec, _, symptr, nsym, optsize, _ = struct.unpack_from('<HHIIIHH', d, 0)
    if machine != 0x14C:
        return []      # import or LTCG (IL) objects
    strtab = symptr + nsym * 18

    def symname(raw):
        if raw[:4] == b'\0\0\0\0':
            o = strtab + struct.unpack_from('<I', raw, 4)[0]
            return d[o:d.index(b'\0', o)].decode('latin-1')
        return raw.rstrip(b'\0').decode('latin-1')

    symbols = []
    i = 0
    while i < nsym:
        o = symptr + i * 18
        raw = d[o:o + 8]
        value, secnum, stype, sclass, naux = struct.unpack_from('<IhHBB', d, o + 8)
        symbols.append((symname(raw), value, secnum, stype, sclass, naux))
        for _ in range(naux):
            symbols.append(None)
        i += 1 + naux

    sections = []
    for s in range(nsec):
        o = 20 + optsize + s * 40
        name, vsize, vaddr, rsize, rptr, relptr, _, nrel, _, chars = struct.unpack_from('<8sIIIIIIHHI', d, o)
        name = name.rstrip(b'\0').decode('latin-1')
        if name.startswith('/') and name[1:].isdigit():
            o = strtab + int(name[1:])
            name = d[o:d.index(b'\0', o)].decode('latin-1')
        sections.append((name, rsize, rptr, relptr, nrel, chars))

    functions = []
    for index, (sname, rsize, rptr, relptr, nrel, chars) in enumerate(sections, 1):
        if not chars & IMAGE_SCN_CNT_CODE or rsize == 0 or rptr == 0:
            continue
        code = d[rptr:rptr + rsize]
        mask = bytearray(b'\1' * rsize)
        relocs = []
        for r in range(nrel):
            vaddr, symidx, rtype = struct.unpack_from('<IIH', d, relptr + r * 10)
            size = RELOC_SIZE.get(rtype, 4)
            mask[vaddr:vaddr + size] = b'\0' * size
            target = symbols[symidx]
            if target is None:
                continue
            tname, tvalue, tsec, ttype, tclass, _ = target
            # Section-relative references (static data, switch tables) have
            # no useful name.
            if tclass == SYM_CLASS_STATIC and (tname.startswith('.') or (0 < tsec <= len(sections)
                                                                         and tname == sections[tsec - 1][0])):
                tname = None
            tcode = 0 < tsec <= len(sections) and bool(sections[tsec - 1][5] & IMAGE_SCN_CNT_CODE)
            relocs.append((vaddr, rtype, tname, tcode or (ttype >> 4) == 2))
        # The functions defined in this section.
        starts = []
        for sym in symbols:
            if sym is None:
                continue
            name, value, secnum, stype, sclass, naux = sym
            if secnum != index or sclass not in (SYM_CLASS_EXTERNAL, SYM_CLASS_STATIC):
                continue
            # The section's own symbol (named like it, e.g. "XGRPH" for
            # code in #pragma code_seg sections).
            if sclass == SYM_CLASS_STATIC and naux and name == sname:
                continue
            if name.startswith('.') or name.startswith('$'):
                continue
            starts.append((value, name, sclass == SYM_CLASS_EXTERNAL, (stype >> 4) != 2))
        starts.sort()
        for k, (value, name, external, is_data) in enumerate(starts):
            end = starts[k + 1][0] if k + 1 < len(starts) else rsize
            if end <= value:
                continue
            frelocs = [(o - value, t, n, c) for (o, t, n, c) in relocs if value <= o < end]
            functions.append(Function(lib, objname, name, xbe_section(sname), code[value:end], bytes(mask[value:end]),
                                      frelocs, external, is_data))
    return functions


def label(name):
    """A demangled name as a Ghidra label: "::" inside template arguments
    becomes "." so only the real scopes turn into namespaces."""
    out, depth, i = [], 0, 0
    name = demangle(name)
    while i < len(name):
        c = name[i]
        depth += (c == '<') - (c == '>')
        if depth > 0 and name.startswith('::', i):
            out.append('.')
            i += 2
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def xbe_section(name):
    """The XBE section a library code section is linked into."""
    if name.startswith('.text'):
        return '.text'
    if name == '.XPPCODE':
        return 'XPP'
    return name.split('$')[0]


def anchor(fn):
    """(offset, bytes) of the longest fixed run, at most 16 bytes of it."""
    best = (0, 0)
    start = None
    for i, b in enumerate(fn.mask + b'\0'):
        if b and start is None:
            start = i
        elif not b and start is not None:
            if i - start > best[1] - best[0]:
                best = (start, i)
            start = None
    s, e = best
    return s, fn.code[s:min(e, s + 16)]


def matches_at(img, pos, fn):
    if pos < 0 or pos + len(fn.code) > len(img):
        return False
    code, mask = fn.code, fn.mask
    i, n = 0, len(code)
    while i < n:
        if mask[i]:
            j = i
            while j < n and mask[j]:
                j += 1
            if img[pos + i:pos + j] != code[i:j]:
                return False
            i = j
        else:
            i += 1
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('xbe')
    ap.add_argument('libdir')
    ap.add_argument('-o', '--out', default='symbols/xdklib.txt')
    ap.add_argument('-v', '--verbose', action='store_true')
    args = ap.parse_args()

    x = Xbe(args.xbe)
    img = x.flat_image()
    base = x.base
    code_ranges = [(s.va - base, s.va - base + s.vsize) for s in x.sections if s.flags & 4]  # executable
    section_ranges = {s.name: (s.va - base, s.va - base + s.vsize) for s in x.sections}

    def in_code(off):
        return any(a <= off < b for a, b in code_ranges)

    def in_section(off, name):
        a, b = section_ranges.get(name, (0, 0))
        return a <= off < b

    functions = []
    for lib in LIBS:
        path = os.path.join(args.libdir, lib)
        if not os.path.exists(path):
            print('missing', path, file=sys.stderr)
            continue
        n = 0
        for objname, data in read_archive(path):
            fs = parse_object(lib, objname, data)
            functions.extend(fs)
            n += len(fs)
        print(f'{lib}: {n} functions', file=sys.stderr)

    defined_in = {}
    by_name = {}
    for fn in functions:
        defined_in.setdefault(fn.name, set()).add(fn.section)
        by_name.setdefault(fn.name, []).append(fn)

    def reloc_target(start, fn, ro, rtype):
        """Where a relocated field points, less the addend kept in the library's bytes."""
        site = start + ro
        addend = struct.unpack_from('<i', fn.code, ro)[0] if ro + 4 <= len(fn.code) else 0
        if rtype == IMAGE_REL_I386_REL32:
            return (site + 4 + struct.unpack_from('<i', img, site)[0] - addend) & 0xFFFFFFFF
        if rtype == IMAGE_REL_I386_DIR32:
            return (struct.unpack_from('<I', img, site)[0] - addend - base) & 0xFFFFFFFF
        return None

    def calls_fit(start, fn):
        """Every call to a library function lands in that function's section."""
        for (ro, rtype, tname, tcode) in fn.relocs:
            if rtype != IMAGE_REL_I386_REL32 or tname not in defined_in:
                continue
            target = reloc_target(start, fn, ro, rtype)
            if not any(in_section(target, h) for h in defined_in[tname]):
                return False
        return True

    # Identical code under several names matches the same places; group it.
    # (With their call targets masked, many small wrappers look alike.)
    groups = {}
    for fn in functions:
        if len(fn.code) < MIN_PATTERN or sum(fn.mask) < MIN_FIXED:
            continue
        groups.setdefault((fn.code, fn.mask, fn.section), []).append(fn)

    pending = {}     # image offset -> the functions whose code matched there
    shared = set()   # offsets whose pattern matched in several places
    ambiguous = 0
    for key, fns in groups.items():
        fn = fns[0]
        aoff, abytes = anchor(fn)
        hits = []
        p = img.find(abytes)
        while p != -1 and len(hits) <= MAX_SHARED_HITS:
            start = p - aoff
            if in_section(start, fn.section) and matches_at(img, start, fn) and calls_fit(start, fn):
                hits.append(start)
            p = img.find(abytes, p + 1)
        if not hits or len(hits) > MAX_SHARED_HITS:
            ambiguous += len(hits) > 1
            continue
        for hit in hits:
            pending.setdefault(hit, []).extend(fns)
            if len(hits) > 1:
                shared.add(hit)

    named = {}       # image offset -> (decorated name, lib, how)
    matched = {}     # image offset -> Function whose relocations apply there
    data_names = {}

    def pick(fns):
        ext = [f for f in fns if f.external] or fns
        return sorted(ext, key=lambda f: f.name)[0]

    def call_score(off, fn):
        """None if a relocation contradicts what's named; else how many agree."""
        score = 0
        for (ro, rtype, tname, tcode) in fn.relocs:
            if rtype != IMAGE_REL_I386_REL32 or not tname:
                continue
            target = reloc_target(off, fn, ro, rtype)
            if target in named:
                if named[target][0] != tname:
                    return None
                score += 1
        return score

    def resolve():
        changed = 0
        for off, fns in list(pending.items()):
            if off in named:
                del pending[off]
                continue
            if len({f.name for f in fns}) > 1 or off in shared:
                scored = [(call_score(off, f), f) for f in fns]
                scored = [(sc, f) for sc, f in scored if sc is not None]
                best = max((sc for sc, f in scored), default=0)
                # A shared pattern needs a call to tell which place is which.
                if off in shared and best == 0:
                    continue
                fns = [f for sc, f in scored if sc == best] if best > 0 else fns
                if len({f.name for f in fns}) > 1:
                    continue
            fn = pick(fns)
            del pending[off]
            named[off] = (fn.name, fn.lib, 'match')
            matched[off] = fn
            changed += 1
        return changed

    def propagate():
        """Name what the matched functions' relocations point at."""
        changed = 0
        for off, fn in list(matched.items()):
            for (ro, rtype, tname, tcode) in fn.relocs:
                if not tname or tname.startswith('$') or tname.startswith('??_C@'):
                    continue
                target = reloc_target(off, fn, ro, rtype)
                if target is None or not 0 <= target < len(img):
                    continue
                if tcode and in_code(target):
                    homes = defined_in.get(tname)
                    if homes and not any(in_section(target, h) for h in homes):
                        continue
                    if target in named:
                        continue
                    named[target] = (tname, fn.lib, 'reloc')
                    pending.pop(target, None)
                    changed += 1
                    # Its own relocations, if the library has its code.
                    for cand in by_name.get(tname, []):
                        if cand.code and matches_at(img, target, cand):
                            matched[target] = cand
                            break
                elif not tcode and target not in data_names and target not in named:
                    data_names[target] = (tname, fn.lib)
        return changed

    def short_functions():
        """Wrappers too short to match alone: one place in their section with
        the same bytes and every call landing on the callee already named."""
        changed = 0
        for fn in functions:
            if fn.name in named_names or not fn.code or len(fn.code) >= MIN_PATTERN and sum(fn.mask) >= MIN_FIXED:
                continue
            calls = [r for r in fn.relocs if r[1] == IMAGE_REL_I386_REL32 and r[2]]
            if len(fn.code) < 5:
                continue
            if not calls:
                # No relocations at all (small getters): the exact bytes,
                # found only once in the section.
                if fn.relocs or len(fn.code) < 10:
                    continue
                a, b = section_ranges.get(fn.section, (0, 0))
                p = img.find(fn.code, a, b)
                if p != -1 and img.find(fn.code, p + 1, b) == -1 and p not in named:
                    named[p] = (fn.name, fn.lib, 'short')
                    named_names.add(fn.name)
                    changed += 1
                continue
            a, b = section_ranges.get(fn.section, (0, 0))
            aoff, abytes = anchor(fn)
            if not abytes:
                continue
            hits = []
            p = img.find(abytes, a)
            while p != -1 and p < b and len(hits) < 2:
                start = p - aoff
                if start not in named and matches_at(img, start, fn) and \
                        all(named.get(reloc_target(start, fn, ro, rt), ('',))[0] == tn for ro, rt, tn, tc in calls):
                    hits.append(start)
                p = img.find(abytes, p + 1)
            if len(hits) == 1:
                named[hits[0]] = (fn.name, fn.lib, 'short')
                named_names.add(fn.name)
                matched[hits[0]] = fn
                changed += 1
        return changed

    named_names = set()
    while True:
        changed = resolve() + propagate()
        named_names = {v[0] for v in named.values()}
        changed += short_functions()
        if not changed:
            break
    # Groups no call tells apart: true aliases (identical calls too) take
    # one of their names; the rest stay unnamed.
    unresolved = 0
    for off, fns in pending.items():
        if off in shared:
            continue
        calls = {tuple(r[2] for r in f.relocs) for f in fns}
        if len(calls) == 1:
            fn = pick(fns)
            named[off] = (fn.name, fn.lib, 'alias ' + '/'.join(sorted({demangle(f.name) for f in fns})))
        else:
            unresolved += 1
            if args.verbose:
                print(f'unresolved {off + base:08x}: ' + ' / '.join(sorted({demangle(f.name) for f in fns})),
                      file=sys.stderr)

    # Names only ever seen as tables in code sections (memcpy's jump tables).
    data_typed = {n for n, fs in by_name.items() if all(f.is_data for f in fs)}
    with open(args.out, 'w') as f:
        f.write('# Generated by tools/libmatch.py from the XDK 5933 libraries -- do not edit; '
                'put overrides in symbols/names.txt\n')
        for off in sorted(named):
            kind = ' data' if named[off][0] in data_typed else ''
            f.write(f'{off + base:08x} {label(named[off][0])}{kind}\n')
        for off in sorted(data_names):
            f.write(f'{off + base:08x} {label(data_names[off][0])} data\n')
    print(f'{len(named)} functions ({sum(1 for v in named.values() if v[2] != "reloc")} by match), '
          f'{len(data_names)} data, {ambiguous} patterns found in several places, '
          f'{unresolved} look-alikes left unnamed -> {args.out}', file=sys.stderr)
    if args.verbose:
        for off in sorted(named):
            print(f'{off + base:08x} {named[off][1]:14} {named[off][2].split()[0]:6} {demangle(named[off][0])}')


if __name__ == '__main__':
    main()

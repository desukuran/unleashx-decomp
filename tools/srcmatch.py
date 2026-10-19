#!/usr/bin/env python3
"""Name bundled third-party code by matching it against its public source.

  srcmatch.py Default.xbe --src 'reference/thirdparty/zip_utils/unzip.cpp' \\
      --range 8b300-8ec00 [--range ...] [-o out.txt] [-v]

The third-party libraries in Default.xbe (unrar, XZip/XUnzip, XKUtils,
iso9660, ...) were compiled by MSVC 7.1 from public source, so their bytes
can't be compared with anything, but their structure can:

  * a string literal used by exactly one source function and referenced by
    exactly one binary function in the range pairs them;
  * a matched pair calls its functions in the same order: between callees
    that are already known, runs of unknown callees of the same length are
    paired in order (inlined helpers just leave a run unpaired).

It starts from the names already in build/export/functions.tsv (from
symbols/names.txt etc.) that also name a source function, repeats both
steps until nothing changes, and drops anything contradictory. Only the
names are written out. The sources stay outside the repo (the gitignored
reference/ folder).
"""
import argparse
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

KEYWORDS = {'if', 'while', 'for', 'switch', 'return', 'sizeof', 'case', 'do', 'else', 'new', 'delete', 'catch',
            'defined', 'throw', '__declspec', '__asm', 'operator'}


def strip_source(text):
    """Comments removed, string literals replaced by placeholders; the
    literals are returned separately (in order)."""
    out, strings = [], []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith('//', i):
            j = text.find('\n', i)
            i = n if j == -1 else j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            i = n if j == -1 else j + 2
        elif c == '"' or (c == "'" and True):
            q = c
            j = i + 1
            while j < n and text[j] != q:
                j += 2 if text[j] == '\\' else 1
            lit = text[i + 1:j]
            if q == '"':
                out.append('"%d"' % len(strings))
                strings.append(lit)
            else:
                out.append("' '")
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out), strings


def norm_string(s):
    s = s.replace('\\n', '').replace('\\r', '').replace('\\t', '').replace('\\', '')
    s = re.sub(r'\s+', '', s).lower()
    return s[:24]


DEF = re.compile(r'((?:\w+\s*<[^<>;{}]*>\s*)?[~\w:]+)\s*\(([^;{}()]|\([^;{}()]*\))*\)\s*(const)?\s*(:[^{;]*)?\s*$', re.S)


STRING_MACRO = re.compile(r'^\s*#\s*define\s+(\w+)\s+(?:\w+\s*\(\s*)?"((?:[^"\\]|\\.)*)"', re.M)
SCOPE = re.compile(r'\b(class|struct|union)\s+(?:__declspec\(\w+\)\s*)?(\w+)[^;{}()]*$')
CALL = re.compile(r'(?:(\w+)\s*::\s*)?([A-Za-z_~]\w*)\s*\(')


def string_macros(paths):
    """#define NAME "text" (also NAME St("text")) from every file."""
    macros = {}
    for path in paths:
        for m in STRING_MACRO.finditer(open(path, encoding='latin-1').read()):
            macros.setdefault(m.group(1), m.group(2))
    return macros


def parse_sources(paths):
    """{key: {'name': qualified name, 'class': ..., 'strings': [...], 'calls': [(qualifier, name)], 'size'}}.
    Functions are read at file level and inside class bodies (inline
    members, qualified with the class)."""
    macros = string_macros(paths)
    funcs = {}
    for path in paths:
        text = open(path, encoding='latin-1').read()
        text = re.sub(r'^\s*#.*?(?<!\\)$', '', text, flags=re.M)   # preprocessor lines
        code, strings = strip_source(text)
        scopes = []       # ('class', name) / ('ns', None) / ('block', None)
        last = 0          # where the current declaration starts
        i = 0
        while i < len(code):
            c = code[i]
            in_decls = all(k != 'block' for k, _ in scopes)
            if c == '{':
                head = code[last:i]
                if in_decls and re.search(r'(namespace\s*\w*|extern\s*"\d+")\s*$', head):
                    scopes.append(('ns', None))
                    last = i + 1
                elif in_decls and SCOPE.search(head):
                    scopes.append(('class', SCOPE.search(head).group(2)))
                    last = i + 1
                elif in_decls and DEF.search(head) and DEF.search(head).group(1).split('::')[-1] not in KEYWORDS:
                    m = DEF.search(head)
                    d, j = 0, i
                    while j < len(code):
                        if code[j] == '{':
                            d += 1
                        elif code[j] == '}':
                            d -= 1
                            if d == 0:
                                break
                        j += 1
                    body = code[i:j + 1]
                    name = re.sub(r'\s*<[^<>]*>\s*', '', m.group(1))   # Array<T>::Push -> Array::Push
                    classes = [n for k, n in scopes if k == 'class']
                    if '::' not in name and classes:
                        name = '::'.join(classes) + '::' + name
                    cls = name.rsplit('::', 1)[0] if '::' in name else None
                    lits = [strings[int(k)] for k in re.findall(r'"(\d+)"', body)]
                    lits += [macros[w] for w in re.findall(r'\b([A-Z]\w+)\b', body) if w in macros]
                    calls = [(q, n) for q, n in CALL.findall(body) if n not in KEYWORDS]
                    key = name
                    k = 2
                    while key in funcs:          # overloads
                        key = '%s#%d' % (name, k)
                        k += 1
                    # Defined in a class body or declared inline: usually
                    # inlined, so left out of call sequences.
                    inline = bool(classes) or bool(re.search(r'\b(__forceinline|__inline|inline)\b', head))
                    funcs[key] = {'name': name, 'class': cls, 'strings': lits, 'calls': calls, 'size': len(body),
                                  'file': os.path.basename(path), 'inline': inline}
                    i = j + 1
                    last = i
                    continue
                else:
                    scopes.append(('block', None))
            elif c == '}':
                if scopes:
                    scopes.pop()
                last = i + 1
            elif c == ';' and in_decls:
                last = i + 1
            elif c == ':' and in_decls and re.search(r'\b(public|private|protected)\s*$', code[last:i]):
                last = i + 1
            i += 1
    return funcs


def load_binary(xbe_path, ranges, reset=()):
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    from capstone.x86 import X86_OP_IMM
    from xbe import Xbe
    x = Xbe(xbe_path)
    img = x.flat_image()
    funcs = []
    for line in list(open(os.path.join(ROOT, 'build/export/functions.tsv')))[1:]:
        f = line.rstrip('\n').split('\t')
        a = int(f[0], 16)
        funcs.append((a, int(f[1]), 'FUN_%08x' % a if a in reset else f[3], f[3]))
    names = {a: n for a, s, n, exported in funcs}
    starts = set(names)
    by_name = {}            # strings.tsv uses the exported names
    for a, s, n, exported in funcs:
        by_name.setdefault(exported, []).append(a)
    funcs = [(a, s, n) for a, s, n, exported in funcs]
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    scope = [a for a, s, n in funcs if any(lo <= a < hi for lo, hi in ranges)]
    calls = {}
    for a, s, n in funcs:
        if a not in scope:
            continue
        out = []
        for ins in md.disasm(img[a - x.base:a - x.base + s], a):
            if ins.mnemonic in ('call', 'jmp') and ins.operands and ins.operands[0].type == X86_OP_IMM:
                t = ins.operands[0].imm
                if t in starts and t != a:
                    out.append(t)
        calls[a] = out
    strings = {}
    for line in open(os.path.join(ROOT, 'build/export/strings.tsv')):
        f = line.rstrip('\n').split('\t')
        if len(f) < 3:
            continue
        for user in f[1].split(','):
            for a in by_name.get(user.strip(), []):
                strings.setdefault(a, []).append(f[2])
    sizes = {a: s for a, s, n in funcs}
    return names, sizes, scope, calls, strings


def distinct(seq):
    seen, out = set(), []
    for x in seq:
        if x not in seen:
            seen.add(x)
            out.append(x)
    return out


def match_library(xbe, src_globs, ranges, seeds=(), verbose=False, reset=()):
    """{binary address: source name} for the functions found, and a
    one-line report. seeds: (address, source name) pairs."""
    paths = sorted({p for g in src_globs for p in glob.glob(g, recursive=True)})
    src = parse_sources(paths)
    names, sizes, scope, calls, bstrings = load_binary(xbe, ranges, reset)
    scope_set = set(scope)

    short = {}
    for key, f in src.items():
        short.setdefault(f['name'].split('::')[-1], []).append(key)

    def resolve(call, caller):
        """A source call -> the source function, when it can be told: an
        explicit Class:: qualifier, a bare call to the caller's own class,
        or a name only one function has."""
        q, name = call
        keys = short.get(name, [])
        if q:
            keys = [k for k in keys if src[k]['class'] == q] or keys
        elif len(keys) > 1 and src[caller]['class']:
            own = [k for k in keys if src[k]['class'] == src[caller]['class']]
            keys = own or keys
        return keys[0] if len(keys) == 1 else None

    # Seeds: binary names that name exactly one source function.
    match = {}        # binary address -> source key
    how = {}
    for a in scope:
        n = names[a]
        if n.startswith(('FUN_', 'thunk_FUN_')):
            continue
        keys = [k for k, f in src.items() if f['name'] == n]
        if not keys:
            keys = [k for k in short.get(n.split('::')[-1], []) if '::' not in n or src[k]['name'] == n]
        if len(keys) == 1:
            match[a] = keys[0]
            how[a] = 'seed'
    for addr, name in seeds:
        keys = [k for k, f in src.items() if f['name'] == name]
        if len(keys) != 1:
            raise ValueError('seed %08x: %d source functions named %s' % (addr, len(keys), name))
        match[addr] = keys[0]
        how[addr] = 'seed'
    taken = set(match.values())
    known_binary = {a for a in scope if not names[a].startswith(('FUN_', 'thunk_FUN_'))}

    def plausible(a, key):
        """Machine code bytes against source characters (inlining and
        macros make this loose)."""
        chars = src[key]['size']
        if chars < 200:
            return sizes[a] < 400
        return 0.04 <= sizes[a] / float(chars) <= 1.5

    def free(a):
        return a in scope_set and a not in match and a not in known_binary

    def assign(pairs, reason):
        """Pairs that don't contradict each other or what's known."""
        by_b, by_s = {}, {}
        for b, s in pairs:
            by_b.setdefault(b, set()).add(s)
            by_s.setdefault(s, set()).add(b)
        n = 0
        for b, ss in by_b.items():
            if len(ss) != 1:
                continue
            s = next(iter(ss))
            if len(by_s[s]) != 1 or s in taken or not free(b):
                continue
            if reason != 'string' and not plausible(b, s):
                continue
            match[b] = s
            how[b] = reason
            taken.add(s)
            n += 1
        return n

    while True:
        added = 0
        # Strings used by one function on each side.
        s_users, b_users = {}, {}
        for key, f in src.items():
            for lit in f['strings']:
                k = norm_string(lit)
                if len(k) >= 6:
                    s_users.setdefault(k, set()).add(key)
        for a in scope:
            for text in bstrings.get(a, []):
                k = norm_string(text)
                if len(k) >= 6:
                    b_users.setdefault(k, set()).add(a)
        pairs = []
        for k, ss in s_users.items():
            bs = b_users.get(k)
            if bs and len(ss) == 1 and len(bs) == 1:
                pairs.append((next(iter(bs)), next(iter(ss))))
        added += assign(pairs, 'string')

        # Call order.
        pairs = []
        for b, s in list(match.items()):
            bseq = distinct(t for t in calls.get(b, []) if t in scope_set)
            sseq = distinct(r for r in (resolve(c, s) for c in src[s]['calls'])
                            if r is not None and r != s and not src[r]['inline'])
            # Tokens: a binary callee is known (its source key) or free.
            btok = [match.get(t, ('?', t)) if not (t in known_binary and t not in match) else ('x', t)
                    for t in bseq]
            stok = [k if k in taken else ('?', k) for k in sseq]
            # Fences: known keys present on both sides, in the same order.
            bpos = {t: i for i, t in enumerate(btok) if isinstance(t, str)}
            spos = {t: i for i, t in enumerate(stok) if isinstance(t, str)}
            common = [t for t in btok if isinstance(t, str) and t in spos]
            fences, last = [], -1
            for t in common:
                if spos[t] > last:
                    fences.append(t)
                    last = spos[t]
            bcuts = [-1] + [bpos[t] for t in fences] + [len(btok)]
            scuts = [-1] + [spos[t] for t in fences] + [len(stok)]
            for (b0, b1), (s0, s1) in zip(zip(bcuts, bcuts[1:]), zip(scuts, scuts[1:])):
                bfree = [t[1] for t in btok[b0 + 1:b1] if isinstance(t, tuple) and t[0] == '?']
                sfree = [t[1] for t in stok[s0 + 1:s1] if isinstance(t, tuple)]
                bother = [t for t in btok[b0 + 1:b1] if not (isinstance(t, tuple) and t[0] == '?')]
                if bfree and len(bfree) == len(sfree) and not bother:
                    pairs.extend(zip(bfree, sfree))
        added += assign(pairs, 'calls')

        # Callers, in any order: a free binary function's candidates are
        # the unmatched source functions every matched caller's source
        # calls, and the other way round; pair when both sides agree on
        # exactly each other (and the sizes are plausible).
        callers = {}
        for b2, ts in calls.items():
            for t in ts:
                callers.setdefault(t, set()).add(b2)
        s_callers = {}
        for key in src:
            for r in (resolve(c, key) for c in src[key]['calls']):
                if r is not None and r != key:
                    s_callers.setdefault(r, set()).add(key)
        # Every caller matched, and their sources share one unmatched
        # callee.
        pairs = []
        for b2 in scope:
            if not free(b2):
                continue
            cs = callers.get(b2, ())
            if not cs or any(c not in match for c in cs):
                continue
            cand = None
            for c in cs:
                options = {r for r in (resolve(x, match[c]) for x in src[match[c]]['calls'])
                           if r is not None and r not in taken}
                cand = options if cand is None else cand & options
            if cand is not None and len(cand) == 1:
                pairs.append((b2, next(iter(cand))))
        added += assign(pairs, 'callers')

        b_of = {v: k for k, v in match.items()}
        bcand, scand = {}, {}
        for b2 in scope:
            if not free(b2):
                continue
            cand = None
            for c in callers.get(b2, ()):
                if c not in match:
                    continue
                options = {r for r in (resolve(x, match[c]) for x in src[match[c]]['calls'])
                           if r is not None and r not in taken and plausible(b2, r)}
                cand = options if cand is None else cand & options
            if cand:
                bcand[b2] = cand
        for key in src:
            if key in taken:
                continue
            cand = None
            for c in s_callers.get(key, ()):
                if c not in b_of:
                    continue
                options = {t for t in calls.get(b_of[c], []) if free(t) and plausible(t, key)}
                cand = options if cand is None else cand & options
            if cand:
                scand[key] = cand
        pairs = []
        for b2, cand in bcand.items():
            if len(cand) == 1:
                key = next(iter(cand))
                if scand.get(key) == {b2}:
                    pairs.append((b2, key))
        added += assign(pairs, 'callers')
        if not added:
            break

    found = {b: src[s]['name'] for b, s in match.items() if how[b] != 'seed'}
    free_left = [a for a in scope if free(a)]
    report = ('%d source functions, %d binary functions in range (%d free); seeds %d, named %d '
              '(strings %d, calls %d), %d left' % (
                  len(src), len(scope), len([a for a in scope if a not in known_binary]),
                  sum(1 for v in how.values() if v == 'seed'), len(found),
                  sum(1 for v in how.values() if v == 'string'), sum(1 for v in how.values() if v == 'calls'),
                  len(free_left)))
    if verbose:
        for b in sorted(found):
            s = match[b]
            print('%08x %-7s %5d %6d %-14s %s' % (b, how[b], sizes[b], src[s]['size'], src[s]['file'],
                                                   src[s]['name']))
    return found, report


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('xbe')
    ap.add_argument('--src', action='append', required=True, help='source glob (repeatable)')
    ap.add_argument('--range', action='append', required=True, help='hex start-end (repeatable)')
    ap.add_argument('--seed', action='append', default=[],
                    help='hexaddr=source name: a known match whose binary name differs (e.g. a macro)')
    ap.add_argument('-o', '--out')
    ap.add_argument('-v', '--verbose', action='store_true')
    args = ap.parse_args()
    ranges = [tuple(int(v, 16) for v in r.split('-')) for r in args.range]
    seeds = [(int(a, 16), n) for a, n in (x.split('=') for x in args.seed)]
    found, report = match_library(args.xbe, args.src, ranges, seeds, args.verbose)
    if args.out:
        with open(args.out, 'w') as f:
            f.write(''.join('%08x %s\n' % (b, n) for b, n in sorted(found.items())))
    print(report, file=sys.stderr)


if __name__ == '__main__':
    main()

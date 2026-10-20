#!/usr/bin/env python3
"""Wraps arguments the XDK compiler rejects (error C2664 "cannot convert
parameter N from 'X' to 'T'") in XDK_CAST(T, ...). Reads MSVC logs."""
import re, sys, glob

ERR = re.compile(r"src\\(.+?)\((\d+)\) : error C2664: '([^']+)' : cannot convert parameter (\d+) from '[^']+' to '([^']+)'")

def split_args(s, start):
    """s[start] is '('; returns list of (begin, end) of top-level args and the close index."""
    depth = 0; args = []; b = start + 1; i = start
    while i < len(s):
        c = s[i]
        if c in '([{': depth += 1
        elif c in ')]}':
            depth -= 1
            if depth == 0:
                args.append((b, i)); return args, i
        elif c == ',' and depth == 1:
            args.append((b, i)); b = i + 1
        elif c == '"':
            i += 1
            while s[i] != '"': i += 2 if s[i] == '\\' else 1
        i += 1
    return None, None

fixes = {}
for log in sys.argv[1:]:
    for line in open(log, errors='replace'):
        m = ERR.search(line)
        if m:
            fixes.setdefault('src/' + m.group(1).replace('\\', '/'), []).append(
                (int(m.group(2)), m.group(3).split('::')[-1], int(m.group(4)), m.group(5).strip()))
for path, items in fixes.items():
    lines = open(path).read().split('\n')
    for lineno, func, n, target in sorted(set(items), key=lambda x: (-x[0], -x[2])):
        text = lines[lineno - 1]
        done = False
        for m in re.finditer(r'\b' + re.escape(func) + r'\s*\(', text):
            args, close = split_args(text, m.end() - 1)
            if not args or n > len(args): continue
            b, e = args[n - 1]
            arg = text[b:e]
            lead = len(arg) - len(arg.lstrip())
            text = text[:b + lead] + 'XDK_CAST(%s, %s)' % (target, arg.strip()) + text[e:]
            done = True
            break
        if done: lines[lineno - 1] = text
        else: print('not fixed:', path, lineno, func, n, target)
    open(path, 'w').write('\n'.join(lines))

#!/usr/bin/env python3
"""Name the STL template instantiations in Default.xbe (VC7.1 Dinkumware).

  stlname.py [-o symbols/stl.txt] [-v]

UnleashX's own objects contain a copy of every std::vector/map/list member
it uses, per element type. They can't be matched against a library, but
the template code is the same for every element type, so each member is
recognised by its shape and its calls (following the VC7.1 headers):

  * size()/capacity() by their code, _Xlen and the _Tree members by the
    Dinkumware error strings, scalar deleting destructors by their two
    calls;
  * the rest from the members that call them (push_back calls size,
    capacity, _Ufill and insert in that order; insert calls _Insert_n; and
    so on), repeated until nothing changes.

The element type comes from the element size in the code and, where sizes
collide, from the T constructor/destructor/operator= an instantiation
calls or the classes that use it; the container types and struct sizes
are read from src/ headers. Names whose T isn't certain keep the size:
std::vector<T_3c>.
"""
import argparse
import collections
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))

PART1 = (0x11000, 0x34000)
APP2 = (0x6A400, 0x13FD38)     # where most callers are
BASIC_SIZES = {'std::string': 0x1C, 'std::wstring': 0x1C, 'int': 4, 'BYTE': 1, 'DWORD': 4, 'char': 1}


class Func:
    def __init__(self, addr, size, name):
        self.addr, self.size, self.name = addr, size, name
        self.text = []          # 'mnemonic op_str'
        self.calls = []         # targets, in order
        self.backjump = False   # a loop
        self.refs = set()       # immediates and absolute addresses used
        self.recursive = False
        self.strings = []
        self.role = None        # e.g. 'vector::push_back'
        self.elem = None        # element size
        self.typ = None         # element type name


def load():
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    from capstone.x86 import X86_OP_IMM, X86_OP_MEM
    from xbe import Xbe
    x = Xbe(os.path.join(ROOT, 'UnleashX_0_39_0528A/Default.xbe'))
    img = x.flat_image()
    funcs = {}
    for line in list(open(os.path.join(ROOT, 'build/export/functions.tsv')))[1:]:
        f = line.rstrip('\n').split('\t')
        funcs[int(f[0], 16)] = Func(int(f[0], 16), int(f[1]), f[3])
    by_name = collections.defaultdict(list)
    for f in funcs.values():
        by_name[f.name].append(f.addr)
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    for f in funcs.values():
        part1 = PART1[0] <= f.addr < PART1[1]
        if not part1 and not APP2[0] <= f.addr < APP2[1]:
            continue
        for ins in md.disasm(img[f.addr - x.base:f.addr - x.base + f.size], f.addr):
            if part1:
                f.text.append('%s %s' % (ins.mnemonic, ins.op_str))
            for op in ins.operands:
                if op.type == X86_OP_IMM:
                    f.refs.add(op.imm)
                elif op.type == X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                    f.refs.add(op.mem.disp)
            if ins.mnemonic.startswith('j') and ins.operands and ins.operands[0].type == X86_OP_IMM:
                if f.addr <= ins.operands[0].imm <= ins.address:
                    f.backjump = True
            if ins.mnemonic in ('call', 'jmp') and ins.operands and ins.operands[0].type == X86_OP_IMM:
                if ins.operands[0].imm == f.addr and ins.mnemonic == 'call':
                    f.recursive = True
                elif ins.operands[0].imm in funcs and ins.operands[0].imm != f.addr:
                    f.calls.append(ins.operands[0].imm)
    # Strings by the addresses each function uses (names change as symbols
    # get applied).
    texts = {}
    for line in open(os.path.join(ROOT, 'build/export/strings.tsv')):
        p = line.rstrip('\n').split('\t')
        if len(p) >= 3:
            texts[int(p[0], 16)] = p[2]
    for f in funcs.values():
        f.strings = [texts[r] for r in sorted(f.refs) if r in texts]
    return funcs


def has_source():
    done = set()
    for path in glob.glob(os.path.join(ROOT, 'src/**/*.cpp'), recursive=True) + \
            glob.glob(os.path.join(ROOT, 'src/**/*.h'), recursive=True):
        for line in open(path, encoding='latin-1'):
            if '//' in line and 'TODO: decompile' not in line:
                for m in re.finditer(r'0x([0-9A-Fa-f]{8})', line.split('//', 1)[1]):
                    done.add(int(m.group(1), 16))
    return done


def element_types():
    """{element size: [types]} for the vector/list element types src/ uses."""
    sizes = {}
    for path in glob.glob(os.path.join(ROOT, 'src/**/*.h'), recursive=True):
        lines = open(path, encoding='latin-1').read().split('\n')
        for i, line in enumerate(lines):
            m = re.match(r'\s*(?:struct|class)\s+(\w+)\b(?!.*;)', line)
            if m:
                found = re.findall(r'\b(0x[0-9A-Fa-f]+|\d+) bytes', ' '.join(lines[max(0, i - 4):i + 1]))
                if found:
                    sizes[m.group(1)] = int(found[-1], 0)
    used = set()
    for path in glob.glob(os.path.join(ROOT, 'src/**/*.h'), recursive=True) + \
            glob.glob(os.path.join(ROOT, 'src/**/*.cpp'), recursive=True):
        text = open(path, encoding='latin-1').read()
        for m in re.finditer(r'std::vector<\s*([\w:]+(?:\s*\*)?)\s*>', text):
            used.add(m.group(1).replace(' ', ''))
        for m in re.finditer(r'typedef\s+std::vector<[^;]*>\s+(\w+);', text):
            sizes[m.group(1)] = 0x10                  # a vector
    table = collections.defaultdict(list)
    for t in sorted(used):
        size = 4 if t.endswith('*') else BASIC_SIZES.get(t, sizes.get(t.split('::')[-1]))
        if size:
            table[size].append(t)
    return table


SIZE_RE = re.compile(r'mov (\w+), dword ptr \[(\w+) \+ 4\]')


def size_or_capacity(f):
    """('size'|'capacity', element size) for vector::size()/capacity()."""
    if f.calls or f.size > 40 or not f.text:
        return None
    m = SIZE_RE.match(f.text[0])
    if not m:
        return None
    first, base = m.groups()
    body = ' ; '.join(f.text)
    if 'test %s, %s' % (first, first) not in body:
        return None
    if 'dword ptr [%s + 8]' % base in body:
        kind = 'size'
    elif 'dword ptr [%s + 0xc]' % base in body:
        kind = 'capacity'
    else:
        return None
    elem = 1
    m = re.search(r'sar \w+, (\d+)', body)
    if m:
        elem = 1 << int(m.group(1))
    m = re.search(r'(?:push|mov \w+,) (0x[0-9a-f]+|\d+) ; (?:cdq ; )?(?:pop \w+ ; )?(?:cdq ; )?(?:mov \w+, (?:0x[0-9a-f]+|\d+) ; )?idiv',
                  body)
    if 'idiv' in body:
        m = re.findall(r'(?:push|mov \w+,) (0x[0-9a-f]+|\d+)', body)
        if not m:
            return None
        elem = int(m[-1], 0)
    return kind, elem


def scalar_deleting(f, funcs):
    """The class of a scalar deleting destructor (dtor, then delete if bit 0)."""
    if f.size > 48 or len(f.calls) != 2:
        return None
    dtor, delete = (funcs[c].name for c in f.calls)
    if delete != 'operator_delete' or '::~' not in dtor:
        return None
    if not any(re.match(r'test byte ptr \[\w+ \+ (8|0xc)\], 1', t) for t in f.text):
        return None
    return dtor.split('::~')[0]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('-o', '--out', default=os.path.join(ROOT, 'symbols/stl.txt'))
    ap.add_argument('-v', '--verbose', action='store_true')
    args = ap.parse_args()

    funcs = load()
    done = has_source()
    known = set()
    for fn in ('xdklib.txt', 'thirdparty.txt'):
        for line in open(os.path.join(ROOT, 'symbols', fn)):
            p = line.split()
            if len(p) >= 2 and not line.startswith('#'):
                known.add(int(p[0], 16))
    # This tool's own earlier output (already applied in Ghidra) counts as
    # unnamed.
    previous = set()
    own = os.path.join(ROOT, 'symbols/stl.txt')
    if os.path.exists(own):
        for line in open(own):
            p = line.split()
            if len(p) >= 2 and not line.startswith('#'):
                previous.add(int(p[0], 16))
    for a in previous:
        if a in funcs:
            funcs[a].name = 'FUN_%08x' % a
    # Roles are worked out for all of part 1 (decompiled wrappers and named
    # functions take part in the call relations); only unnamed functions
    # without source get names.
    scope = [f for a, f in sorted(funcs.items()) if PART1[0] <= a < PART1[1] and a not in known]
    free = {f.addr for f in scope}
    curated = set()
    for line in open(os.path.join(ROOT, 'symbols/names.txt')):
        p = line.split()
        if len(p) >= 2 and not line.startswith('#'):
            curated.add(int(p[0], 16))
    nameable = {f.addr for f in scope if f.addr not in done and f.addr not in curated
                and f.name.startswith(('FUN_', 'thunk_FUN_'))}
    names = {}                                       # address -> name

    def give(f, role, elem=None):
        if f.addr in free and f.role is None:
            f.role = role
            if elem:
                f.elem = elem
            return 1
        return 0

    # Recognised by themselves.
    for f in scope:
        sc = size_or_capacity(f)
        if sc:
            give(f, 'vector::' + sc[0], sc[1])
            continue
        cls = scalar_deleting(f, funcs)
        if cls and f.addr in nameable:
            names[f.addr] = '%s::scalar_deleting_destructor' % cls
            continue
        for s in f.strings:
            if 'vector<T> too long' in s:
                give(f, 'vector::_Xlen')
            elif 'map/set<T> too long' in s:
                give(f, 'tree::_Insert')
            elif 'invalid map/set<T> iterator' in s:
                give(f, 'tree::erase')
            elif 'list<T> too long' in s:
                give(f, 'list::_Xlen')

    def role(a):
        return funcs[a].role if a in funcs else None

    all_callers = collections.defaultdict(set)
    for g in funcs.values():
        for c in g.calls:
            all_callers[c].add(g.addr)

    def callers_of(a):
        return sorted(all_callers[a])

    def is_tag_dispatch(f):
        """std::_Uninitialized_copy and friends: pass the arguments on with a
        dummy iterator tag ([ebp - 4], never written)."""
        return len(f.calls) == 1 and f.size < 48 and any('dword ptr [ebp - 4]' in t for t in f.text)

    def is_loop(f):
        return f.backjump or bool(f.calls and funcs[f.calls[0]].name == '__EH_prolog') or not f.calls

    # What an element loop calls per element.
    PER_ELEMENT = {'std::_Destroy_range': 'T::~T', 'std::fill': 'T::operator=', 'std::_Uninit_copy': 'T::T(copy)',
                   'std::_Uninit_fill_n': 'T::T(copy)', 'std::_Copy_backward_opt': 'T::operator=',
                   'std::_Copy_opt': 'T::operator='}

    def slot(a, kind, elem):
        """Name a call slot of the vector code by what the function is: the
        vector member, the std:: dispatcher, or the implementation."""
        if a not in funcs or a not in free:
            return 0
        f = funcs[a]
        impl = {'ucopy': 'std::_Uninit_copy', 'ufill': 'std::_Uninit_fill_n', 'destroy': 'std::_Destroy_range',
                'fill': 'std::fill', 'copy_backward': 'std::_Copy_backward_opt', 'copy': 'std::_Copy_opt'}[kind]
        disp = {'ucopy': 'std::_Uninitialized_copy', 'ufill': 'std::_Uninitialized_fill_n',
                'copy_backward': 'std::copy_backward', 'copy': 'std::copy'}.get(kind)
        member = {'ucopy': 'vector::_Ucopy', 'ufill': 'vector::_Ufill', 'destroy': 'vector::_Destroy'}.get(kind)
        if disp and is_tag_dispatch(f):
            n = give(f, disp, elem)
            return n + slot(f.calls[0], kind + '!impl', elem) if False else n + give_impl(f.calls[0], impl, elem)
        if member and len(f.calls) == 1 and not is_loop(f) and f.size < 60:
            n = give(f, member, elem)
            return n + slot(f.calls[0], kind, elem)
        return give_impl(a, impl, elem)

    def give_impl(a, r, elem):
        if a not in funcs:
            return 0
        f = funcs[a]
        n = give(f, r, elem)
        if f.role == r and r in PER_ELEMENT and f.backjump:
            for c in distinct(f.calls):
                if funcs[c].name not in ('__EH_prolog', '_CxxThrowException', 'operator_delete', 'operator_new'):
                    n += give(funcs[c], PER_ELEMENT[r], elem)
                    break
        return n

    SKIP = ('vector::size', 'vector::capacity', 'vector::_Xlen')

    def step(f):
        """The element step of a loop (add/sub reg, S)."""
        for t in f.text:
            m = re.match(r'(add|sub) (e[a-z]{2}), (0x[0-9a-f]+|\d+)$', t)
            if m and m.group(2) not in ('esp', 'ebp'):
                return m.group(1), int(m.group(3), 0)
        return None, None

    def args_passed(f):
        """Arguments a dispatcher passes on besides the tag."""
        return sum(1 for t in f.text if re.match(r'push dword ptr \[(ebp|esp) \+ (0x[0-9a-f]+|\d+)\]', t))

    def tree_slots():
        """_Tree: _Insert calls _Buynode then rotates (L, R, R, L in the
        header); erase(iterator) calls ++ (const_iterator::_Inc), rotates,
        then destroys the value and deletes the node. _Erase deletes a
        subtree recursively."""
        n = 0
        THROWS = ('std::string::ctor_cstr2', 'std::logic_error::logic_error', '_CxxThrowException', 'operator_delete',
                  'operator_new')
        for f in scope:
            calls = [c for c in f.calls if funcs[c].name not in THROWS and not funcs[c].name.startswith('std::')]
            if f.role == 'tree::_Insert' and len(calls) >= 3:
                n += give(funcs[calls[0]], 'tree::_Buynode')
                rot = distinct(calls[1:])
                if len(rot) == 2:
                    n += give(funcs[rot[0]], 'tree::_Lrotate') + give(funcs[rot[1]], 'tree::_Rrotate')
            elif f.role == 'tree::erase' and len(calls) >= 4:
                n += give(funcs[calls[0]], 'tree::const_iterator::_Inc')
                last = calls[-1]
                if role(last) not in ('tree::_Lrotate', 'tree::_Rrotate'):
                    n += give(funcs[last], 'tree::_Destval')
        for f in scope:
            if f.role is None and f.recursive and any(role(c) == 'tree::_Destval' for c in f.calls):
                n += give(f, 'tree::_Erase')
        for f in scope:
            if f.role is not None:
                continue
            rs = [role(c) for c in f.calls]
            names_ = [funcs[c].name for c in f.calls]
            # clear(): _Erase(_Root()) and reset the head.
            if rs == ['tree::_Erase'] and f.size < 48:
                n += give(f, 'tree::clear')
            # erase(first, last): clear() for everything, else erase(it++).
            elif 'tree::const_iterator::_Inc' in rs and 'tree::erase' in rs and f.size < 120:
                n += give(f, 'tree::erase')
                for c in f.calls:
                    if c in free and role(c) is None and c != f.addr:
                        n += give(funcs[c], 'tree::clear')
                        break
            # _Tidy(): erase(begin(), end()), delete the head.
            elif len(f.calls) == 2 and names_[1] == 'operator_delete' and f.size < 48 and \
                    funcs[f.calls[0]].role == 'tree::erase' and funcs[f.calls[0]].size < 120:
                n += give(f, 'tree::_Tidy')
            # insert(value): _Insert, and --_Where (_Dec) in between.
            elif 'tree::_Insert' in rs and f.size < 400:
                n += give(f, 'tree::insert')
                for c in distinct(f.calls):
                    if c in free and role(c) is None and not funcs[c].calls and funcs[c].size < 80:
                        n += give(funcs[c], 'tree::const_iterator::_Dec')
        return n

    def chain(a, names_):
        """Follow single-call forwarders from a, naming them in order."""
        n = 0
        for r in names_:
            if a not in funcs or a not in free:
                break
            f = funcs[a]
            n += give(f, r)
            inner = [c for c in distinct(f.calls) if c in nameable and not (role(c) or '').startswith('T::')]
            if len(inner) != 1:
                break
            a = inner[0]
        return n

    def sort_slots():
        """std::sort (VC7.1): _Sort is recursive and calls
        _Unguarded_partition, itself, make_heap, sort_heap and
        _Insertion_sort; _Unguarded_partition starts with _Median, which
        calls _Med3; _Med3 compares and calls iter_swap."""
        n = 0
        for f in scope:
            if f.role is None and f.recursive:
                others = [c for c in distinct(f.calls) if c in free]
                if len(others) == 4 and f.size > 120:
                    n += give(f, 'std::_Sort')
            if f.role != 'std::_Sort':
                continue
            part, mk, srt, ins = [c for c in distinct(f.calls) if c in free][:4]
            n += give(funcs[part], 'std::_Unguarded_partition') + give(funcs[ins], 'std::_Insertion_sort')
            n += chain(mk, ['std::make_heap', 'std::_Make_heap', 'std::_Adjust_heap', 'std::_Push_heap'])
            n += chain(srt, ['std::sort_heap', 'std::pop_heap', 'std::_Pop_heap_0', 'std::_Pop_heap'])
            for c in callers_of(f.addr):
                g = funcs[c]
                if g.addr in free and g.role is None and len(distinct(g.calls)) == 1 and g.size < 64:
                    n += give(g, 'std::sort')
        for f in scope:
            if f.role == 'std::_Unguarded_partition':
                inner = [c for c in distinct(f.calls) if c in free]
                if inner:
                    n += give(funcs[inner[0]], 'std::_Median')
                    for c in inner[1:]:
                        if funcs[c].role is None and any(role(x) and role(x).startswith('T::') or
                                                         funcs[x].name.endswith(('::operator=', '_copy'))
                                                         for x in funcs[c].calls):
                            n += give(funcs[c], 'std::iter_swap')
            elif f.role == 'std::_Median':
                counts = collections.Counter(c for c in f.calls if c in free)
                if counts:
                    n += give(funcs[counts.most_common(1)[0][0]], 'std::_Med3')
            elif f.role == 'std::_Med3':
                for c in distinct(f.calls):
                    if role(c) == 'std::iter_swap' or (c in free and funcs[c].role is None and any(
                            funcs[x].name.endswith(('::operator=', '_copy')) or (role(x) or '').startswith('T::')
                            for x in funcs[c].calls)):
                        n += give(funcs[c], 'std::iter_swap')
            elif f.role == 'std::_Insertion_sort':
                for c in distinct(f.calls):
                    g = funcs[c]
                    if c not in nameable or g.role is not None:
                        continue
                    if is_tag_dispatch(g):
                        n += slot(c, 'copy_backward', None)
                    # rotate(first, mid, last): checks, then _Rotate (the
                    # random-access one: gcd cycles, idiv).
                    elif len(g.calls) == 1 and g.size < 48 and g.calls[0] in nameable and \
                            any(t.startswith('idiv') for t in funcs[g.calls[0]].text):
                        n += give(g, 'std::rotate') + give(funcs[g.calls[0]], 'std::_Rotate')
        return n

    def more_members():
        """vector reserve/_Insert_n/operator=/copy constructor, map
        _Lbound/find/operator[], use_facet."""
        n = 0
        for f in scope:
            if f.role is not None or f.addr not in nameable:
                continue
            rs = [role(c) for c in f.calls]
            names_ = [funcs[c].name for c in f.calls]
            vec = [c for c in f.calls if role(c) in ('vector::size', 'vector::capacity')]
            elem = funcs[vec[0]].elem if vec else None
            if 'vector::_Xlen' in rs and 'vector::capacity' in rs and 'operator_new' in names_:
                if rs.index('vector::_Xlen') < rs.index('vector::capacity'):
                    # reserve(n): max_size check, capacity, allocate, _Ucopy...
                    n += give(f, 'vector::reserve', elem)
                elif 'vector::size' in rs:
                    n += give(f, 'vector::_Insert_n', elem)
                continue
            if any('bad cast' in x for x in f.strings) and '_Lockit' in ' '.join(names_):
                n += give(f, 'std::use_facet')
                continue
            by = [funcs[c].name for c in all_callers[f.addr]]
            if vec and rs.count('vector::size') >= 2 and any(x.endswith('::operator=') for x in by):
                n += give(f, 'vector::operator=', elem)
            elif vec and names_ and names_[0] == '__EH_prolog' and \
                    any(re.match(r'(\w+)::\1$', x) for x in by) and f.size < 160:
                n += give(f, 'vector::vector', elem)
        # map lookups: _Lbound (a compare loop down the tree), find,
        # operator[] (_Lbound, then insert).
        for f in scope:
            if f.role is not None or f.addr not in nameable:
                continue
            rs = [role(c) for c in f.calls]
            lb = [c for c in f.calls if is_lbound(funcs[c])]
            if lb and 'tree::insert' in rs and f.size < 160:
                n += give(f, 'tree::operator[]') + give(funcs[lb[0]], 'tree::_Lbound')
            elif lb and f.size < 140 and len(distinct(f.calls)) <= 4 and \
                    any(x in ('CSoundEffects::Find', 'CImageManager::Find') or x.endswith('::Find')
                        for x in (funcs[c].name for c in all_callers[f.addr])):
                n += give(f, 'tree::find') + give(funcs[lb[0]], 'tree::_Lbound')
        return n

    def is_lbound(g):
        """_Lbound: down the tree comparing keys until the nil node."""
        if g.size > 100 or not g.backjump or (g.calls and any(
                funcs[c].name not in ('std::string::compare',) for c in g.calls)):
            return False
        return any(re.match(r'cmp byte ptr \[e\w\w \+ 0x\w+\], 0', t) for t in g.text)

    def standalone_helpers():
        """The std:: helpers by their own shape, wherever they're used:
        dispatchers (arguments plus a dummy tag) and their loops."""
        n = 0
        for f in scope:
            if f.role or not is_tag_dispatch(f):
                continue
            g = funcs[f.calls[0]]
            if g.addr not in free or g.role not in (None,) and not g.role.startswith('std::'):
                continue
            direction, size = step(g)
            if not size:
                continue
            body = ' ; '.join(g.text)
            if args_passed(f) == 3:
                if direction == 'add' and 'dword ptr [ebp + 0xc], 0' in body:
                    r = ('std::_Uninitialized_fill_n', 'std::_Uninit_fill_n')
                else:
                    r = ('std::_Uninitialized_copy', 'std::_Uninit_copy') if direction == 'add' else None
            elif args_passed(f) == 2:
                r = ('std::copy', 'std::_Copy_opt') if direction == 'add' else \
                    ('std::copy_backward', 'std::_Copy_backward_opt')
            else:
                r = None
            if r:
                n += give(f, r[0], size) + give_impl(g.addr, r[1], size)
        # Loops assigning elements with T::operator=.
        for f in scope:
            if f.role or not f.backjump or not f.calls or f.size > 80:
                continue
            if {role(c) for c in f.calls} != {'T::operator='}:
                continue
            steps = [m.group(1, 2) for m in (re.match(r'(add|sub) (e[a-z]{2}), (?:0x[0-9a-f]+|\d+)$', t)
                                             for t in f.text) if m and m.group(2) not in ('esp', 'ebp')]
            elem = funcs[f.calls[0]].elem
            if any(d == 'sub' for d, r in steps):
                n += give(f, 'std::_Copy_backward_opt', elem)
            elif len({r for d, r in steps}) >= 2:
                n += give(f, 'std::_Copy_opt', elem)
            elif steps:
                n += give(f, 'std::fill', elem)
        # ~vector: only _Tidy.
        for f in scope:
            if f.role is None and len(f.calls) == 1 and role(f.calls[0]) == 'vector::_Tidy' and f.size < 32:
                n += give(f, 'vector::~vector', funcs[f.calls[0]].elem)
        # Members that only forward to a dispatcher: _Ucopy and _Ufill.
        for f in scope:
            if f.role or len(f.calls) != 1 or f.size > 48 or f.backjump:
                continue
            r = role(f.calls[0])
            if r == 'std::_Uninitialized_copy':
                n += give(f, 'vector::_Ucopy', funcs[f.calls[0]].elem)
            elif r == 'std::_Uninitialized_fill_n' and any(t.startswith(('imul', 'shl', 'lea')) for t in f.text):
                n += give(f, 'vector::_Ufill', funcs[f.calls[0]].elem)
        return n

    def insert_n_slots(f):
        """_Insert_n's calls follow the header: [T copy] capacity size _Xlen
        size... new _Ucopy _Ufill _Ucopy size [_Destroy delete]... throw
        _Ucopy _Ufill fill [_Destroy] throw _Ucopy copy_backward fill."""
        n = 0
        names_ = [funcs[c].name for c in f.calls]
        if 'operator_new' not in names_:
            return 0
        k = names_.index('operator_new')
        after = [c for c in f.calls[k + 1:] if role(c) not in SKIP]
        plain = [c for c in after if funcs[c].name not in ('operator_delete', '_CxxThrowException')]
        if len(plain) < 2:
            return 0
        ucopy, ufill = plain[0], plain[1]
        n += slot(ucopy, 'ucopy', f.elem) + slot(ufill, 'ufill', f.elem)
        for x, y in zip(f.calls, f.calls[1:]):
            if funcs[y].name == 'operator_delete' and x not in (ucopy, ufill) and role(x) not in SKIP \
                    and funcs[x].name not in ('operator_delete', '_CxxThrowException'):
                n += slot(x, 'destroy', f.elem)
                destroy = x
                break
        else:
            destroy = None
        rest = [c for c in plain[2:] if c not in (ucopy, ufill, destroy)]
        if rest:
            n += slot(rest[0], 'fill', f.elem)
            rest2 = [c for c in rest[1:] if c != rest[0]]
            if rest2:
                n += slot(rest2[0], 'copy_backward', f.elem)
        # The copy constructor of T, for _Tmp (before capacity()).
        for c in f.calls[1:]:
            if role(c) in SKIP:
                break
            if c in free and funcs[c].role is None:
                n += give(funcs[c], 'T::T(copy)', f.elem)
            break
        return n
    changed = True
    while changed:
        changed = 0
        # push_back: small, calls size and capacity first.
        for f in scope:
            if f.role is None and 40 < f.size < 120 and len(f.calls) >= 3:
                r = [role(c) for c in f.calls[:2]]
                if set(r) == {'vector::capacity', 'vector::size'}:
                    changed += give(f, 'vector::push_back', funcs[f.calls[0]].elem)
        for f in scope:
            if f.role == 'vector::push_back':
                # size, capacity, _Ufill (or what it inlines to), insert
                unk = [c for c in distinct(f.calls) if role(c) not in SKIP]
                if len(unk) == 2:
                    changed += slot(unk[0], 'ufill', f.elem)
                    changed += give(funcs[unk[1]], 'vector::insert', f.elem)
            elif f.role == 'vector::insert':
                unk = [c for c in distinct(f.calls) if role(c) not in SKIP]
                if len(unk) == 1:
                    changed += give(funcs[unk[0]], 'vector::_Insert_n', f.elem)
            elif f.role == 'vector::_Insert_n':
                changed += insert_n_slots(f)
            elif f.role in ('vector::_Destroy', 'std::_Destroy_range') or f.role == 'vector::_Tidy':
                pass
        # _Tidy: _Destroy (or _Destroy_range), then operator delete.
        for f in scope:
            if f.role is None and f.size < 60 and len(f.calls) == 2 and \
                    role(f.calls[0]) in ('vector::_Destroy', 'std::_Destroy_range') and \
                    funcs[f.calls[1]].name == 'operator_delete':
                changed += give(f, 'vector::_Tidy', funcs[f.calls[0]].elem)
        changed += standalone_helpers()
        changed += tree_slots()
        changed += sort_slots()
        changed += more_members()
        if not changed:
            break

    members, globals_ = class_members()
    infer_types(funcs, scope, element_types(), members, globals_)
    infer_tree_types(funcs, scope)
    infer_sort_types(funcs, scope, element_types(), members)
    # Members of T (and helpers) shared by instantiations of different sizes:
    # no size.
    for f in scope:
        if f.role and f.role.startswith(('T::', 'std::')):
            sizes_ = {funcs[c].elem for c in all_callers[f.addr] if funcs[c].role and funcs[c].elem}
            if len(sizes_) > 1:
                f.elem, f.typ = None, None
    for f in scope:
        if f.role and f.addr not in names and f.addr in nameable:
            names[f.addr] = ghidra_safe(format_name(f))
    with open(args.out, 'w') as out:
        out.write('# Generated by tools/stlname.py -- do not edit; put overrides in symbols/names.txt\n')
        for a in sorted(names):
            out.write('%08x %s\n' % (a, names[a]))
    roles = collections.Counter(f.role for f in scope if f.role and f.addr in names)
    print('%d unnamed functions, %d named: %s' % (len(nameable), len(names), dict(roles)), file=sys.stderr)
    if args.verbose:
        for a in sorted(names):
            print('%08x %5d %s' % (a, funcs[a].size, names[a]))


def class_members():
    """Where src/ says which vectors are used: {class or function name:
    element types} (members, parameters) and {global address: type}."""
    out = collections.defaultdict(set)
    for path in glob.glob(os.path.join(ROOT, 'src/**/*.h'), recursive=True):
        current = None
        for line in open(path, encoding='latin-1'):
            m = re.match(r'\s*(?:class|struct)\s+(\w+)\b(?!.*;)', line)
            if m:
                current = m.group(1)
            m = re.search(r'std::vector<\s*([\w:]+(?:\s*\*)?)\s*>\s+\w+;', line)
            if m and current:
                out[current].add(m.group(1).replace(' ', ''))
            m = re.search(r'(\w+)\s*\([^;]*std::vector<\s*([\w:]+(?:\s*\*)?)\s*>', line)
            if m:
                out[m.group(1)].add(m.group(2).replace(' ', ''))
                if current:
                    out['%s::%s' % (current, m.group(1))].add(m.group(2).replace(' ', ''))
    # A vector of a vector typedef holds the inner element type too.
    typedefs = {}
    for path in glob.glob(os.path.join(ROOT, 'src/**/*.h'), recursive=True):
        for m in re.finditer(r'typedef\s+std::vector<\s*([\w:]+(?:\s*\*)?)\s*>\s+(\w+);',
                             open(path, encoding='latin-1').read()):
            typedefs[m.group(2)] = m.group(1).replace(' ', '')
    for key in list(out):
        todo = list(out[key])
        while todo:
            t = todo.pop()
            if t in typedefs and typedefs[t] not in out[key]:
                out[key].add(typedefs[t])
                todo.append(typedefs[t])
    globals_ = {}
    for path in glob.glob(os.path.join(ROOT, 'src/**/*.h'), recursive=True) + \
            glob.glob(os.path.join(ROOT, 'src/**/*.cpp'), recursive=True):
        for m in re.finditer(r'std::vector<\s*([\w:]+(?:\s*\*)?)\s*>\s+\w+;\s*//\s*0x([0-9A-Fa-f]{8})',
                             open(path, encoding='latin-1').read()):
            globals_[int(m.group(2), 16)] = m.group(1).replace(' ', '')
    return out, globals_


def infer_types(funcs, scope, table, members, globals_):
    """Element types: each function from its own evidence (the T members it
    calls, the classes and functions that call it, the global vectors they
    use), then along the calls between roles where everything agrees. The
    linker merged identical code between same-size types (size(), the
    std:: helpers, ...), so those stay T_<size>."""
    callers = collections.defaultdict(set)
    for f in funcs.values():
        for c in f.calls:
            callers[c].add(f.addr)
    # T's members and the std:: helpers can be shared between same-size
    # types: they're typed below, only when all their callers agree.
    roled = [f for f in scope if f.role and f.elem and f.role not in ('vector::_Xlen', 'list::_Xlen')
             and not f.role.startswith(('T::', 'std::'))]

    def own_type(f):
        cands = set(table.get(f.elem, []))
        if not cands:
            return None
        evidence = set()
        for c in f.calls:
            n = funcs[c].name
            if n.startswith('std::string::'):
                evidence.add('std::string')
            elif n.startswith('std::wstring::'):
                evidence.add('std::wstring')
            m = re.match(r'(\w+)::(?:~?\1(?:_copy)?|operator=)$', n)
            if m:
                evidence.add(m.group(1))
        hit = {t for t in cands if t in evidence or t.split('::')[-1] in evidence}
        if len(hit) == 1:
            return hit.pop()
        owners, weak = set(), set()
        for c in callers[f.addr]:
            if funcs[c].role:
                continue
            n = funcs[c].name
            owners.add(n)
            owners.add(n.split('::')[-1])
            if '::' in n:
                owners.add(n.split('::')[0])
            weak.update(globals_[r] for r in funcs[c].refs if r in globals_)
        if not owners:
            return None
        owned = {t for o in owners for t in members.get(o, ()) if t in cands}
        weak &= cands
        if len(owned) == 1:
            return owned.pop()
        if not owned and len(weak) == 1:
            return weak.pop()
        if len(cands) == 1:
            return next(iter(cands))
        return None

    for f in roled:
        f.typ = own_type(f)
    # Along the calls: a function whose role callers (or, for the vector
    # members, role callees other than size/capacity) all agree.
    for _ in range(6):
        changed = False
        for f in roled:
            if f.typ:
                continue
            up = {funcs[c].typ for c in callers[f.addr] if funcs[c].role}
            if up and None not in up and len(up) == 1:
                f.typ = up.pop()
                changed = True
                continue
            if f.role.startswith('vector::'):
                down = {funcs[c].typ for c in f.calls
                        if funcs[c].role and funcs[c].role.startswith('vector::')
                        and funcs[c].role not in ('vector::size', 'vector::capacity', 'vector::_Xlen')}
                if down and None not in down and len(down) == 1:
                    f.typ = down.pop()
                    changed = True
        if not changed:
            break
    # T's members and the std:: helpers: typed when every caller in the
    # program agrees (callers without a role count as unknown).
    for _ in range(4):
        for f in scope:
            if f.role and f.role.startswith(('T::', 'std::')) and not f.typ:
                types = {funcs[c].typ for c in callers[f.addr]}
                if len(types) == 1 and None not in types:
                    f.typ = types.pop()


MAP_TYPE = r'std::map<[^;{}()]*?>(?=\s*(?:\w|::|\*|&|>))'


def file_maps():
    """{function address: the std::map types its body uses} (explicitly, or
    through map variables declared in its file or header); the whole
    file's when the body names none."""
    out = {}
    for path in glob.glob(os.path.join(ROOT, 'src/**/*.cpp'), recursive=True):
        text = open(path, encoding='latin-1').read()
        hdr = path[:-4] + '.h'
        decls = text + (open(hdr, encoding='latin-1').read() if os.path.exists(hdr) else '')
        variables = {}
        for m in re.finditer(r'(std::map<[^;{}()]*?>)\s+(\w+)\s*;', decls):
            variables[m.group(2)] = re.sub(r'\s+', '', m.group(1))
        whole = {re.sub(r'\s+', '', m) for m in re.findall(MAP_TYPE, decls)}
        whole = {m for m in whole if m.count('<') == m.count('>')}
        marks = [(m.start(), int(m.group(1), 16)) for m in re.finditer(r'//\s*0x([0-9A-Fa-f]{8})', text)]
        for k, (pos, addr) in enumerate(marks):
            body = text[pos:marks[k + 1][0] if k + 1 < len(marks) else len(text)]
            used = {re.sub(r'\s+', '', m) for m in re.findall(MAP_TYPE, body)}
            used |= {t for v, t in variables.items() if re.search(r'\b%s\b' % v, body)}
            used = {m for m in used if m.count('<') == m.count('>')}
            out[addr] = used or whole
    return out


def infer_tree_types(funcs, scope):
    """A tree's type: the one std::map type in the source files of the
    named functions it's reached from."""
    callers = collections.defaultdict(set)
    for f in funcs.values():
        for c in f.calls:
            callers[c].add(f.addr)
    maps = file_maps()
    trees = [f for f in scope if f.role in ('tree::_Insert', 'tree::erase')]
    for f in trees:
        seen, found, stack = {f.addr}, set(), [f.addr]
        while stack:
            a = stack.pop()
            for p in callers[a]:
                if p in seen:
                    continue
                seen.add(p)
                if p in maps:
                    found |= maps[p]
                else:                             # no source: keep going up
                    stack.append(p)
        if len(found) == 1:
            f.typ = found.pop()
    # Along the tree's own calls, both ways; the rotations and iterator
    # steps are shared between trees.
    SHARED = ('tree::_Lrotate', 'tree::_Rrotate', 'tree::const_iterator::_Inc', 'tree::const_iterator::_Dec')
    own = [f for f in scope if f.role and f.role.startswith('tree::') and f.role not in SHARED]
    for _ in range(6):
        for f in own:
            if f.typ:
                continue
            near = [funcs[c] for c in list(callers[f.addr]) + f.calls]
            types = {g.typ for g in near if g.role and g.role.startswith('tree::') and g.role not in SHARED}
            types.discard(None)
            if len(types) == 1:
                f.typ = types.pop()
    for f in scope:
        if f.role in SHARED and not f.typ:
            types = {funcs[c].typ for c in callers[f.addr] if funcs[c].role and funcs[c].role.startswith('tree::')}
            if len(types) == 1 and None not in types:
                f.typ = types.pop()


# std::sort calls whose element type the decompiled source already
# records (the std::sort copy itself is inlined or documented at the call).
SORT_SITES = {
    0x00020D23: 'SGameEntry*',     # CGameManager's game scan: std::sort(m_games...) (GameManager.h)
    0x000274D4: 'SSkinDrawItem',   # CSkin draw orders: std::sort(..., CompareSkinZIndex) (SkinParser.cpp)
}

SORT_FAMILY = ('std::sort', 'std::_Sort', 'std::_Unguarded_partition', 'std::_Median', 'std::_Med3', 'std::iter_swap',
               'std::_Insertion_sort', 'std::make_heap', 'std::_Make_heap', 'std::_Adjust_heap', 'std::_Push_heap',
               'std::sort_heap', 'std::pop_heap', 'std::_Pop_heap_0', 'std::_Pop_heap', 'std::rotate', 'std::_Rotate',
               'std::copy_backward', 'std::_Copy_backward_opt')


def infer_sort_types(funcs, scope, table, members):
    """A std::sort instantiation's element type: T's members or the
    comparator it calls (X::X, X::operator=, X::operator_less, ...), else
    the classes and functions that call std::sort."""
    callers = collections.defaultdict(set)
    for f in funcs.values():
        for c in f.calls:
            callers[c].add(f.addr)
    known_types = {t for ts in table.values() for t in ts}
    votes = collections.defaultdict(set)
    for top in [f for f in scope if f.role == 'std::_Sort']:
        family, todo = {top.addr}, [top.addr]
        while todo:
            a = todo.pop()
            for c in funcs[a].calls:
                if c not in family and funcs[c].role in SORT_FAMILY:
                    family.add(c)
                    todo.append(c)
        sorts = [c for c in callers[top.addr] if funcs[c].role == 'std::sort']
        family.update(sorts)
        evidence = set()
        for a in family:
            for c in funcs[a].calls:
                n = funcs[c].name
                m = re.match(r'(\w+)::', n)
                if m and m.group(1) in {t.rstrip('*') for t in known_types}:
                    evidence.add(m.group(1))
                if funcs[c].typ:
                    evidence.add(funcs[c].typ)
                elif (funcs[c].role or '').startswith('T::'):
                    # What T's typed callers elsewhere agree on.
                    seen = {funcs[x].typ for x in callers[c] if x not in family} - {None}
                    if len(seen) == 1:
                        evidence |= seen
        types = {t for t in known_types if t.rstrip('*') in evidence or t in evidence}
        pick = None
        sites = {SORT_SITES[c] for a in [top.addr] + sorts for c in callers[a] if c in SORT_SITES}
        owned = set()
        for sfn in sorts + [top.addr]:
            for c in callers[sfn]:
                if c in family:
                    continue
                n = funcs[c].name
                for key in (n, n.split('::')[0], n.split('::')[-1]):
                    owned |= {t for t in members.get(key, set()) if t in known_types}
        if len(sites) == 1:
            pick = sites.pop()
        elif len(owned) == 1:
            pick = owned.pop()
        elif len(types) == 1:
            pick = types.pop()
        for a in family:
            votes[a].add(pick)
    for a, v in votes.items():
        if len(v) == 1 and None not in v:
            funcs[a].typ = v.pop()


def ghidra_safe(name):
    """'::' inside template arguments becomes '.', so only real scopes
    become Ghidra namespaces."""
    out, depth, i = [], 0, 0
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


def format_name(f):
    t = f.typ or ('T_%x' % f.elem if f.elem else 'T')
    r = f.role
    if r.startswith('std::'):                         # algorithms: std::fill<T>
        return '%s<%s>' % (r, t)
    if r == 'T::~T' and any(re.match(r'test byte ptr \[\w+ \+ (8|0xc)\], 1', x) for x in f.text):
        r = 'T::scalar_deleting_destructor'           # called with 0: just the destructor
        if f.typ and not f.typ.endswith('*'):
            return '%s::scalar_deleting_destructor' % f.typ
    if r.startswith('T::'):                           # T's own members
        if f.typ and not f.typ.endswith('*') and '::' not in f.typ.replace('std::', ''):
            short = f.typ.split('::')[-1]
            return {'T::~T': '%s::~%s', 'T::T(copy)': '%s::%s', 'T::operator=': '%s::operator='}[r].replace(
                '%s', f.typ, 1).replace('%s', short)
        # An element type known only by its size: T_3c::destructor etc.
        return '%s::%s' % (t, {'T::~T': 'destructor', 'T::T(copy)': 'copy_constructor', 'T::operator=': 'operator=',
                               'T::scalar_deleting_destructor': 'scalar_deleting_destructor'}[r])
    kind, member = r.split('::', 1)
    if kind == 'tree':
        return '%s::%s' % (f.typ or 'std::_Tree<T>', member)
    return 'std::%s<%s>::%s' % (kind, t, member)


def distinct(seq):
    seen, out = set(), []
    for x in seq:
        if x not in seen:
            seen.add(x)
            out.append(x)
    return out


if __name__ == '__main__':
    main()

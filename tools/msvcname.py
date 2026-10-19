#!/usr/bin/env python3
"""Readable qualified names from MSVC 7.1 decorated names.

  msvcname.py '?_Tidy@?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@IAEX_NI@Z'
  -> std::basic_string<char,std::char_traits<char>,std::allocator<char>>::_Tidy

Only the name is decoded (scopes, templates, special members), not the
parameter list: enough for labels. Names that can't be decoded come back
unchanged. Plain C names lose their decoration (_f, _f@8, @f@8 -> f).
"""
import re
import sys

PRIMITIVES = {
    'C': 'signed_char', 'D': 'char', 'E': 'unsigned_char', 'F': 'short', 'G': 'unsigned_short', 'H': 'int',
    'I': 'unsigned', 'J': 'long', 'K': 'unsigned_long', 'M': 'float', 'N': 'double', 'O': 'long_double',
    'X': 'void', 'Z': '...',
}
PRIMITIVES_EXT = {'D': '__int8', 'E': 'unsigned___int8', 'F': '__int16', 'G': 'unsigned___int16', 'H': '__int32',
                  'I': 'unsigned___int32', 'J': '__int64', 'K': 'unsigned___int64', 'N': 'bool', 'W': 'wchar_t'}

SPECIAL = {
    '0': '{ctor}', '1': '{dtor}', '2': 'operator_new', '3': 'operator_delete', '4': 'operator=', '5': 'operator>>',
    '6': 'operator<<', '7': 'operator!', '8': 'operator==', '9': 'operator!=', 'A': 'operator[]', 'B': 'operator_cast',
    'C': 'operator->', 'D': 'operator*', 'E': 'operator++', 'F': 'operator--', 'G': 'operator-', 'H': 'operator+',
    'I': 'operator&', 'J': 'operator->*', 'K': 'operator/', 'L': 'operator%', 'M': 'operator<', 'N': 'operator<=',
    'O': 'operator>', 'P': 'operator>=', 'Q': 'operator,', 'R': 'operator()', 'S': 'operator~', 'T': 'operator^',
    'U': 'operator|', 'V': 'operator&&', 'W': 'operator||', 'X': 'operator*=', 'Y': 'operator+=', 'Z': 'operator-=',
    '_0': 'operator/=', '_1': 'operator%=', '_2': 'operator>>=', '_3': 'operator<<=', '_4': 'operator&=',
    '_5': 'operator|=', '_6': 'operator^=', '_7': 'vftable', '_8': 'vbtable', '_9': 'vcall', '_A': 'typeof',
    '_B': 'local_static_guard', '_C': 'string', '_D': 'vbase_destructor', '_E': 'vector_deleting_destructor',
    '_F': 'default_constructor_closure', '_G': 'scalar_deleting_destructor', '_H': 'vector_constructor_iterator',
    '_I': 'vector_destructor_iterator', '_J': 'vector_vbase_constructor_iterator',
    '_K': 'virtual_displacement_map', '_L': 'eh_vector_constructor_iterator',
    '_M': 'eh_vector_destructor_iterator', '_N': 'eh_vector_vbase_constructor_iterator',
    '_O': 'copy_constructor_closure', '_S': 'local_vftable', '_T': 'local_vftable_constructor_closure',
    '_U': 'operator_new[]', '_V': 'operator_delete[]',
}


class Error(Exception):
    pass


class Parser:
    def __init__(self, s):
        self.s, self.pos = s, 0
        self.names = []      # back-references to names (0-9)
        self.types = []      # back-references to template argument types (0-9)

    def peek(self, n=1):
        return self.s[self.pos:self.pos + n]

    def take(self, n=1):
        t = self.s[self.pos:self.pos + n]
        if len(t) < n:
            raise Error('truncated')
        self.pos += n
        return t

    def identifier(self):
        end = self.s.index('@', self.pos)
        name = self.s[self.pos:end]
        self.pos = end + 1
        return name

    def number(self):
        neg = self.peek() == '?'
        if neg:
            self.pos += 1
        c = self.take()
        if c.isdigit():
            value = int(c) + 1
        else:
            value = 0
            while c != '@':
                value = value * 16 + ord(c) - ord('A')
                c = self.take()
        return -value if neg else value

    def remember(self, name):
        if len(self.names) < 10 and name not in self.names:
            self.names.append(name)

    def template(self):
        name = self.identifier()
        saved_names, saved_types = self.names, self.types
        self.names, self.types = [name], []
        args = []
        while self.peek() != '@':
            args.append(self.type())
        self.pos += 1
        self.names, self.types = saved_names, saved_types
        return name + '<' + ','.join(args) + '>'

    def fragment(self):
        c = self.peek()
        if c.isdigit():
            self.pos += 1
            return self.names[int(c)]
        if self.peek(2) == '?$':
            self.pos += 2
            name = self.template()
            self.remember(name)
            return name
        if self.peek(2) == '?A':
            self.identifier()
            return '`anonymous_namespace\''
        if c == '?':
            raise Error('nested symbol')
        name = self.identifier()
        self.remember(name)
        return name

    def scopes(self):
        parts = []
        while self.peek() != '@':
            parts.append(self.fragment())
        self.pos += 1
        return list(reversed(parts))

    def qualified(self):
        return '::'.join(self.scopes())

    def type(self):
        c = self.take()
        if c.isdigit():
            return self.types[int(c)]
        if c in PRIMITIVES:
            return PRIMITIVES[c]
        if c == '_':
            return PRIMITIVES_EXT.get(self.take(), '?')
        if c in 'PQRS':
            cv = self.take()
            if cv == 'E':
                cv = self.take()
            if cv == '6':
                raise Error('function pointer')
            t = ('const_' if cv in 'BD' else '') + self.type() + '*'
        elif c in 'AB':
            cv = self.take()
            t = ('const_' if cv in 'BD' else '') + self.type() + '&'
        elif c in 'TUV':
            t = self.qualified()
        elif c == 'W':
            self.take()
            t = self.qualified()
        elif c == '$':
            k = self.take()
            if k == '0':
                return str(self.number())
            if k == '1':
                raise Error('symbol argument')
            raise Error('template argument $' + k)
        else:
            raise Error('type ' + c)
        if len(self.types) < 10:
            self.types.append(t)
        return t

    def symbol(self):
        if self.take() != '?':
            raise Error('not decorated')
        special = None
        if self.peek() == '?' and self.peek(2) != '?$':
            self.pos += 1
            code = self.take()
            if code == '_':
                code += self.take()
            if code.startswith('_R'):
                raise Error('RTTI')
            special = SPECIAL.get(code)
            if special is None:
                raise Error('special ' + code)
            parts = self.scopes()
        else:
            first = self.fragment()
            parts = self.scopes() + [first]
        if special:
            cls = re.sub(r'<.*', '', parts[-1]) if parts else ''
            if special == '{ctor}':
                special = cls
            elif special == '{dtor}':
                special = '~' + cls
            parts.append(special)
        return '::'.join(parts)


def demangle(name):
    if name.startswith('?'):
        try:
            return Parser(name).symbol()
        except (Error, IndexError, ValueError):
            return name
    m = re.match(r'^[_@]?([A-Za-z_]\w*)@\d+$', name)
    if m:
        return m.group(1)
    if name.startswith('_'):
        return name[1:]
    return name


if __name__ == '__main__':
    for arg in sys.argv[1:]:
        print(demangle(arg))

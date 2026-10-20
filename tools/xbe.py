#!/usr/bin/env python3
"""XBE (Xbox executable) helper.

  xbe.py info    <xbe>                 print header, sections, libraries
  xbe.py flat    <xbe> <out.bin>       write a flat memory image (base = XBE base)
  xbe.py layout  <xbe> <out.txt>       write section/thunk layout for the Ghidra import script
  xbe.py extract <xbe> <outdir>        dump each section's raw bytes to <outdir>/<name>.bin
  xbe.py retail  <xbe> <out.xbe>       rescramble a debug XBE's entry point and kernel thunk
                                       with the retail keys (imagebld writes debug ones)
"""
import os
import struct
import sys

XOR_EP_RETAIL = 0xA8FC57AB
XOR_EP_DEBUG = 0x94859D4B
XOR_KT_RETAIL = 0x5B6D40B6
XOR_KT_DEBUG = 0xEFB1F152

SEC_WRITABLE = 0x1
SEC_PRELOAD = 0x2
SEC_EXECUTABLE = 0x4

HERE = os.path.dirname(os.path.abspath(__file__))


class Section:
    def __init__(self, name, flags, va, vsize, raw, rsize):
        self.name, self.flags = name, flags
        self.va, self.vsize, self.raw, self.rsize = va, vsize, raw, rsize


class Xbe:
    def __init__(self, path):
        self.data = d = open(path, 'rb').read()
        if d[:4] != b'XBEH':
            raise ValueError('not an XBE')
        self.base, = struct.unpack_from('<I', d, 0x104)
        (self.headers_size, self.image_size, self.image_header_size,
         self.timestamp, self.cert_addr, nsec, sec_addr) = struct.unpack_from('<7I', d, 0x108)
        ep, = struct.unpack_from('<I', d, 0x128)
        kt, = struct.unpack_from('<I', d, 0x158)
        nlib, lib_addr = struct.unpack_from('<2I', d, 0x160)

        # Retail and debug XBEs scramble these with different keys; pick the one
        # that lands inside the image.
        self.retail = self._in_image(ep ^ XOR_EP_RETAIL)
        self.entry = ep ^ (XOR_EP_RETAIL if self.retail else XOR_EP_DEBUG)
        self.kernel_thunk = kt ^ (XOR_KT_RETAIL if self.retail else XOR_KT_DEBUG)

        self.sections = []
        for i in range(nsec):
            o = self._off(sec_addr) + i * 56
            flags, va, vs, raw, rs, name_addr = struct.unpack_from('<6I', d, o)
            self.sections.append(Section(self._cstr(name_addr), flags, va, vs, raw, rs))

        self.libraries = []
        for i in range(nlib):
            name, major, minor, build, _ = struct.unpack_from('<8sHHHH', d, self._off(lib_addr) + i * 16)
            self.libraries.append((name.rstrip(b'\0').decode(), major, minor, build))

        co = self._off(self.cert_addr)
        self.title_id, = struct.unpack_from('<I', d, co + 8)
        self.title_name = d[co + 12:co + 92].decode('utf-16le').split('\0')[0]

    def _in_image(self, va):
        return self.base <= va < self.base + self.image_size

    def _off(self, va):
        return va - self.base

    def _cstr(self, va):
        o = self._off(va)
        return self.data[o:self.data.index(b'\0', o)].decode('latin-1')

    def flat_image(self):
        img = bytearray(self.image_size)
        img[:self.headers_size] = self.data[:self.headers_size]
        for s in self.sections:
            o = s.va - self.base
            img[o:o + s.rsize] = self.data[s.raw:s.raw + s.rsize]
        return bytes(img)

    def kernel_imports(self):
        """[(thunk address, ordinal)] read from the kernel thunk table."""
        out = []
        o = self.flat_image()
        a = self.kernel_thunk
        while True:
            v, = struct.unpack_from('<I', o, a - self.base)
            if v == 0:
                return out
            out.append((a, v & 0x7FFFFFFF))
            a += 4


def kernel_names():
    names = {}
    with open(os.path.join(HERE, 'data', 'xboxkrnl_ordinals.txt')) as f:
        for line in f:
            if line.startswith('#') or not line.strip():
                continue
            n, name, conv, argbytes = line.split()
            names[int(n)] = (name, conv, int(argbytes))
    return names


def cmd_info(x):
    print(f'title     : {x.title_name} (0x{x.title_id:08X})')
    print(f'type      : {"retail" if x.retail else "debug"}')
    print(f'base      : 0x{x.base:08X}  image size 0x{x.image_size:X}')
    print(f'entry     : 0x{x.entry:08X}')
    print(f'kthunk    : 0x{x.kernel_thunk:08X}  ({len(x.kernel_imports())} imports)')
    print('sections  :')
    for s in x.sections:
        f = ''.join(c if s.flags & b else '-' for c, b in (('W', SEC_WRITABLE), ('X', SEC_EXECUTABLE), ('P', SEC_PRELOAD)))
        print(f'  {s.name:10} va 0x{s.va:08X}-0x{s.va + s.vsize:08X} raw 0x{s.raw:08X} rsize 0x{s.rsize:06X} {f}')
    print('libraries :')
    for name, ma, mi, b in x.libraries:
        print(f'  {name:8} {ma}.{mi}.{b}')


def cmd_layout(x, out):
    names = kernel_names()
    with open(out, 'w') as f:
        f.write(f'base {x.base:#x} {x.headers_size:#x}\n')
        f.write(f'entry {x.entry:#x}\n')
        for s in x.sections:
            f.write(f'section {s.name} {s.va:#x} {s.vsize:#x} {s.flags:#x}\n')
        for addr, n in x.kernel_imports():
            name, conv, argbytes = names.get(n, (f'Ordinal_{n}', 'stdcall', 0))
            f.write(f'kernel {addr:#x} {n} {name} {conv} {argbytes}\n')


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cmd, x = sys.argv[1], Xbe(sys.argv[2])
    if cmd == 'info':
        cmd_info(x)
    elif cmd == 'flat':
        open(sys.argv[3], 'wb').write(x.flat_image())
    elif cmd == 'layout':
        cmd_layout(x, sys.argv[3])
    elif cmd == 'extract':
        os.makedirs(sys.argv[3], exist_ok=True)
        for s in x.sections:
            open(os.path.join(sys.argv[3], s.name.strip('$.') + '.bin'), 'wb').write(x.data[s.raw:s.raw + s.rsize])
    elif cmd == 'retail':
        d = bytearray(x.data)
        struct.pack_into('<I', d, 0x128, x.entry ^ XOR_EP_RETAIL)
        struct.pack_into('<I', d, 0x158, x.kernel_thunk ^ XOR_KT_RETAIL)
        open(sys.argv[3], 'wb').write(d)
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()

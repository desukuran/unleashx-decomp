# UnleashX decompilation

A preservation project that recovers readable C++ source for **UnleashX v0.39.0528A (Build 584)**, the classic replacement dashboard for the original Xbox.

This is not a matching decomp. The aim is source that you can read, understand and modify, and that behaves like the original.

## Layout

| Path | What |
|---|---|
| `src/` | Recovered source, organized by subsystem |
| `symbols/rtti.txt` | Generated names (classes, vtables, ctors/dtors) from MSVC RTTI. Regenerate, don't edit |
| `symbols/xdklib.txt` | Generated names of the statically linked XDK library code (CRT, STL, XAPI, D3D, DirectSound, XACT, XNet, ...), matched against the XDK's own `.lib` files. Regenerate, don't edit |
| `symbols/thirdparty.txt` | Generated names of the bundled third-party code (unrar, XZip/XUnzip, XKUtils, iso9660, the FAT32 driver, the XDK samples). Regenerate, don't edit |
| `symbols/stl.txt` | Generated names of the STL template instantiations (`std::vector<SFileItem>::push_back`, ...). Regenerate, don't edit |
| `symbols/xdk.txt` | Generated XDK names and signatures from XbSymbolDatabase (overrides `xdklib.txt`) |
| `symbols/names.txt` | Hand-curated names and signatures (overrides everything else) |
| `tools/xbe.py` | XBE parser: header info, flat memory image, section extraction, retail rescrambling |
| `src/xdk/` | UnleashX's copies of the XDK sample framework, for the Xbox build |
| `xbox/` | Xbox build support: third-party adapters and stand-ins (see Building the XBE) |
| `Makefile.xbox` | The Xbox build (XDK 5933 under Wine) |
| `tools/rtti.py` | MSVC RTTI recovery (classes, base classes, vtables) |
| `tools/gen_rtti_symbols.py` | Generates `symbols/rtti.txt` |
| `tools/libmatch.py` | Generates `symbols/xdklib.txt` from a local XDK's `lib/` folder |
| `tools/msvcname.py` | Readable names from MSVC decorated names (used by `libmatch.py`) |
| `tools/srcmatch.py` | Matches bundled third-party code against its public source (strings, call order, callers) |
| `tools/stlname.py` | Generates `symbols/stl.txt`: the STL template instantiations (vector, map members and helpers) by shape and calls, with element types from `src/` |
| `tools/gen_thirdparty_symbols.py` | Generates `symbols/thirdparty.txt` with `srcmatch.py`, plus `<library>_sub_<address>` labels |
| `tools/ghidra_scripts/` | Headless Ghidra scripts: XBE setup, apply symbols, export |
| `tools/data/xboxkrnl_ordinals.txt` | Kernel export ordinals/conventions (from nxdk, CC0) |

## Getting started

You need your own copy of the UnleashX 0.39.0528A release. The binary isn't in this repo.

```sh
# Default.xbe sha1: a717b82d8506d6a21d1d5efb6fa1262c6b91f9bc
cp -r /path/to/UnleashX_0_39_0528A .
tools/setup.sh          # downloads Ghidra 12.1.4 + Python venv (needs JDK 21+)
tools/analyze.sh        # import + auto-analysis + symbols + export (~3-4 min)
tools/apply.sh          # after editing symbols/*.txt: re-apply and re-export (~2 min)
```

Output in `build/export/`:
- `functions.tsv`: address, size, section, name, caller and callee counts
- `strings.tsv`: string address, functions that reference it, text
- `decomp/<section>.c`: raw Ghidra decompiler output for every function

To work interactively, open `build/ghidra/UnleashX.gpr` in the Ghidra GUI (`tools/ghidra_12.1.4_PUBLIC/ghidraRun`).

## Building the XBE

`Makefile.xbox` builds a runnable `Default.xbe` from `src/` with the XDK
5933 toolchain (MSVC 7.1, `imagebld`) under Wine. Nothing from the XDK, the
third-party sources or the release is committed; they're all local copies:

- `reference/5933/`: the XDK (`bin/`, `include/`, `lib/`);
- `reference/thirdparty/`: Lucian Wischik's `zip_utils` (unzip), Valve's
  Source SDK 2013 `XZip.cpp`/`zip/XZip.h` (zip creation), libmad 0.15.1b,
  D. J. Bernstein's `ftpparse.c`/`.h` and XBMC4Xbox (`XKSHA1.cpp`);
- `UnleashX_0_39_0528A/`: the release, for the embedded resources
  (`MEDIA_FILE`, `DEF_FONT`, `INT_SKINS` and the title images are copied
  from its `Default.xbe`) and the XML files on the disc.

```sh
make -f Makefile.xbox -j4         # build/xbox/default.xbe (retail-scrambled)
make -f Makefile.xbox xiso        # build/xbox/UnleashX.iso
```

`xiso` uses XboxDev's extract-xiso (`git clone` it into `tools/extract-xiso`
and build it with CMake, or set `EXTRACT_XISO`). imagebld writes a debug
XBE; `tools/xbe.py retail` rescrambles its entry point and kernel thunk with
the retail keys. The image isn't signed, so it needs a modded BIOS (or xemu
with one).

`xbox/` holds what the build links in place of the third-party code that
isn't part of the decomp: `CIoSupport` and the XKUtils EEPROM getters
written from the binary, adapters onto the public zip sources, and
stand-ins that fail cleanly for unrar (RAR archives), XBMC's iso9660 (DVD
data discs in the file manager) and the FAT32 driver (USB storage). The WMV
decoder (`wmvdec.lib`) wasn't part of the XDK, so WMV videos don't play
(XMV ones do).

## Regenerating library symbols

`Default.xbe` was linked against XDK 5933's static libraries (WMVDEC and
XMOUSE excepted). With a local copy of that XDK's `lib/` folder (kept out of
the repo, e.g. in the gitignored `reference/`), every library function is
compared byte for byte with the XBE, relocated bytes masked. Unique matches
are named, and their relocations then name their callees and the globals
they use. Only names are written out.

```sh
python3 -I tools/libmatch.py UnleashX_0_39_0528A/Default.xbe reference/5933/lib
```

## Regenerating third-party symbols

UnleashX bundles third-party libraries compiled from public source:
- XBMC's UnrarXLib (unrar 3.x)
- Hans Dietrich's XZip/XUnzip
- Team Assembly's XKUtils
- XBMC's iso9660 reader

These can't be byte-matched, but `tools/srcmatch.py` pairs their functions with the source's:
- a string literal only one function uses on each side;
- the order of unknown callees between known ones;
- callers whose source shares exactly one unknown callee.

The sizes have to be plausible. Functions only reached from one library's code get a `<library>_sub_<address>` label instead. The sources are kept locally under the gitignored `reference/thirdparty/`; `tools/gen_thirdparty_symbols.py` lists where each one comes from.

```sh
tools/venv/bin/python -I tools/gen_thirdparty_symbols.py   # then tools/apply.sh
```

## Regenerating RTTI symbols

```sh
tools/venv/bin/python -I tools/gen_rtti_symbols.py UnleashX_0_39_0528A/Default.xbe \
    build/export/functions.tsv symbols/rtti.txt
```

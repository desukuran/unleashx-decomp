# Working on the decompilation

How the source in `src/` is recovered from UnleashX's `Default.xbe`, and the
tools for it. (To just build UnleashX, see [BUILDING.md](../BUILDING.md).)

The decomp needs your own copy of the UnleashX 0.39.0528A release in
`UnleashX_0_39_0528A/` (gitignored), for the binary itself. Looking up
Microsoft names and prototypes needs the XDK, kept in the gitignored
`reference/` (`reference/5933/` for the exact XDK). Neither is committed.

## Tools and generated files

| Path | What |
|---|---|
| `symbols/rtti.txt` | Generated names (classes, vtables, ctors/dtors) from MSVC RTTI. Regenerate, don't edit |
| `symbols/xdklib.txt` | Generated names of the statically linked XDK library code (CRT, STL, XAPI, D3D, DirectSound, XACT, XNet, ...), matched against the XDK's own `.lib` files. Regenerate, don't edit |
| `symbols/thirdparty.txt` | Generated names of the bundled third-party code (unrar, XZip/XUnzip, XKUtils, iso9660, the FAT32 driver, the XDK samples). Regenerate, don't edit |
| `symbols/stl.txt` | Generated names of the STL template instantiations (`std::vector<SFileItem>::push_back`, ...). Regenerate, don't edit |
| `symbols/xdk.txt` | Generated XDK names and signatures from XbSymbolDatabase (overrides `xdklib.txt`) |
| `symbols/names.txt` | Hand-curated names and signatures (overrides everything else) |
| `tools/xbe.py` | XBE parser: header info, flat memory image, section extraction, retail rescrambling |
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

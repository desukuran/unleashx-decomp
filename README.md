# UnleashX decompilation

A preservation project that recovers readable C++ source for **UnleashX v0.39.0528A (Build 584)**, the classic replacement dashboard for the original Xbox.

This is not a matching decomp. The aim is source that you can read, understand and modify, and that behaves like the original.

## Layout

| Path | What |
|---|---|
| `src/` | Recovered source, organized by subsystem |
| `symbols/rtti.txt` | Generated names (classes, vtables, ctors/dtors) from MSVC RTTI. Regenerate, don't edit |
| `symbols/names.txt` | Hand-curated names and signatures (overrides `rtti.txt`) |
| `tools/xbe.py` | XBE parser: header info, flat memory image, section extraction |
| `tools/rtti.py` | MSVC RTTI recovery (classes, base classes, vtables) |
| `tools/gen_rtti_symbols.py` | Generates `symbols/rtti.txt` |
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

## Regenerating RTTI symbols

```sh
tools/venv/bin/python -I tools/gen_rtti_symbols.py UnleashX_0_39_0528A/Default.xbe \
    build/export/functions.tsv symbols/rtti.txt
```

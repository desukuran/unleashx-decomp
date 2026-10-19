#!/bin/sh
# Downloads Ghidra and creates the Python venv used by the tools.
set -e
TOOLS=$(cd "$(dirname "$0")" && pwd)
GHIDRA_URL=https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_12.1.4_build/ghidra_12.1.4_PUBLIC_20260921.zip

if [ ! -d "$TOOLS/ghidra_12.1.4_PUBLIC" ]; then
	mkdir -p "$TOOLS/dl"
	curl -L -o "$TOOLS/dl/ghidra.zip" "$GHIDRA_URL"
	unzip -q "$TOOLS/dl/ghidra.zip" -d "$TOOLS"
	rm "$TOOLS/dl/ghidra.zip"
fi
if [ ! -d "$TOOLS/venv" ]; then
	python3 -m venv "$TOOLS/venv"
	"$TOOLS/venv/bin/pip" install -q capstone
fi

# XbSymbolDatabase (MIT): finds XDK library functions by signature. Used to
# regenerate symbols/xdk.txt:
#   tools/dl/XbSymbolDatabase/build/projects/cli/XbSymbolDatabaseCLI Default.xbe -e > build/xbsymbols.txt
#   python3 -I tools/gen_xdk_symbols.py build/xbsymbols.txt symbols/xdk.txt
if [ ! -x "$TOOLS/dl/XbSymbolDatabase/build/projects/cli/XbSymbolDatabaseCLI" ]; then
	mkdir -p "$TOOLS/dl"
	[ -d "$TOOLS/dl/XbSymbolDatabase" ] || git clone --depth 1 https://github.com/Cxbx-Reloaded/XbSymbolDatabase.git "$TOOLS/dl/XbSymbolDatabase"
	cmake -S "$TOOLS/dl/XbSymbolDatabase" -B "$TOOLS/dl/XbSymbolDatabase/build" -DXBSDB_BUILD_UNITTEST=OFF -DCMAKE_BUILD_TYPE=Release
	cmake --build "$TOOLS/dl/XbSymbolDatabase/build" -j4
fi

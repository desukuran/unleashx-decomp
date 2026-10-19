#!/bin/sh
# Imports Default.xbe into a fresh headless Ghidra project and exports
# functions/strings/decompiler output to build/export.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
XBE=${XBE:-$ROOT/UnleashX_0_39_0528A/Default.xbe}
GHIDRA=${GHIDRA:-$ROOT/tools/ghidra_12.1.4_PUBLIC}
BUILD=$ROOT/build
SYMBOLS="$ROOT/symbols/xdk.txt $ROOT/symbols/rtti.txt $ROOT/symbols/names.txt"

mkdir -p "$BUILD/ghidra" "$BUILD/export"
python3 -I "$ROOT/tools/xbe.py" flat "$XBE" "$BUILD/default.flat"
python3 -I "$ROOT/tools/xbe.py" layout "$XBE" "$BUILD/layout.txt"

python3 -I "$ROOT/tools/gen_structs.py"
cat "$ROOT/symbols/types.h" "$BUILD/structs.h" > "$BUILD/types_all.h"

"$GHIDRA/support/analyzeHeadless" "$BUILD/ghidra" UnleashX \
	-overwrite \
	-import "$BUILD/default.flat" \
	-loader BinaryLoader -loader-baseAddr 0x10000 \
	-processor x86:LE:32:default -cspec windows \
	-scriptPath "$ROOT/tools/ghidra_scripts" \
	-preScript XbeSetup.java "$BUILD/layout.txt" "$ROOT/symbols/noreturn.txt" \
	-postScript ApplyTypes.java "$BUILD/types_all.h" \
	-postScript ApplySymbols.java $SYMBOLS \
	-postScript ExportAll.java "$BUILD/export"

#!/bin/sh
# Re-applies symbols to the existing Ghidra project (no re-analysis) and
# refreshes build/export. Run tools/analyze.sh once first.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GHIDRA=${GHIDRA:-$ROOT/tools/ghidra_12.1.4_PUBLIC}
BUILD=$ROOT/build

python3 -I "$ROOT/tools/gen_structs.py"
cat "$ROOT/symbols/types.h" "$BUILD/structs.h" > "$BUILD/types_all.h"

"$GHIDRA/support/analyzeHeadless" "$BUILD/ghidra" UnleashX \
	-process default.flat -noanalysis \
	-scriptPath "$ROOT/tools/ghidra_scripts" \
	-postScript ApplyTypes.java "$BUILD/types_all.h" \
	-postScript ApplySymbols.java "$ROOT/symbols/xdk.txt" "$ROOT/symbols/rtti.txt" "$ROOT/symbols/names.txt" \
	-postScript ExportAll.java "$BUILD/export"

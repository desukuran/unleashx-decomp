#!/bin/sh
# Re-applies symbols to the existing Ghidra project (no re-analysis) and
# refreshes build/export. Run tools/analyze.sh once first.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GHIDRA=${GHIDRA:-$ROOT/tools/ghidra_12.1.4_PUBLIC}
BUILD=$ROOT/build

"$GHIDRA/support/analyzeHeadless" "$BUILD/ghidra" UnleashX \
	-process default.flat -noanalysis \
	-scriptPath "$ROOT/tools/ghidra_scripts" \
	-postScript ApplySymbols.java "$ROOT/symbols/rtti.txt" "$ROOT/symbols/names.txt" \
	-postScript ExportAll.java "$BUILD/export"

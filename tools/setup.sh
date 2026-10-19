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

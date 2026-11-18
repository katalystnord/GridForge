#!/bin/sh
# Runs macro_test.py in FreeCAD's command line program (installed, or
# FreeCAD's Flatpak) and reads its verdict. Exits 77 (skipped) without
# FreeCAD.
#
#   run-macro-test.sh SOURCE_DIR OUT_DIR
set -u
src=$1
out=$2
rm -rf "$out"
mkdir -p "$out"
if command -v FreeCADCmd >/dev/null 2>&1; then
    PYTHONDONTWRITEBYTECODE=1 MACRO=$src/tools/freecad/GridForge.FCMacro FIXTURES=$src/tests/fixtures OUT=$out \
        FreeCADCmd "$src/tests/freecad/macro_test.py" >"$out/freecad.log" 2>&1
elif command -v flatpak >/dev/null 2>&1 && flatpak info org.freecad.FreeCAD >/dev/null 2>&1; then
    # No document portal: the folders are shared by --filesystem, and with
    # the portal not running (seen 2026-11-18, late) bwrap would not start.
    flatpak run --no-documents-portal --filesystem="$src":ro --filesystem="$out" \
        --env=PYTHONDONTWRITEBYTECODE=1 --env=MACRO="$src/tools/freecad/GridForge.FCMacro" --env=FIXTURES="$src/tests/fixtures" --env=OUT="$out" \
        --command=FreeCADCmd org.freecad.FreeCAD "$src/tests/freecad/macro_test.py" >"$out/freecad.log" 2>&1
else
    echo "no FreeCAD here: skipped"
    exit 77
fi
if [ ! -f "$out/macro-test.txt" ]; then
    echo "the macro test wrote no verdict; FreeCAD said:"
    cat "$out/freecad.log"
    exit 1
fi
cat "$out/macro-test.txt"
grep -q '^0 failure(s)$' "$out/macro-test.txt"

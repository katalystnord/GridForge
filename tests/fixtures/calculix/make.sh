#!/bin/sh
# Writes the .frd fixtures from their committed .inp decks with a released
# ccx. The tests only read the committed results; this runs by hand, when a
# deck changes. ccx comes from FreeCAD's Flatpak (2.23 on 2026-11-16), or
# from CCX if set (CCX=/usr/bin/ccx tests/fixtures/calculix/make.sh).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
for deck in "$here"/*/*.inp; do
    dir=$(dirname "$deck"); job=$(basename "$deck" .inp)
    cd "$dir"
    if [ -n "${CCX:-}" ]; then
        "$CCX" -i "$job" > ccx.log 2>&1
    else
        flatpak run --filesystem="$dir" --command=ccx org.freecad.FreeCAD \
            -i "$job" > ccx.log 2>&1
    fi
    find "$dir" -type f -empty -delete   # ccx leaves empty .dat, .12d, spooles.out
    echo "$dir/$job.frd"
done

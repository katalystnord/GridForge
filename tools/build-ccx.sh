#!/bin/sh
# build-ccx.sh -- build and install the ccx that GridForge ships (CalculiX
# 2.23), from its released source, into build-ccx/install/bin/ccx.
#
# David, 2026-11-18: the AppImage bundles ccx, so every user solves with the
# version the tests and examples were solved with. ccx (GPL-2.0-only) stays
# a separate program the CalculiX toolkit's adapter runs; its source goes on
# the release page beside the AppImage (tools/appimage/README.md).
#
# Sources, pinned by SHA-256 as fetched from the official sites on
# 2026-11-18 (neither site publishes checksums):
#   ccx 2.23     http://www.dhondt.de/ccx_2.23.src.tar.bz2
#   SPOOLES 2.2  https://www.netlib.org/linalg/spooles/spooles.2.2.tgz
#                (public domain; ccx's sparse solver, built here as ccx's
#                makefile expects it, beside the CalculiX folder)
# ARPACK, LAPACK and BLAS are the system's (Ubuntu: libarpack2-dev,
# liblapack-dev, libblas-dev, gfortran).
#
# The build itself, and why its compiler settings: tools/ccx-make.sh (one
# recipe, shared with the .deb).
#
# Usage: tools/build-ccx.sh [-f]   (-f: build even if the stamp matches)

set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build="$here/build-ccx"
downloads="$build/downloads"
obj="$build/obj"
prefix="$build/install"
stamp="$prefix/.gridforge-ccx"

ccx_url=http://www.dhondt.de/ccx_2.23.src.tar.bz2
ccx_sha=9c88385c10fb04f5dc6c4e98027a51bebdd8aee3920e05190d6c1dd08357d6e7
spooles_url=https://www.netlib.org/linalg/spooles/spooles.2.2.tgz
spooles_sha=a84559a0e987a1e423055ef4fdf3035d55b65bbe4bf915efaa1a35bef7f8c5dd
want="ccx 2.23 $ccx_sha, SPOOLES 2.2 $spooles_sha"

if [ "${1:-}" != "-f" ] && [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$want" ]; then
    exit 0
fi

fetch() {  # url sha file
    if [ ! -f "$downloads/$3" ]; then
        mkdir -p "$downloads"
        curl -sSfL --retry 3 -o "$downloads/$3.part" "$1"
        mv "$downloads/$3.part" "$downloads/$3"
    fi
    echo "$2  $downloads/$3" | sha256sum -c --quiet - || {
        echo "build-ccx.sh: $3 is not the file pinned here" >&2
        exit 1
    }
}
fetch "$ccx_url" "$ccx_sha" ccx_2.23.src.tar.bz2
fetch "$spooles_url" "$spooles_sha" spooles.2.2.tgz

echo "=== Building ccx 2.23 for GridForge ==="
rm -rf "$obj"
mkdir -p "$obj/SPOOLES.2.2"
tar xjf "$downloads/ccx_2.23.src.tar.bz2" -C "$obj"
tar xzf "$downloads/spooles.2.2.tgz" -C "$obj/SPOOLES.2.2"

"$here/tools/ccx-make.sh" "$obj/SPOOLES.2.2" "$obj/CalculiX/ccx_2.23/src" "$prefix/bin/ccx"

mkdir -p "$prefix/share/doc/ccx"
for f in "$obj"/CalculiX/ccx_2.23/src/gpl-2.0.txt "$obj"/CalculiX/ccx_2.23/src/LOGBOOK "$obj"/CalculiX/ccx_2.23/src/README.INSTALL; do
    [ -f "$f" ] && cp "$f" "$prefix/share/doc/ccx/"
done
[ -f "$prefix/share/doc/ccx/gpl-2.0.txt" ] || {
    echo "build-ccx.sh: ccx's licence (gpl-2.0.txt) was not found to ship with it" >&2
    exit 1
}
"$prefix/bin/ccx" -v 2>&1 | grep -q "Version 2.23" || {
    echo "build-ccx.sh: the ccx built does not say Version 2.23" >&2
    exit 1
}
printf '%s\n' "$want" >"$stamp"
echo "ccx 2.23 in $prefix/bin/ccx"

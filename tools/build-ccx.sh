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
# Compiler settings, as found building with gcc and gfortran 15 (2026-11-18):
# SPOOLES is pre-ANSI C (-std=gnu89 -fcommon); ccx's C needs the errors gcc
# 14 and 15 added turned back into warnings, and its Fortran
# -fallow-argument-mismatch. Checked: the bent dogbone
# (examples/dogbone-bend) gives the same tip deflection and reaction to
# the last digit printed as FreeCAD's ccx 2.23.
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

make -C "$obj/SPOOLES.2.2" lib CC=gcc OPTLEVEL="-O2 -std=gnu89 -fcommon -w" >"$build/spooles.log" 2>&1 || {
    tail -20 "$build/spooles.log" >&2
    exit 1
}

jobs=$(nproc 2>/dev/null || echo 4)
make -C "$obj/CalculiX/ccx_2.23/src" -j"$jobs" CC=gcc FC=gfortran \
    CFLAGS='-Wall -O2 -I ../../../SPOOLES.2.2 -DARCH="Linux" -DSPOOLES -DARPACK -DMATRIXSTORAGE -DNETWORKOUT -std=gnu17 -Wno-error=return-mismatch -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration -Wno-error=int-conversion -w' \
    FFLAGS='-Wall -O2 -cpp -fallow-argument-mismatch -w' \
    LIBS='../../../SPOOLES.2.2/spooles.a -larpack -llapack -lblas -lpthread -lm -lc' \
    >"$build/ccx.log" 2>&1 || {
    grep -i -m20 'error' "$build/ccx.log" >&2
    exit 1
}

mkdir -p "$prefix/bin" "$prefix/share/doc/ccx"
cp "$obj/CalculiX/ccx_2.23/src/ccx_2.23" "$prefix/bin/ccx"
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

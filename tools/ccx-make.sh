#!/bin/sh
# ccx-make.sh -- build ccx 2.23 from its unpacked source and SPOOLES 2.2's:
# the one recipe, for tools/build-ccx.sh and the .deb (packaging/debian).
#
#   tools/ccx-make.sh SPOOLES_DIR CCX_SRC_DIR OUT
#
# SPOOLES_DIR holds spooles.2.2.tgz unpacked; CCX_SRC_DIR is ccx_2.23/src;
# OUT is the ccx built. ARPACK, LAPACK and BLAS are the system's.
#
# As found building with gcc and gfortran 15 (2026-11-18): SPOOLES is
# pre-ANSI C (-std=gnu89 -fcommon); ccx's C needs the errors gcc 14 and
# 15 added turned back into warnings, its Fortran -fallow-argument-mismatch.
# Checked: the bent dogbone (examples/dogbone-bend) gives the same tip
# deflection and reaction to the last digit printed as FreeCAD's ccx 2.23.
set -eu
spooles=$(CDPATH= cd -- "$1" && pwd)
src=$(CDPATH= cd -- "$2" && pwd)
out=$3
jobs=$(nproc 2>/dev/null || echo 4)

make -C "$spooles" lib CC=gcc OPTLEVEL="-O2 -std=gnu89 -fcommon -w" >"$spooles/build.log" 2>&1 || {
    tail -20 "$spooles/build.log" >&2
    exit 1
}
make -C "$src" -j"$jobs" CC=gcc FC=gfortran \
    CFLAGS="-Wall -O2 -g -I $spooles -DARCH=\"Linux\" -DSPOOLES -DARPACK -DMATRIXSTORAGE -DNETWORKOUT -std=gnu17 -Wno-error=return-mismatch -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration -Wno-error=int-conversion -w" \
    FFLAGS='-Wall -O2 -g -cpp -fallow-argument-mismatch -w' \
    LIBS="$spooles/spooles.a -larpack -llapack -lblas -lpthread -lm -lc" \
    >"$src/build.log" 2>&1 || {
    grep -i -m20 'error' "$src/build.log" >&2
    exit 1
}
mkdir -p "$(dirname -- "$out")"
cp "$src/ccx_2.23" "$out"
"$out" -v 2>&1 | grep -q "Version 2.23" || {
    echo "ccx-make.sh: the ccx built does not say Version 2.23" >&2
    exit 1
}

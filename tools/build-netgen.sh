#!/bin/sh
# build-netgen.sh -- build and install the pinned Netgen that gf_mesh links.
#
# Netgen (LGPL-2.1) is GridForge's meshing engine. It lives in
# third_party/netgen as a submodule of our fork, github.com/katalystnord/netgen:
# master tracks NGSolve's upstream, gridforge-dev carries our changes on
# top. The submodule pins the exact commit, so every GridForge build names
# the Netgen it meshes with.
#
# Built once into build-netgen/ and installed into build-netgen/install,
# which the GridForge configure finds. Built again only when the pinned
# commit changes (the stamp file records the commit that was built).
#
# Options chosen for GridForge:
#   USE_SUPERBUILD=OFF    no downloads; Ubuntu's OpenCASCADE is used
#   USE_OCC=ON            STEP and IGES geometry
#   USE_PYTHON=OFF        the engine only; Python is GridForge's own toolkit
#   USE_GUI=OFF           no Tcl/Tk window
#   USE_NATIVE_ARCH=OFF   portable binaries, the same mesh on every machine
#   NG_FP_CONTRACT=OFF    no fused multiply-add: reproducible across platforms
#
# Usage: tools/build-netgen.sh [-f]   (-f: build even if the stamp matches)

set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
src="$here/third_party/netgen"
build="$here/build-netgen"
prefix="$build/install"
stamp="$prefix/.gridforge-netgen-commit"

if [ ! -f "$src/CMakeLists.txt" ]; then
    git -C "$here" submodule update --init third_party/netgen
fi

commit=$(git -C "$src" rev-parse HEAD)

# Netgen names its version from the tag before the commit (git describe):
# a shallow clone without tags, as CI's checkout makes, gave "6.2.0" for
# the commit a full clone calls 6.2.2608-104-g7420b2f9 (CI, 2026-11-18).
# The same commit must be named the same everywhere, so fetch what
# describe needs when it is missing.
if [ "$(git -C "$src" rev-parse --is-shallow-repository)" = true ]; then
    git -C "$src" fetch --quiet --unshallow --tags origin
elif ! git -C "$src" describe --tags --match 'v[0-9]*' >/dev/null 2>&1; then
    git -C "$src" fetch --quiet --tags origin
fi
if [ "${1:-}" != "-f" ] && [ -f "$stamp" ] && [ "$(cat "$stamp")" = "$commit" ]; then
    exit 0
fi

echo "=== Building Netgen $(git -C "$src" describe --tags --always) for GridForge ==="
cmake -S "$src" -B "$build/obj" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DUSE_SUPERBUILD=OFF \
    -DUSE_OCC=ON \
    -DUSE_PYTHON=OFF \
    -DUSE_GUI=OFF \
    -DUSE_NATIVE_ARCH=OFF \
    -DNG_FP_CONTRACT=OFF \
    -DUSE_JPEG=OFF -DUSE_MPEG=OFF -DUSE_CGNS=OFF -DUSE_MPI=OFF \
    -DENABLE_UNIT_TESTS=OFF \
    -DBUILD_STUB_FILES=OFF >/dev/null
cmake --build "$build/obj"
cmake --install "$build/obj" >/dev/null
printf '%s\n' "$commit" > "$stamp"

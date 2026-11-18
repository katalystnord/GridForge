#!/bin/sh
# make-deb.sh -- GridForge's .deb packages, gridforge and gridforge-calculix,
# from the commit checked out (packaging/debian; its README.source).
#
# Usage: tools/deb/make-deb.sh [output-dir]
#
# Writes the source package (GridForge's tarball and its netgen, ccx and
# spooles components) and builds both binary packages from it with
# dpkg-buildpackage, then checks them with lintian. For Ubuntu 26.04 and
# newer (the first whose VTK is built with Qt 6).

set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
out=${1:-"$here/dist/deb"}
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd)
version=$(sed -n 's/^project(GridForge VERSION \([0-9.]*\).*/\1/p' "$here/CMakeLists.txt")
build="$here/build-deb"
src="$build/gridforge-$version"

if [ -n "$(git -C "$here" status --porcelain --untracked-files=no)" ]; then
    echo "make-deb: the tree has uncommitted changes; the package is made from HEAD only" >&2
fi

# ccx's and SPOOLES's tarballs, fetched and checked against their pins.
"$here/tools/build-ccx.sh"

rm -rf "$build"
mkdir -p "$build/netgen-tmp"
git -C "$here" archive --format=tar --prefix="gridforge-$version/" HEAD | xz -T0 >"$build/gridforge_$version.orig.tar.xz"

ng="$here/third_party/netgen"
commit=$(git -C "$ng" rev-parse HEAD)
git -C "$ng" archive --format=tar --prefix=netgen/ "$commit" | tar x -C "$build/netgen-tmp"
# Netgen reads its version from version.txt when there is no git
# (cmake/generate_version_file.cmake); the commit, for GridForge's provenance.
git -C "$ng" describe --tags --match 'v[0-9]*' --long >"$build/netgen-tmp/netgen/version.txt"
printf '%s\n' "$commit" >"$build/netgen-tmp/netgen/COMMIT"
tar cJf "$build/gridforge_$version.orig-netgen.tar.xz" -C "$build/netgen-tmp" netgen
cp "$here/build-ccx/downloads/ccx_2.23.src.tar.bz2" "$build/gridforge_$version.orig-ccx.tar.bz2"
cp "$here/build-ccx/downloads/spooles.2.2.tgz" "$build/gridforge_$version.orig-spooles.tar.gz"

# The tree dpkg-buildpackage builds: GridForge, each component in the folder
# named for it (as dpkg-source unpacks them), and debian/.
tar xJf "$build/gridforge_$version.orig.tar.xz" -C "$build"
mkdir -p "$src/netgen" "$src/ccx" "$src/spooles"
tar xJf "$build/gridforge_$version.orig-netgen.tar.xz" -C "$src/netgen" --strip-components=1
tar xjf "$build/gridforge_$version.orig-ccx.tar.bz2" -C "$src/ccx" --strip-components=1
tar xzf "$build/gridforge_$version.orig-spooles.tar.gz" -C "$src/spooles"
cp -a "$here/packaging/debian" "$src/debian"

(cd "$src" && dpkg-buildpackage --no-sign >"$build/build.log" 2>&1) || {
    tail -40 "$build/build.log" >&2
    echo "make-deb: dpkg-buildpackage failed; all of it in $build/build.log" >&2
    exit 1
}
lintian --info "$build"/gridforge_"$version"-*.changes >"$build/lintian.log" 2>&1 || true
cp "$build"/*.deb "$build"/*.dsc "$build"/*.tar.* "$build"/*.changes "$out/"
ls -l "$out"
echo "lintian: $(grep -c '^E:' "$build/lintian.log") errors, $(grep -c '^W:' "$build/lintian.log") warnings ($build/lintian.log)"

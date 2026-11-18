#!/bin/sh
# make-appimage.sh -- GridForge as one AppImage, ccx included, from a fresh
# Release build.
#
# Usage: tools/appimage/make-appimage.sh [output-dir]
#
# Netgen (tools/build-netgen.sh) and ccx 2.23 (tools/build-ccx.sh) are
# built first. ccx goes beside the CalculiX toolkit's adapter, which finds
# it there (David, 2026-11-18: the AppImage bundles ccx); its source is
# offered on the release page beside the AppImage (README.md here).
#
# Uses linuxdeploy and its Qt plugin (vetted, widely used; tenet 4), at the
# releases and SHA-256 TenSense pins, fetched into the build tree on first
# use. They run as programs here only; nothing of theirs is in GridForge.
#
# The AppImage carries Qt, VTK, OpenCASCADE, Netgen and every other
# library the programs need, but not the C library or the OpenGL driver,
# which come from the system: it runs on distributions at least as new as
# the one it was built on.

set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
out=${1:-"$here/dist"}
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd)
build="$here/build-appimage"
tools="$build/tools"

LD_TAG=1-alpha-20251107-1
LD_SHA=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
QT_TAG=1-alpha-20250213-1
QT_SHA=15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724

fetch() {  # repo tag asset sha
    dest="$tools/$3"
    if [ ! -f "$dest" ] || ! echo "$4  $dest" | sha256sum -c --quiet - 2>/dev/null; then
        mkdir -p "$tools"
        curl -fsSL -o "$dest.part" "https://github.com/$1/releases/download/$2/$3"
        echo "$4  $dest.part" | sha256sum -c --quiet - || {
            echo "make-appimage: $3 does not match its pinned SHA-256; refusing it" >&2
            rm -f "$dest.part"
            exit 1
        }
        mv "$dest.part" "$dest"
        chmod +x "$dest"
    fi
}
fetch linuxdeploy/linuxdeploy "$LD_TAG" linuxdeploy-x86_64.AppImage "$LD_SHA"
fetch linuxdeploy/linuxdeploy-plugin-qt "$QT_TAG" linuxdeploy-plugin-qt-x86_64.AppImage "$QT_SHA"

"$here/tools/build-netgen.sh"
"$here/tools/build-ccx.sh"

cmake -S "$here" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr -DGF_BUNDLE_CCX="$here/build-ccx/install" >/dev/null
cmake --build "$build"
rm -rf "$build/AppDir"
DESTDIR="$build/AppDir" cmake --install "$build" >/dev/null

version=$(sed -n 's/^project(GridForge VERSION \([0-9.]*\).*/\1/p' "$here/CMakeLists.txt")
# From TenSense's and SurView's packaging: the Qt plugin brings the
# platform, image-format and icon-engine plugins (SVG among them: every
# icon here is SVG, and without it they draw nothing, as CI showed); an
# empty LDAI_UPDATE_INFORMATION breaks the last step, so it is unset.
export QMAKE="${QMAKE:-/usr/lib/qt6/bin/qmake6}"
export EXTRA_QT_PLUGINS="${EXTRA_QT_PLUGINS:-svg;imageformats;iconengines}"
export LDAI_OUTPUT="GridForge-$version-x86_64.AppImage"
if [ -z "${LDAI_UPDATE_INFORMATION:-}" ]; then
    unset LDAI_UPDATE_INFORMATION
fi
export PATH="$tools:$PATH"
export APPIMAGE_EXTRACT_AND_RUN=1     # no FUSE needed (CI runners have none)
# Netgen is built in the tree, not installed on the system: where linuxdeploy
# is to find it, to copy it in.
export LD_LIBRARY_PATH="$here/build-netgen/install/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

kit="$build/AppDir/usr/lib/gridforge/toolkits/calculix"
cd "$out"
"$tools/linuxdeploy-x86_64.AppImage" \
    --appdir "$build/AppDir" \
    --executable "$build/AppDir/usr/bin/GridForge" \
    --executable "$build/AppDir/usr/bin/gf-study" \
    --deploy-deps-only "$kit/gf-calculix" \
    --deploy-deps-only "$kit/ccx" \
    --desktop-file "$build/AppDir/usr/share/applications/gridforge.desktop" \
    --icon-file "$build/AppDir/usr/share/icons/hicolor/scalable/apps/gridforge.svg" \
    --plugin qt \
    --output appimage
ls -l "$out/$LDAI_OUTPUT"

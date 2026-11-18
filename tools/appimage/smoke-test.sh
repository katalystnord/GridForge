#!/bin/sh
# smoke-test.sh -- the AppImage, run as a user would: it says its version,
# and solves the box study with the ccx it carries (no GF_CCX, no ccx on
# PATH), and it carries the licences.
#
# Usage: tools/appimage/smoke-test.sh GridForge-<version>-x86_64.AppImage
set -eu

image=$(CDPATH= cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1")
here=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export APPIMAGE_EXTRACT_AND_RUN=1
fail() { echo "FAIL: $*" >&2; exit 1; }

"$image" --version | grep -q '^GridForge ' || fail "--version does not say GridForge"
echo "ok: $("$image" --version | head -1)"

cd "$work"
"$image" --appimage-extract >/dev/null
root="$work/squashfs-root"
kit="$root/usr/lib/gridforge/toolkits/calculix"
[ -x "$kit/ccx" ] || fail "no ccx beside the CalculiX adapter"
[ -f "$root/usr/share/doc/ccx/gpl-2.0.txt" ] || fail "ccx's licence is not carried"
[ -f "$root/usr/share/doc/gridforge/NOTICE" ] || fail "GridForge's NOTICE is not carried"

env -u GF_CCX PATH=/usr/bin:/bin "$root/usr/bin/gf-study" solve \
    "$here/tests/fixtures/calculix/box-setup/tension.json" "$here/tests/fixtures/step/box.step" \
    --size 5 --order 2 -o "$work/job" >"$work/solve.log" 2>&1 || { cat "$work/solve.log"; fail "the box was not solved"; }
grep -q "ccx started: $kit/ccx" "$work/solve.log" || { cat "$work/solve.log"; fail "not solved by the ccx it carries"; }
grep -q "solved by CalculiX Version 2.23" "$work/solve.log" || { cat "$work/solve.log"; fail "not CalculiX 2.23"; }
echo "ok: the box solved by the ccx it carries (CalculiX 2.23)"

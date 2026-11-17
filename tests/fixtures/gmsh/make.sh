#!/bin/sh
# Writes the .msh fixtures from their committed .geo files with a released
# Gmsh (4.14.0 from Ubuntu's gmsh package on 2026-11-17: the release's
# source, built without GMSH_RELEASE, so it calls itself "4.14.0-git").
# The tests only read the committed meshes; this runs by hand, when a .geo
# changes. GMSH=/path/to/gmsh picks another.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
gmsh=${GMSH:-gmsh}
mesh() {  # folder geo options...
    folder=$1; geo=$2; shift 2
    mkdir -p "$here/$folder"
    log=$("$gmsh" "$here/$geo" -3 -format msh41 "$@" -o "$here/$folder/box.msh" 2>&1)
    # What made it and what it made, without this machine's paths.
    { echo "gmsh $("$gmsh" --version 2>&1) $geo -3 -format msh41 $*"
      echo "$log" | grep -E 'nodes .* elements|Warning|Error' || true; } > "$here/$folder/gmsh.log"
    echo "$here/$folder/box.msh"
}
incomplete="Mesh.SecondOrderIncomplete=1;"
mesh box-hex8 hex.geo
mesh box-hex8-binary hex.geo -bin
mesh box-hex20 hex.geo -order 2 -string "$incomplete"
mesh box-prism6 prism.geo
mesh box-prism15 prism.geo -order 2 -string "$incomplete"
mesh box-tet4 tet.geo
mesh box-tet10 tet.geo -order 2
# Refused: a complete second-order hexahedron (27 nodes; ccx has none),
# and the older MSH 2.2 format.
mesh refused/hex27 hex.geo -order 2
mesh refused/msh22 hex.geo -format msh22

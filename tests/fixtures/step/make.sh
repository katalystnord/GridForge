#!/bin/sh
# Writes the STEP fixtures from their .geo files with Gmsh (run once; the
# files are committed). dogbone.step is the benchmark's, copied from
# tools/mesh-bench/models.
set -eu
cd "$(dirname "$0")"
for g in box twoboxes overlapping plate ball; do gmsh "$g.geo" -0 -o "$g.step" >/dev/null; done
# tworoots.step: two bodies, each a root of the file (OpenCASCADE's writer).
c++ -std=c++17 -I/usr/include/opencascade tworoots.cpp -o /tmp/gf-tworoots \
    -lTKDESTEP -lTKXSBase -lTKPrim -lTKTopAlgo -lTKBRep -lTKG3d -lTKMath -lTKernel
/tmp/gf-tworoots
rm -f /tmp/gf-tworoots
# openbox.brep: a cube missing its top face, its open shell made a solid.
c++ -std=c++17 -I/usr/include/opencascade openbox.cpp -o /tmp/gf-openbox \
    -lTKPrim -lTKTopAlgo -lTKBRep -lTKG3d -lTKMath -lTKernel
/tmp/gf-openbox
rm -f /tmp/gf-openbox

#!/bin/sh
# Writes the STEP fixtures from their .geo files with Gmsh (run once; the
# files are committed). dogbone.step is the benchmark's, copied from
# tools/mesh-bench/models.
set -eu
cd "$(dirname "$0")"
for g in box twoboxes; do gmsh "$g.geo" -0 -o "$g.step" >/dev/null; done

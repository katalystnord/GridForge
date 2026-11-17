#!/bin/sh
# Runs inside the OpenFOAM v2512 image: each case's mesher, then checkMesh.
. /usr/lib/openfoam/openfoam2512/etc/bashrc
cd /work/snappy && blockMesh > log.blockMesh 2>&1 && surfaceFeatureExtract > log.sfe 2>&1 \
  && snappyHexMesh -overwrite > log.snappy 2>&1; checkMesh > log.checkMesh 2>&1


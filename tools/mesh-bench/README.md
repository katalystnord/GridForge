# mesh-bench: meshers measured on the same models

The benchmark behind `docs/survey/meshing.md` (2026-11-17). Every mesher
is run out of the box on the same geometry and largest element size, and
one referee, `evaluate.py`, measures every result the same way. Kept so
the numbers can be reproduced, and so GridForge's own mesher can be
measured against the same models.

## What is here

| file | does |
|---|---|
| `models.py` | the models (OpenCASCADE through Gmsh's Python API): STEP for the CAD meshers, STL for the surface meshers, and each model's exact volume and outer area in `models/models.json`. The STEP files measured are committed; the STLs are made again |
| `make_dirty.py` | `models/bracket-dirty.stl`: the bracket's surface damaged (seeded) |
| `meshers.py` | one mesher, one model, one size; prints the seconds |
| `evaluate.py` | the referee: cells, volume against the CAD's (straight and curved), mean ratio and smallest dihedral angle of tets, folded quadratic tets, scaled Jacobian of hexes, inverted cells, the boundary's area, a hash for repeatability |
| `run.py` | every mesher on every model, twice, into `results.jsonl` |
| `summarize.py` | `results.jsonl` as the tables in the study |
| `det.py` | Gmsh's repeatability with one thread and with `Mesh.Reproducible` |
| `cfd_surface.py`, `cfd/` | the duct's surface with named patches; snappyHexMesh and cfMesh cases (`cfd/*-system`), run by `cfd/run-foam.sh` inside OpenFOAM v2512 |
| `cfd_layers.py`, `netgen_layers.py` | the duct with wall layers by Gmsh and by Netgen, and plain Gmsh tets for polyDualMesh |
| `results.jsonl` | the run of 2026-11-17 |
| `gridforge-baseline.json` | GridForge's own mesher on the same models, the numbers `tests/test_mesher_benchmark.cpp` holds it to on every change. Written again with `test_mesher_benchmark --baseline FILE` when a change is meant to move them, and the change said in the commit |

## Running it

```sh
uv venv -p 3.12 .venv
uv pip install -p .venv gmsh==4.14.0 netgen-mesher==6.2.2608 numpy meshio pytetwild==0.4.2 pyvista
.venv/bin/python models.py && .venv/bin/python make_dirty.py
mkdir -p out && .venv/bin/python run.py && .venv/bin/python summarize.py
docker pull opencfd/openfoam-default:2512   # for the CFD part
```

The CFD part: copy `cfd/snappy-system` and `cfd/cfmesh-system` to
`snappy/system` and `cfmesh/system` beside the surface from
`cfd_surface.py` (in `constant/triSurface/`), then run `cfd/run-foam.sh`
in the image with the folder mounted at `/work`. Gmsh, Netgen, pytetwild,
meshio and OpenFOAM are run, not part of GridForge: their licences are
theirs (Gmsh GPL-2.0+, Netgen LGPL-2.1, fTetWild MPL-2.0, OpenFOAM
GPL-3.0+).

## What the referee was checked against

Before any mesher was measured: the box fixtures of `tests/fixtures/gmsh`
give 6000 mm3 and 2200 mm2 exactly, and a cuboid hexahedron a scaled
Jacobian of 1; quadratic tets with two middle nodes swapped, or one
middle node pushed along its edge, are all flagged.

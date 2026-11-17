# Meshing: adopt, wrap, or write our own?

**Status:** study, 2026-11-17. Asked by David the same day: meshing is
GridForge's main job ("GridForge could have been called MeshForge"), so
are Gmsh or Netgen good enough to carry a next-generation mesher, or do
we write our own? And look beyond the usual meshers. Recommendation in
section 7; the decision is David's.

Sources: four research notes (open meshers; commercial meshers; other
kinds of meshes; machine learning), each claim checked against a primary
source, and a benchmark run on this machine (section 5), with its
scripts and raw results in `tools/mesh-bench/`.

## 1. The job, per application

| | CalculiX | OpenFOAM® | Surrogates and agents |
|---|---|---|---|
| Cells | Only C3D4, C3D10, C3D8, C3D20, C3D6, C3D15 (CalculiX 2.22 manual): no pyramids, no polyhedra | Any polyhedra (CFD Direct user guide v12, "mesh description") | Not tied to an element type; recent models (GINO, Transolver) take points, normals and a distance field |
| What matters | Second order on curved faces; shape quality; parts sharing nodes; named sets that survive a geometry change | Wall layers with full coverage; non-orthogonality and skewness inside checkMesh's limits; cell count | Thousands of meshes made unattended; the same mesh from the same input; stable group names; template morphing |
| Hard parts | Dirty STEP; small features; multi-part contact | Layers at corners; dirty surfaces | Determinism; a machine-readable quality report |

The machine-learning note found no learned mesher that builds valid
volume meshes for real CAD (2D or low-genus demonstrations only). What
works is a model predicting a **sizing field** or refinement decisions,
with a classical mesher building the cells (MeshingNet, arXiv
2004.07016; ASMR, arXiv 2304.00818). So ML changes the mesher's
**interface**, not its kernel: an external sizing field, local refine and
coarsen steps, template morphing, determinism, and a JSON quality report.
Agents that set up CAE (Foam-Agent, arXiv 2505.04997; FeaGPT with
CalculiX, arXiv 2510.21993) fail mostly at geometry and meshing, and need
text specifications and readable failure reports.

## 2. The bar set by commercial meshers

Recurring across Ansys Fluent Meshing, Simcenter STAR-CCM+, Fidelity
Pointwise, ANSA and HyperMesh (vendor documentation and papers; details
in the research note):

1. Wrapping dirty CAD into a watertight surface that keeps sharp features.
2. Hex-core or poly-hexcore volumes with conformal transitions (Ansys
   reports 20 to 50 % fewer cells than hexcore; Zore et al. 2019, an
   Ansys paper).
3. Prism layers that handle corners and collisions. Siemens shipped a
   corner-retraction fix in STAR-CCM+ 2602 (February 2026): unsolved even
   there.
4. Guided task workflows that are also scripts saved as files (Fluent
   `.wft`, ANSA scenarios).
5. Meshing driven by quality criteria with automatic repair (HyperMesh
   criteria files).
6. Per-part settings, parallel per-part meshing.
7. Defeaturing and virtual topology for structural hex and sweep meshes.

No vendor ships ML that meshes, as far as their documentation shows: AI
appears as assistants, agents and surrogates. Analysts' time is the
cost: Sandia's study found geometry and meshing the largest share of
effort, 53 % for nonlinear structural work (SAND2005-4647).

## 3. The open meshers

| Mesher | Licence | Use | Strongest | Biggest gap |
|---|---|---|---|---|
| Gmsh 4.14 | GPL-2.0+ | separate process | Broad CAD to mesh; fast parallel tets (HXT); high order | 3D layers are "a simple extrusion: no fans, no special treatments of reentrant corners" (its manual); all-hex only by subdivision; not repeatable multi-threaded |
| Netgen 6.2 | LGPL-2.1 | **may be linked** | CAD-native tets, best shapes, prism layers, repeatable | Layers still maturing (its own tests: "version 2 not working yet"); no hex or polyhedral output |
| fTetWild | MPL-2.0 | linked or process | Meshes broken surfaces | Linear tets only; surface approximated; no CAD faces; not repeatable (issue #72) |
| wildmeshing-toolkit | MIT | linked | Multi-region, remeshing, offsets (v2.0.0, 2026-10-30) | New; not benchmarked here |
| MMG | LGPL-3.0+ | linked | Metric adaptation; level-set cutting (`-ls`) | Not a CAD mesher |
| CGAL Mesh_3, Alpha wrap | GPL-3.0+ or commercial | separate process | Feature protection; unconditionally robust wrapping | GPL; no layers |
| snappyHexMesh | GPL-3.0+ | separate process | Hex-dominant CFD from STL | Drops layers wherever quality checks fail (its user guide) |
| cfMesh | GPL-3.0+ | separate process | Hex-dominant and polyhedral | Layers tied to the boundary cell; better layers only in the commercial version |
| SMESH (Salome) | LGPL-2.1 | heavy | Hosts many algorithms; viscous layers; dual mesh | Tied to the Salome stack |

## 4. Beyond the usual meshers

- **Non-body-fitted** (octree, cut cells, immersed boundaries): never
  fails on dirty geometry, but CalculiX cannot solve on cut cells. Voxel
  C3D8 meshes into CalculiX are proven (ciclope, MIT, JOSS
  10.21105/joss.04952), with stair-stepped surfaces: fine for stiffness
  and surrogate data, wrong for peak stress.
- **Polyhedral and Voronoi**: native to OpenFOAM, impossible in
  CalculiX. `polyDualMesh` turns tets into polyhedra today (measured in
  section 5); VoroCrust (ACM TOG 2020, permissive licence, quiet since
  2024) is the reference for conforming Voronoi meshes.
- **Implicit geometry and level sets**: a distance field (OpenVDB,
  Apache-2.0) cut into a tet mesh by MMG `-ls` (LGPL) gives body-fitted,
  region-tagged tets: a route from lattices, scans and implicit CAD to
  CalculiX.
- **Hex meshing from computer graphics** (frame fields, integer grid
  maps, polycubes; AlgoHex, AGPL): benchmark-grade, not robust on
  arbitrary CAD yet. Watch, wrap as an optional toolkit later.
- **Isogeometric analysis**: needs the same volume parameterisation as
  hex meshing; could be sampled into curved C3D20. Not now.
- **Not a fit**: virtual element and polyhedral FEM (CalculiX has no
  such element), hex-dominant with pyramids for CalculiX, meshfree
  methods (fracture and impact, other solvers).

## 5. The benchmark

Run 2026-11-17 on this machine (24 cores; 8 threads for every mesher):
Gmsh 4.14.0, Netgen 6.2.2608, fTetWild through pytetwild 0.4.2, and
OpenFOAM v2512 (`opencfd/openfoam-default:2512`) for snappyHexMesh,
cfMesh, polyDualMesh and checkMesh. Every mesher was run **out of the
box**: the same geometry and largest element size, otherwise its own
defaults. Each configuration ran twice. One referee
(`tools/mesh-bench/evaluate.py`) measures every result the same way; it
was checked first against today's box fixtures (6000 mm3 and 2200 mm2
exactly, a cuboid hexahedron at scaled Jacobian 1) and against broken
quadratic tets (every one flagged).

The models, made with OpenCASCADE and kept as STEP and STL: a dogbone; an
L bracket with a fillet, a rib and four holes; an open thin-walled box
(1.5 mm walls); two parts touching (a block and a cylinder); a bent duct
with a rod across it; and the bracket's STL damaged (corners nudged so
they no longer meet, 12 triangles missing, 40 doubled).

### 5.1 Structural meshes (tets for CalculiX)

Mean shape is the mean ratio (1 for a regular tetrahedron); the volume
error is of the curved second-order mesh.

| | Gmsh Delaunay | Gmsh HXT | Netgen | fTetWild |
|---|---|---|---|---|
| Succeeded | 5 of 5 | 5 of 5 | 5 of 5 | 5 of 5 |
| Time, bracket | 0.21 s | 0.11 s | 0.40 s | 166 s |
| Mean shape, bracket | 0.795 | 0.796 | **0.864** | 0.858 |
| Smallest dihedral angle, worst model | 0.9 degrees (thin wall, a sliver) | 12.8 | 8.2 | 8.3 |
| Second-order nodes on the curved CAD | yes (error 0.0006 %) | yes | yes | no second order |
| Inverted or folded quadratic tets | 0 | 0 | 0 | |
| Same mesh twice | **no** | **no** | **yes** | no |
| Two parts share nodes, from STEP | **no** (+10.3 % boundary) | **no** | **no** (+10.3 %) | merged into one body: identity lost |
| Damaged STL (5.4) | **failed** | | **failed** | ok, 730 s |

Repeatability, the bracket at 1.5 mm, hashed node for node:

| | 1 thread, twice | 8 threads, twice | 1 against 8 |
|---|---|---|---|
| Gmsh Delaunay | same | different | different |
| Gmsh HXT | same | different | different |
| Gmsh with `Mesh.Reproducible` on | | **different** | |
| Netgen | same | **same** | **same** (and 3x faster on 8) |

All-hex by Gmsh's subdivision: every model meshed, mean scaled Jacobian
0.49 to 0.56, worst 0.06 to 0.18: hexahedra worse shaped than the
tetrahedra they replace.

### 5.2 CFD meshes (the duct, 5 wall layers, checkMesh)

| | cells | layers | max non-orthogonality | checkMesh |
|---|---|---|---|---|
| snappyHexMesh | 41,067 (32,494 hex) | **4.1 of 5 on average, 82 % of the thickness** | 56.9 | OK |
| cfMesh | 205,707 (200,454 hex) | built; **no coverage reported** | 40.4 | OK |
| Netgen | 103,153 (56,600 prisms) | **complete**: 5 prisms on each of the 11,320 wall triangles | 59.8 | OK |
| Gmsh (`extrudeBoundaryLayer`) | **failed**: "A segment and a facet intersect", at the rod's junction with the pipe (the duct without the rod meshes in 1.5 s) | | | |
| Gmsh tets, no layers | 75,589 | none | 68.9 | OK |
| the same, through polyDualMesh | **15,633 polyhedra** | none | **46.5** | OK |

Along the way: Netgen's own OpenFOAM exporter crashed ("double free")
and left empty files, and its Gmsh 2 exporter does not write the layers'
quadrilateral faces; the mesh reached checkMesh through our own writer.
Gmsh's 3D layer example (`tube_boundary_layer.py`) removes the inlet and
outlet faces by hand under the comment "fixme: automate this".

### 5.3 What the numbers say

- **The volume fill is solved well enough.** Every CAD-based tet mesher
  meshed every clean model, with second-order nodes on the curved
  surfaces and no inverted elements, in under two seconds.
- **Netgen is the best kernel here**: best-shaped tets, the only
  repeatable one, complete layers on the duct, and LGPL, so GridForge
  may link it. Gmsh is the fastest and the broadest.
- **Everything around the fill is where they fail**: joining parts (none
  did from STEP), damaged input (Gmsh and Netgen failed; fTetWild took
  730 s and dropped the CAD faces), wall layers (Gmsh failed,
  snappyHexMesh 82 %, cfMesh silent), repeatability (only Netgen),
  hex quality (all-hex by subdivision is poor), and reports (none says
  which faces lost their layers or why).
- **Polyhedra pay off for OpenFOAM**: the dual of a tet mesh has a fifth
  of the cells and much better orthogonality.

### 5.4 Damaged input

The bracket's surface as an STL (46,580 triangles), then damaged
(`make_dirty.py`), meshed at 3 mm:

| | undamaged STL | damaged STL |
|---|---|---|
| Gmsh (classify, rebuild, fill) | 1.9 s, 19,037 tets | **failed**: "A segment and a facet intersect" |
| Netgen (STL geometry) | 0.8 s, 10,106 tets | **failed**: "TRIG 46608 has 0 neighbours" |
| fTetWild | (166 s from the clean STL in 5.1) | **730 s**, 25,998 tets; volume within 0.08 %, outer area within 0.002 % of the CAD's: holes closed, doubles gone |

Only the "wild" mesher survives damage, at about 400 times the CAD
meshers' time, with a surface that approximates the input and no CAD
faces left to name. A front end that repairs while keeping face identity
is missing from every tool measured.

## 6. What "our own mesher" should mean

Not a new tetrahedral kernel. Measured, the kernels are good, and a
robust 3D Delaunay mesher is years of work for no gain. What no open tool
does well, and what makes a mesher next-generation by every source here,
is the **pipeline around the kernels**. That is GridForge's to write.

Write our own:

1. **The mesh model**: mixed cells (tet, hex, prism, polyhedron), regions,
   named groups that survive remeshing and geometry changes, provenance.
2. **The recipe**: a text file (and the local API) that says the whole
   mesh job; run twice, the same mesh, bit for bit, recorded in the
   provenance. The kernel is pinned and run so it can be repeatable
   (Netgen as is; Gmsh single-threaded).
3. **Assembly**: parts joined so they share nodes (or kept apart on
   purpose, for contact), each keeping its name.
4. **Wall layers**: our own prism-layer generator with corner treatment,
   collision handling and a coverage map that says where layers thinned
   and why. The largest gap, and the one users complain about most.
5. **Polyhedra and hex-core for OpenFOAM**: our own dual (with quality
   control, beyond polyDualMesh), later an octree hex core.
6. **Quality: measure, report, repair.** The referee written for this
   study is its seed: per-element measures, worst elements, a JSON report
   readable by people and agents, local repair.
7. **Sizing fields as an open interface**: curvature, proximity, user
   regions, and fields from a trained model, labelled as predictions.
8. **Writers per solver**: CalculiX decks, OpenFOAM polyMesh, and points,
   normals and distance fields for surrogates.

Adopt:

- **Netgen, linked**, as the default kernel for surfaces, tets and
  second order; repeatable as measured. Its layers as a baseline to beat.
- **OpenCASCADE** (already under Netgen and Gmsh) for STEP and BREP.
- **MMG, linked**, for adaptation and level-set cutting.

Run as toolkits (GPL, or not repeatable):

- **Gmsh**, for speed and its breadth (structured, transfinite); its
  .msh reader is already written.
- **snappyHexMesh and cfMesh**, for users who want them; their meshes
  read back through polyMesh.
- **fTetWild or the wildmeshing toolkit** and **CGAL's alpha wrapping**,
  for damaged input.
- Later, **AlgoHex** or similar for hexes, as research toolkits.

## 7. Recommendation

1. Treat the mesher as GridForge's core. Phase 2 gains a mesh core
   (`gf_mesh`): the model, the recipe, the quality referee and report.
   The deck writer follows it.
2. Link Netgen as the first kernel; keep Gmsh as a toolkit.
3. Write, in this order, each against the benchmark: assembly (parts
   sharing nodes), the quality report, the polyhedral dual, wall layers
   with coverage, an octree hex core.
4. Keep the benchmark as a test: every change to the mesher is measured
   against these models, and the numbers go in the release notes
   (tenet 9).

## 8. For David

- Whether this is the direction (own the pipeline, link Netgen, write
  the layers and the dual ourselves).
- Whether Netgen linked in is acceptable (LGPL-2.1, as Qt's modules
  are).
- Whether the mesh core comes before the CalculiX deck writer in Phase 2.

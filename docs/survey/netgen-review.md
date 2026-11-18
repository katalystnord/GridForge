# Netgen: what to improve, for GridForge (2026-11-18)

Two reviews of Netgen at our pin (7420b2f9, which is upstream master: our
fork, `gridforge-dev`, carries no commits of its own yet). One read the
code against `src/mesh/Mesher.cpp` and ran small experiments; the other
read upstream's commits, issues, pull requests and forum. Each finding
below is marked **checked** (by us, since) or **reported** (by the review,
with its source, not yet checked by us).

## Fixed

1. **A STEP file with several roots lost all but the first, silently.**
   Netgen's `LoadOCCInto` keeps `step_shapes.Value(1)` only
   (`libsrc/occ/occgeom.cpp:1654`). **Checked**: two cubes, one root
   each, gave 28 tetrahedra against 84 for the same cubes as one FreeCAD
   assembly. Fixed at our call site (OpenCASCADE's `STEPControl_Reader`,
   every root as one shape), with the fixture `tworoots.step`. The same
   change stops Netgen reading mesh sizes stored in the file
   (`netgen_geometry_properties`, `occgeom.cpp:2565`), an input nobody saw.

## Open, from the code review

2. **OpenCASCADE errors can pass our catch.** `Standard_Failure` does not
   derive from `std::exception` (`Standard_Failure.hxx:30`), and the mesher
   catches only `std::exception`; a raw OpenCASCADE throw from the STEP
   reader, BOPAlgo (our join) or BRepMesh would end the window.
   **Reported**: follows from the types; no file that triggers it yet, so
   no failing test yet.
3. **Thin walls get one element through the thickness.** Netgen's OCC
   sizing has no face to face proximity; `closeedgefac` is off by
   default (`meshtype.hpp:1844`). Reported: on a 1.5 mm wall at maxh 5,
   4854 of 4862 tetrahedra have no interior node. For bending this is the
   case a tet mesh gets wrong. A wall thickness pass of ours, fed back as
   per-face maxh or `meshsize_points`.
4. **Every sizing parameter on a hidden default.** Done 2026-11-18 for
   the provenance: each is now set by GridForge and named with the mesh,
   with OpenCASCADE's version. Making them choosable in study.json is a
   spec addition, still to be proposed. The review's words: We set only `maxh`:
   `grading` 0.3, `curvaturesafety` 2, `optsteps3d` 3, `nthreads` 4 and
   others run as Netgen defaults (`meshtype.hpp:1804-1909`). State each in
   the recipe and the provenance. Local size per solid, face, edge and
   vertex exists (`ShapeProperties::maxh`, `basegeom.hpp:26`).
5. **Second-order elements are never validated.** `ValidateSecondOrder`
   is commented out (`secondorder.cpp:421`); its repair straightens middle
   nodes until elements are valid. Reported: 0 bad elements on the
   benchmark models today, so insurance.
6. **Reproducibility across machines is not covered by our tests.**
   Reported: bit-identical meshes at 1, 4 and 16 threads (the parallel
   searches sort their candidates; smoothing goes colour by colour). What
   can still differ is OpenCASCADE's own triangulation, run in parallel,
   which sizes curved faces: the OpenCASCADE version and build matter.
   Name `OCC_VERSION_COMPLETE` in the provenance; set `nthreads`; commit
   mesh hashes of the benchmark models and compare them in CI. Done
   2026-11-18: all three.
7. **A failed mesh gives no reason.** Netgen prints the failing face to
   stdout and returns 1 (`basegeom.cpp:1080`, `1657`). Name the faces
   left without surface elements. A cancel through
   `multithread.terminate` returns 0 with a partial mesh
   (`basegeom.cpp:1611`): any Cancel must check that flag itself.
8. **Global state.** A static XCAF application and a growing static
   property map (`occgeom.cpp:84-97`, `1617`) are the likely cause of the
   crash two concurrent meshes gave (not checked); keep the mutex, or
   mesh in a child process (crashes isolated, cancel real).

## Open, from upstream

9. **An external size field** (a callback or a VTK grid of sizes, taken
   as a minimum, and a size per solid): the interface for local
   refinement and later learned sizing; upstream users ask for the same
   ([#152](https://github.com/NGSolve/netgen/issues/152),
   [PR #230](https://github.com/NGSolve/netgen/pull/230)). Fork first.
10. **Cancel and progress per meshing call** in place of the global
    `multithread` ([#73](https://github.com/NGSolve/netgen/issues/73)).
11. **15-node prisms for CalculiX** if layered meshes reach it: Netgen
    makes 12-node ones ([PR #243](https://github.com/NGSolve/netgen/pull/243)).
12. **On every pin bump, run the benchmark again**: upstream changed
    curvature sizing (6b346a65) and made OCC parsing parallel (0d2178c4)
    within the last months.
13. **Wall layers** (later, for OpenFOAM): layers at re-entrant corners
    do not work upstream yet (`tests/pytest/test_boundarylayer.py`, its
    own "not working yet"). Measure first, fix in the fork what looks like
    a bug, write our own only for what remains.
14. **MMG**, linked as a library, for adaptation and metric remeshing
    (LGPL; its exact version not checked).

## Contributing upstream

NGSolve asks for a signed CLA that lets TU Wien relicense
(`CONTRIBUTING.md`, `CLA.pdf` in their repository), and GitHub pull
requests can sit for years. So: keep our changes in the fork, small and
rebased, and offer them upstream without depending on them landing.

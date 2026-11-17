# Roadmap (DRAFT, 2026-11-14, for David's review)

From the survey ([docs/survey/](docs/survey/README.md)). Sizes are
relative (S, M, L), not dates. Each phase ends with its exit criteria met
and verified by driving the real thing.

The order puts first what is useful on its own: the CalculiX results
hand-off serves everyone who runs ccx today (from FreeCAD, PrePoMax or by
hand) before there is any window.

## Phase 0: Foundations (S) - started 2026-11-14

- Repository, CMake skeleton (`gf_core` headless, `gf_gui`, `GridForge`,
  `gf-study`), test regime, hook, CI. Done in the skeleton commit.
- CLAUDE.md with tenets (DRAFT until David settles them).
- Written specs, reviewed by David before code: toolkits (v1, drafted
  2026-11-14), the study file (v1) and
  the results hand-off (v1: what a VTKHDF/.pvd from us holds).
- The name: GridForge (David, 2026-11-15); trademark check done
  2026-11-16, clear in the EU, Sweden and WIPO.
- Exit: CI green; tenets settled; specs approved.

Toolkits (David, 2026-11-14): CalculiX and OpenFOAM® are selectable
toolkits behind one door (`docs/specs/toolkits-v1.md`), and Python and AI
surrogates follow through the same door. The manifests and the offer
logic are in Phase 0; each phase below adds to the door what it needs.

## Phase 1: CalculiX results to ParaView (M)

- `.frd` reader, ASCII and binary, own C++ (cgx manual chapter 11 as the
  specification; FreeCAD's LGPL reader as a reference). Done 2026-11-16:
  long ASCII and ccx's binary, several increments; he8, he20, te4, te10,
  pe6 and pe15, and tr3, tr6, qu4, qu8, be2, be3 (shells and beams with
  OUTPUT=2D, plane elements always) mapped to ParaView cells, each proven
  by a fixture. Without OUTPUT=2D ccx writes shells and beams as expanded
  solids on nodes of its own, so the deck's node sets are lost (said).
- Writer: VTKHDF time series with named sets as blocks; `.vtu` + `.pvd`
  fallback. Derived von Mises and principal stresses, said as derived.
  Done 2026-11-16: one `.vtkhdf` (every time point; Model, NodeSets,
  ElementSets), the `.vtu` + `.pvd` fallback, the provenance in every file.
  The named sets come from the deck (`InpSets`: the `.frd` has none).
- `gf-study convert job.frd` on the command line. Done 2026-11-16
  (`--deck`, `--pvd`).
- Fixtures: `.frd` files written by a released ccx from small committed
  `.inp` decks, each with an independently known answer (a bar in
  tension, a cantilever); one binary, one with several increments.
  Done 2026-11-16: bar-tension, bar-sets, bar-increments, bar-binary,
  cantilever-te10, cube-* (4), shell-* (5), plane-cps4, beam-* (2).
- Exit: ParaView 6.x opens our output with every variable and set; the
  same numbers as cgx shows for the fixtures.
  Status 2026-11-16: ParaView 6.2.0 opens the `.vtkhdf` with every time
  point, variable and set. Open: ParaView 6.2 (VTK 9.7.1) drops field
  data on the blocks of a VTKHDF collection, so the provenance is in the
  file but not shown there (it is in the `.vtu` fallback). The numbers match cgx 2.21's at every node of the sixteen result
  fixtures, raw and derived (test the_numbers_match_what_cgx_shows; cgx's
  values committed beside each fixture). Not filing the VTK issue and not
  offering the reader upstream, for now (David, 2026-11-17).

## Phase 2: Study model, deck writer, running ccx (M)

- The study file (spec from Phase 0); `.inp` writer: nodes, elements,
  sets, surfaces, materials (`.FCMat` elastic part, TenSense's `*PLASTIC`
  by `*INCLUDE`), sections, steps, supports, loads; raw-keyword
  passthrough per step.
- The job folder and the progress protocol (toolkits spec, sections 4
  and 5); `gf-calculix`, the CalculiX toolkit's adapter, as their first
  user. ccx as a watched child process: log, `.sta`/`.cvg` increments,
  cancel.
  Started 2026-11-16: the `.sta` and `.cvg` parsers (`JobProgress`), with
  cutbacks and a line ccx is still writing (fixture cantilever-cutbacks).
- Meshing: Gmsh as a process, `.msh` 4.1, physical groups to sets.
  Started 2026-11-17: the `.msh` 4.1 reader (ASCII and binary, every
  element type a ccx deck takes) and its hand-off to ParaView with the
  physical groups as sets (`gf-study convert mesh.msh`), against boxes
  meshed by Gmsh 4.14.0 (fixtures/gmsh). Not yet: running Gmsh, and the
  groups into the deck (Gmsh's node order to ccx's).
- Exit: a plastic dogbone from a study file to ParaView with no hand edits,
  checked against a known answer.

## Phase 3: The window, release 0.1 (L)

- Model tree (geometry, mesh, materials, steps, results), the banner-led
  next step (as in TenSense), the generated deck shown.
- Geometry in: `.brep` + the group file from a FreeCAD macro we ship;
  STEP with picking faces; per-face fingerprints.
- Run monitor that never blocks; quick look (deformed shape, contours);
  "Open in ParaView" by handing over the file.
- Exit: walkthrough test from a FreeCAD export to a ParaView file using
  only on-screen guidance; AppImage; release 0.1.

## Phase 4: Nonlinear breadth (M)

- Several steps, contact pairs, large deformation, increment control;
  geometry swap keeping assignments.

## Phase 5: Measured against predicted (M)

- A SurView strain field and TenSense's force curve, imported as files,
  beside the CalculiX prediction; difference fields out to ParaView.

## Phase 6: OpenFOAM® back end (L)

- Case writers for both lines (Foundation v13/v14, OpenCFD v25xx/v26xx),
  meshing with snappyHexMesh or cfMesh as processes, results opened in
  ParaView through `case.foam`. Tested versions stated.

## Phase 7: More toolkits (M each)

- Python: a reference toolkit and a small job-folder library, for a
  user's own solver or post-processing.
- AI surrogates: a `solve` toolkit with trust `surrogate`, its validity
  domain and held-out error in its results; a solved run beside it in
  one click. Possibly helping make one: parameter studies with CalculiX
  as the training set (David to decide).

## Later

Physics panels from data files (the toolkits' parameter schemas), remote and container runs, parameter
studies, Elmer for heat and coupled problems.

## Needs from David

| When | Need |
|---|---|
| Phase 0 | Settle the tenets; approve the three specs (toolkits, drafted; study file and results hand-off, to write); where the repository lives (GitHub katalystnord, the forge, or both). |
| Phase 1 | Whether the test suite may use FreeCAD's Flatpak ccx (as TenSense's FreeCAD test does) or a system ccx (`calculix-ccx` 2.21 in Ubuntu; needs installing). |
| Phase 3 | Whether the AppImage bundles ccx and gmsh (with their sources on the release page) or uses the user's own. |

# What a CalculiX and OpenFOAM® GUI between FreeCAD and ParaView should be

**Survey, 2026-11-14.** Working name FluidStress (the name is David's to
decide). Three research notes sit beside this page, every claim in them
with its source: [landscape.md](landscape.md) (about 25 existing tools and
what users ask for), [formats.md](formats.md) (the file at every edge) and
[citizenship.md](citizenship.md) (licences, trademarks, how to behave in
the ecosystem). They were researched by sub-agents; the claims this page
leans on hardest were checked again by hand, and are marked so.

## The gap, in one paragraph

On Linux there is no open, maintained, standalone pre-processor for
nonlinear solid mechanics with steps, and no clean road from CalculiX's
results into ParaView. PrePoMax does the first well but is Windows only
(.NET 4.8; its developer calls a Linux port hard because ActiViz stopped
being open source), and Wine breaks between versions. FreeCAD FEM runs on
Linux but has one step per analysis (issue #11639), freezes while the
solver runs (#12706) and has SI-only legends (#6922). CalculiX writes its
results only as `.frd`, which **ParaView 6.1 cannot read** (checked by hand:
its 256 readers include OpenFOAM and VTKHDF, no `.frd`); Kitware answered
the request with a Python tutorial, and the usual converter (ccx2paraview,
GPL-3.0) reads ASCII only, writes one file per increment and drops the
named sets. On the OpenFOAM side every free standalone GUI is dead,
stalled, capped or not open source (HELYX-OS deprecated since 2016,
Visual-CFD stuck at v2112, SimFlow proprietary and capped at 200k nodes,
Stokes non-commercial); CfdOF is alive and good but lives inside FreeCAD.

## What it should be

The middle, and only the middle: **set-up, solve and hand-off**, native on
Linux, with lossless files at both ends. Not a CAD modeller (FreeCAD is),
not a post-processor (ParaView is), not a solver.

```
FreeCAD ──.brep + names──▶  FluidStress  ──.inp──▶ ccx ──.frd──▶ FluidStress ──VTKHDF/.pvd──▶ ParaView
TenForge ──.FCMat + *PLASTIC──▶  (mesh with Gmsh as a process, steps,      ──case dir──▶ OpenFOAM (later)
SurView ──measured field──▶       run, quick look, measured vs predicted)
```

Ranked, each answering a gap in the notes:

1. **Native on Linux**, Qt 6 and VTK, AppImage first. (PrePoMax's Linux
   requests; Range FEA shows the stack ships.)
2. **A step-based model tree**: geometry, mesh, materials and sets, steps
   (supports, loads, outputs per step), results. (PrePoMax's model;
   FreeCAD's one-step limit.)
3. **Nonlinear solids first**: plasticity, then contact and large
   deformation. Materials come in as TenForge writes them: a FreeCAD
   `.FCMat` card for the elastic part and a CalculiX `*PLASTIC` block,
   included unchanged (FreeCAD cards have no plasticity model; FreeCAD
   FEM keeps plasticity outside the card too).
4. **The results hand-off nobody does**: our own `.frd` reader (ASCII and
   binary), writing one time series for ParaView (VTKHDF, with `.vtu` +
   `.pvd` as the safe fallback) that **keeps the named sets**. Written in
   C++ under our licence: ccx2paraview is GPL-3.0; FreeCAD's LGPL reader
   and the cgx manual's chapter 11 (the format's specification) are the
   references.
5. **The solver as a watched child process**: the window never freezes;
   live log, increment and convergence plot from ccx's `.sta`/`.cvg`,
   cancel. (FreeCAD #12706, CfdOF #141.)
6. **A readable generated deck with raw-keyword passthrough**: the `.inp`
   is shown, and any CalculiX card can be added per step and survives
   regeneration. ("Some cards will not load", CalculiX forum.)
7. **A text study file with a schema**, the same for the window and the
   command line (`fs-study`), so studies diff, script and review.
   (code_saturne's XML case, su2gui's JSON schema.)
8. **Meshing by Gmsh as a separate process** (GPL), `.msh` 4.1 with
   physical groups that become CalculiX sets; Netgen (LGPL) as a second
   engine.
9. **Units per study**, shown everywhere (mm, t, s, MPa). (FreeCAD #6922.)
10. **Measured against predicted**: a SurView strain field and TenForge's
    force beside CalculiX's prediction, as files. Nobody provides this;
    it is why the project was first thought of.

## What it could be, later

- **OpenFOAM® back end** on the same model tree. Two case writers are
  needed: the Foundation line (v13/v14: `foamRun`, `solver
  incompressibleFluid;`, `momentumTransport`) and the OpenCFD line
  (v2506/v2606: `simpleFoam`, `turbulenceProperties`). Results need no
  converter: ParaView opens a case through an empty `case.foam` file.
- **Geometry swap** that keeps loads and supports on their faces after
  FreeCAD re-exports (PrePoMax's "regenerate history"); per-face
  fingerprints make it safe.
- **Physics panels described by data** (ElmerGUI's XML, su2gui's JSON
  schema), so a new keyword or solver is a file, not code.
- Remote or container runs (CfdOF #70, 133 comments), parameter studies,
  Elmer for heat and coupled problems.

## Geometry in: a finding that changes the plan

**FreeCAD's STEP export does not carry face names** (checked by hand in
FreeCAD 1.1.4's `ExportOCAF2.cpp`: face labels get a colour, names go on
whole objects only, and `WriterStep.cpp` never turns on OCCT's sub-shape
names). What does survive: FreeCAD's face numbers (`Face7`) equal the
surface numbers Gmsh gives the same BREP, the contract FreeCAD FEM itself
relies on. So the clean road is a **small FreeCAD macro we ship**: it
writes the part as `.brep` plus a JSON file naming groups of faces ("fixed",
"load"). STEP stays accepted (solid names, face colours), with faces picked
in the window. Face numbers change when the model is edited, so each face
gets a fingerprint and a mismatch is said, never guessed.

## Being a good citizen

From [citizenship.md](citizenship.md), short:

- **Files and command lines only** with ccx, Gmsh and OpenFOAM (GPL); never
  link them. OCCT (LGPL-2.1 with an exception asking for a notice in the
  documentation), Netgen (LGPL-2.1), VTK and Qt's LGPL modules may be
  linked; Qt Charts, Graphs, Quick 3D and Data Visualization never.
- **Treat ccx and cgx as GPL-2.0 only** (the source headers say so; a web
  page says "or later").
- If an AppImage bundles ccx or gmsh (FreeCAD's AppImage does): unmodified
  binaries, their licence texts, and the exact sources on the same release
  page. Prefer the user's own install when there is one.
- **OpenFOAM® naming**: no "OpenFOAM" or "Foam" in the product's name,
  binaries, file extensions or domains; "OpenFOAM®" at first mention; and
  OpenCFD's Disclaimer and Acknowledgement wherever the product is
  described (both checked word for word against OpenCFD's policy page; in
  the README). Support both OpenFOAM lines and say which versions are
  tested.
- **Give back where it counts**: the `.frd` reader is wanted upstream
  (ParaView Discourse request 7807, with 166 votes on Kitware's old
  tracker); offer it as a ParaView plugin or beside ccx2paraview rather
  than keeping it private. Report bugs where each project asks
  (CalculiX's GitHub issues and Discourse, bugs.openfoam.org, FreeCAD's
  GitHub, ParaView's GitLab).
- **Upstream AI policies**: FreeCAD's (2026) asks that AI assistance be
  disclosed and turns away clearly AI-generated pull requests. Fixes to
  FreeCAD itself are therefore David's to make, or to report as issues.
  Kitware's and CalculiX's policies are not checked.
- Credits in README and NOTICE, never the About dialog (house policy).

## The name (for the pool)

Web and GitHub only; a formal trademark search is separate.

- **FluidStress**: nothing found (no software, no GitHub user or repository;
  the .com does not resolve). Clean, but an ordinary phrase, so weak as a
  mark.
- **TauFlow**: taken in the same field. Tau Flow is a CFD consultancy in
  Campinas, Brazil (confirmed by hand in a FAPESP article of 2018; the
  research note says its site, tauflow.com, offers OpenFOAM courses, but
  the site did not answer from here, so whether it is still active is not
  confirmed).
- **FlowTau**: nothing found, but it is Tau Flow's words swapped, in the
  same field, and DLR's TAU is a well-known CFD solver.
- Anything with "Foam" is out by OpenCFD's guidelines.

## Not checked

Reddit and CFD Online (blocked for automated access; CFD Online quotes
come from search snippets only); the FreeCAD wiki and forum (bot wall);
USPTO and EUIPO records; Gmsh's own MSH 4.1 chapter (checked against
meshio's reader instead); Kitware's and CalculiX's policies on AI-assisted
contributions.

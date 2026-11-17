# GridForge - Context File

Open simulation set-up between FreeCAD and ParaView: CalculiX first,
OpenFOAM® later. Katalyst Nord. Licence LGPL-2.1-or-later.

**Named GridForge on 2026-11-15** (David; the working name was
FluidStress). Code uses the `gf` prefix. The survey (`docs/survey/`) keeps
the working name it was written under. Trademark check done
2026-11-16: clear in the EU, Sweden and WIPO (`docs/survey/names.md`).

The survey that shaped it: [docs/survey/](docs/survey/README.md). The
plan: [ROADMAP.md](ROADMAP.md) (DRAFT). Earlier notes:
[gridforge-notes.md](gridforge-notes.md).

# THE TENETS (settled by David, 2026-11-17)

1. **Geometry and materials in, solved and trustworthy results out.**
   Set-up, solve and hand-off; everything else only augments them.
2. **There are no robust open-source GUI tools that fill the space
   between geometry (FreeCAD) and visualisation (ParaView).** We want a
   vetted, packaged solution that reliably meshes a geometry, sets up its
   materials, loads and conditions, solves it, and shows how it responds.
3. **GridForge's domain is the mesher pipeline, the model, the readers and
   the writers**, written from the formats' specifications. If viable and
   vetted open-source solutions exist, we can include and build on them.
4. **We incorporate vetted good solutions** from wherever they come.
5. **We hold no allegiance to any solver or upstream at the code level**:
   licensing and attribution only.
6. **All interoperability with other tools in the modelling chain happens
   at the file level**, never at the model or code level. What is inside
   GridForge is one program, and its parts work together freely.
7. **The GUI is also the product.** A UX defect is a defect. The window
   never freezes while a solver runs.
8. **We break with inherited approaches** whenever that is required.
9. **We say how far results can be trusted**: what was derived, what was
   converted, which solver and version produced it.
10. **GridForge's goal is that all processing and data is reproducible
    and traceable.** The same study and recipe give the same mesh and the
    same deck. Every input, generated
    file, solver output and conversion is kept, named and traced to what
    made it, and nothing is lost silently.
11. **GridForge is built to be modular and open**, including optional
    tools such as solvers, meshers and other tools as toolkits. Each
    toolkit is treated as a separate, optional program, but can be used
    and collectively orchestrated from within GridForge, for example
    through Python.

## Settled decisions

- **Position (David, 2026-11-13):** between FreeCAD and ParaView. No CAD
  modelling (geometry arrives as files), no full post-processing (results
  leave as files). Mesh, set-up, run, quick look, measured against
  predicted.
- **CalculiX first, OpenFOAM® later**; Elmer a candidate for heat and
  coupled problems.
- **Toolkits (David, 2026-11-14):** CalculiX and OpenFOAM® are toolkits the
  user selects, and more follow (Python, AI surrogates such as an ANN
  standing in for FEM). Every toolkit, the shipped ones included, is a
  separate program behind one door: a `toolkit.json` manifest and a job
  folder (`docs/specs/toolkits-v1.md`, DRAFT). Every toolkit is optional and
  installable (David, 2026-11-16). A surrogate's results are labelled as
  predictions everywhere.
- **Python (David, 2026-11-17):** GridForge runs without Python. Python
  is an optional, installable toolkit; when installed it has full access
  to GridForge, so GridForge can serve as one large Python tool in a
  modelling toolchain (as FreeCAD and ParaView can, but without setting
  paths by hand). How it is wired (an importable module, a console in
  the window, the local API) is decided when it is built.
- **Independent kit**: TenSense and SurView hand files across; no code is
  shared or linked with them.
- **Stack:** C++20, Qt 6 (LGPL modules only: never Qt Charts, Graphs,
  Quick 3D or Data Visualization), VTK, CMake.
  Kept over newer toolkits such as egui (David, 2026-11-16): engineering
  software lives in decades, so the stack must too. The look is earned by
  design (Fusion, palette, spacing, our icon style), not by the toolkit.
- **`gf_core` is headless**: Qt Core and VTK's data model only; a
  configure-time guard and the `core_is_headless` test enforce it.
- **GPL tools are run, never linked**: ccx and cgx (GPL-2.0 only), Gmsh
  (GPL-2.0-or-later), OpenFOAM (GPL-3.0-or-later).
- **OpenFOAM® trade mark**: no "OpenFOAM" or "Foam" in our names, binaries,
  file extensions or domains; "OpenFOAM®" at first mention; OpenCFD's
  Disclaimer and Acknowledgement wherever the product is described (README).
- **Attribution** in README and NOTICE, never the About dialog
  (`katalystnord/policy-on-attribution.md`). Datasets follow
  `katalystnord/policy-on-training-and-extraction.md`.
- **Upstream contributions**: follow each project's AI policy. FreeCAD's
  turns away clearly AI-generated pull requests: changes to FreeCAD are
  David's, or issues.

## How this project is run

TenSense's and SurView's regime, carried over.

```bash
tools/run-tests.sh                              # the one command
git config core.hooksPath tools/git-hooks       # once per clone
```

1. **A failing test first**, named for the case, not the function.
2. **Then the code**, written so it can be covered: pure functions over
   values, apart from widgets, threads, files and processes.
3. **Then the full suite. Then commit.**

- A green test proves nothing until it has been seen to fail without the
  fix; record the negative check in `tests/CMakeLists.txt`.
- Fixtures are committed, never generated by the code under test, each
  with an independently known answer. Solver output used as a fixture is
  written by a released solver and kept with the deck that made it.
- Walkthrough tests never open a window on the desktop (they put
  themselves on a private X server, as TenSense's do).
- **No em-dashes, ever**: `tools/check-no-em-dashes.sh`, in the hook and
  first in CI.

## Layout

```
src/core/   gf_core   the solver-neutral model and the toolkit door, headless
toolkits/   the shipped toolkits' manifests (calculix, openfoam)
src/gui/    gf_gui    the window; GridForge
src/cli/    gf-study  the same work without a window
tests/      one executable per subject; fixtures/ committed data
docs/survey/  the 2026-11-14 survey and its research notes
docs/specs/   file contracts (study file, results hand-off): to be written
tools/      run-tests.sh, sanitize.sh, check-no-em-dashes.sh, git-hooks/
```

Back ends (`src/calculix/`, `src/gmsh/`, later `src/openfoam/`) arrive with their first
function, not before: no empty targets.

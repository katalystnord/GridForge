# Toolkits, version 1

**Status: DRAFT, for David's review (2026-11-14; updated 2026-11-16).**

David, 2026-11-14: OpenFOAM® and CalculiX are toolkits the user selects,
and more will follow: Python, and AI models (for example an ANN that
stands in for a finite-element analysis). The design must be open for
them.

## 1. What a toolkit is

A toolkit is anything that does one **stage** of a study:

| Stage | Takes | Gives | Examples |
|---|---|---|---|
| `mesh` | geometry and named face groups | a mesh with named sets | Gmsh, Netgen, snappyHexMesh, cfMesh |
| `solve` | a mesh, materials, steps, loads, supports | result fields over time | CalculiX, OpenFOAM, Elmer, an ANN surrogate, a Python solver |
| `derive` | results (and measurements) | more results | a Python post-processing script, measured against predicted |

A toolkit may offer several stages (OpenFOAM meshes with snappyHexMesh and
solves). David, 2026-11-16: every toolkit is **optional and installable**,
CalculiX, OpenFOAM® and Python included. The program runs without any;
it says which stages have no toolkit yet and how to get one. The study says which toolkit does each stage, and may run the
same study with two solve toolkits to compare them (CalculiX against a
surrogate, or both against a SurView measurement).

## 2. One door for every toolkit

Every toolkit, **the shipped CalculiX and OpenFOAM ones included**, is a
separate program reached through the same door: a **job folder** and a
**command**. There are no in-process plugins.

Why one door:

- **Licences.** ccx, Gmsh and OpenFOAM are GPL; a model runtime or a
  user's script may carry any licence. A process boundary keeps each on
  its own side (tenet 6). In-process plugins would also tie every toolkit
  to our compiler, Qt and VTK versions.
- **Any language.** A toolkit can be C++, Python, a shell script or a
  container. Python is not a special case.
- **Honesty.** The shipped toolkits use the same door as everyone else's,
  so the door is tested by real use and a third party gets nothing less.
- **A failing toolkit cannot take the window down**, and it can always be
  cancelled.

## 3. The manifest: `toolkit.json`

Each toolkit is a folder holding `toolkit.json`:

```json
{
  "format": "gridforge-toolkit",
  "version": "1.0",
  "id": "calculix",
  "name": "CalculiX",
  "toolkit_version": "0.1.0",
  "licence": "GPL-2.0-only (ccx); the adapter LGPL-2.1-or-later",
  "homepage": "http://www.calculix.de/",
  "stages": [
    {
      "stage": "solve",
      "kinds": ["structural.static", "structural.modal"],
      "features": ["plasticity", "contact", "large-deformation", "multi-step"],
      "trust": "solved",
      "parameters": "solve-parameters.schema.json"
    }
  ],
  "command": ["gf-calculix", "{job}"],
  "requires": [{"program": "ccx"}],
  "notices": []
}
```

- **`kinds`**: what the stage can do, from a shared vocabulary
  (`structural.static`, `fluid.incompressible.steady`,
  `thermal.steady`, ...). The list grows with the toolkits; a kind is a
  string, so a new one needs no new code.
- **`features`**: finer abilities within a kind. The study states what it
  needs (a plastic material needs `plasticity`), and the window offers
  only toolkits that can, **saying why** another cannot ("Elmer: no
  plasticity").
- **`trust`**: `solved` (a numerical solution of the equations),
  `surrogate` (a trained model's prediction), `derived` (computed from
  other results). Shown with every result (tenet 9); see section 6.
- **`parameters`** (not read yet): a JSON Schema for the stage's own settings. The window
  draws its panel from it (as ElmerGUI and su2gui do), so a new toolkit
  needs no new window code.
- **`command`**: the program and its arguments; `{job}` is replaced by the
  job folder's path. Relative to the toolkit's folder unless on PATH.
- **`requires`**: programs the toolkit needs, each `{"program": name}` or
  `{"one_of": [names]}` (OpenFOAM's two lines name their solvers
  differently); checked before running it, and a missing one is named.
  A program may be named by the environment variable `GF_<PROGRAM>`
  (upper case, other characters as `_`: `GF_CCX`), for one not on PATH
  (ccx from FreeCAD's Flatpak, through a wrapper); the adapter uses the
  same variable.
  (Later: the found version shown, "ccx 2.21 from /usr/bin".)
- **`notices`**: text that must be shown where the toolkit is described
  (OpenFOAM's: OpenCFD's Disclaimer and Acknowledgement).

Where toolkits are found, in order: shipped with the application; the
user's folder (`~/.local/share/<app>/toolkits/`); a study's own
`toolkits/` folder. The same id found twice: the later one is used, and
the window says so.

## 4. The job folder

The application writes, the toolkit reads; the toolkit writes, the
application reads. Nothing else passes between them.

```
job/
  job.json         the stage asked for: kind, units, parameters, and the
                   study's materials, steps, loads and supports, by set name
  geometry/        part.brep and groups.json            (mesh stage)
  mesh/mesh.vtu    VTK unstructured grid; named sets as   (solve stage)
                   cell and point data arrays
  materials/       <name>.FCMat, and <name>-plastic.inp where measured
  results/         the stage's output (mesh stage: mesh/mesh.vtu)
  work/            the toolkit's own files: its deck, case folder, logs.
                   Kept, never cleaned (tenet 10)
```

Results come back as **VTKHDF** (or `.vtu` + `.pvd`) with
`results/results.json`: the toolkit and its version, the solver and its
version, `trust`, what was converted or derived, and the time points
(`"time points"`: VTK 9.5's reader reports none for a file of one). A toolkit whose
solver writes ParaView-readable files already (OpenFOAM) may point at
them instead of copying.

## 5. While it runs

The toolkit writes **one JSON object per line** to standard output:

```
{"type": "progress", "fraction": 0.42, "text": "step 1, increment 7"}
{"type": "metric", "name": "residual force", "value": 3.1e-5, "at": 0.42}
{"type": "log", "level": "warning", "text": "..."}
{"type": "file", "path": "results/results.vtkhdf"}
```

Anything else on standard output or error is kept as the log. Exit code 0
is success. The application can stop it at any time (SIGTERM, then
SIGKILL), and the window never waits on it. On SIGTERM a toolkit stops
its own solver, keeps its work files, writes no results, says where it
had got to, and exits with 3 (cancelled). A toolkit running its solver
in a sandbox (a Flatpak) makes sure the sandbox stops too.

## 6. Surrogates (AI models)

David, 2026-11-16: small ANN models for FEM- and CFD-like calculations
come next after the solvers. A trained network that stands in for a
finite-element or a flow analysis is a
`solve` toolkit with `"trust": "surrogate"`. It must also state, in
`results.json`:

- what it was trained on (the solver and the studies), and where that
  training data came from (house rule: `policy-on-training-and-extraction.md`);
- its **validity domain** (the ranges of geometry, material and load it
  was trained over) and whether this job lies inside it;
- the error it showed against its solver on held-out cases.

The window labels surrogate results as predictions everywhere they
appear, and refuses none: outside the validity domain it says so. A
solved run of the same study can be put beside it in one click. The
model's runtime (ONNX Runtime, PyTorch, ...) lives in the toolkit's
process, never in ours.

## 7. Python: full access, optional

David, 2026-11-16: Python is a first-class way to script GridForge, with
full access to the program, but optional and installed as a toolkit.

- **The program serves its whole model through a local API**: open and
  save a study; read and change geometry groups, mesh, materials, steps,
  loads and supports; offer and run toolkits; follow a run; read results;
  and, in the window, select, show and ask. Both `gf-study` and the
  running window serve it, on a local socket only. Everything the window
  can do, the API can do.
- **The Python toolkit** brings a client library for that API and a
  console, and the job-folder library for writing `solve` or `derive`
  toolkits in Python (section 1). Installing it is what makes Python
  appear in the program; without it nothing is missing but Python.
- **Out of process.** Python runs in its own process, so its version,
  its packages and its crashes stay its own, and the window never waits
  on a script. The API is language-neutral; any program can use it.
- **Parameter studies and surrogates** use the same API: a script builds
  variants of a study, runs them through CalculiX or OpenFOAM®, and
  collects the results as a training set (section 6).

David, 2026-11-17: how Python is wired (an importable module, a console
in the window, this API) is decided when it is built; Python works both
ways, into GridForge and out from it to other tools. GridForge runs
without it. (This section's "out of process" is the first candidate, not
settled.)

## 8. What the first toolkits declare

| Toolkit | Stages | Kinds | Notes |
|---|---|---|---|
| CalculiX | solve | structural.static, structural.modal; later structural.dynamic, thermal | ccx GPL-2.0-only, run as a process |
| Gmsh | mesh | tetrahedral and hexahedral solid meshes | GPL-2.0-or-later, run as a process |
| OpenFOAM® | mesh, solve | fluid.incompressible.steady and .transient first | Foundation and OpenCFD lines as two adapters or one with a switch; OpenCFD's notices shown |

## 9. In code so far (2026-11-14)

`src/core/Toolkit.h`: manifests read and checked (a broken one is refused
with its reason, the others still read), toolkits found in folders in
order (a later one replaces an earlier one of the same id, and says so),
offers for a stage, kind and features (with the reason a toolkit cannot),
whether its programs are there, and the command for a job folder. The
shipped manifests are `toolkits/calculix/` and `toolkits/openfoam/`; their
adapter programs (`gf-calculix`, `gf-openfoam`) are not written yet, so
both report themselves as not ready. Not yet: the job folder, running,
the progress protocol, parameter schemas.

## 10. Open questions for David

1. The API's transport and shape: JSON-RPC over a local socket is the
   first candidate; to be specified before the window (Phase 3).
2. The surrogate: should the application also help make one (run a
   parameter study with CalculiX, collect the training set), or only run
   one made elsewhere?
3. Should installing a toolkit from outside (a folder someone sends) ask
   the user to confirm its command before its first run? It runs a
   program of its own choosing.

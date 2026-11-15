# Toolkits, version 1

**Status: DRAFT, for David's review (2026-11-14).**

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
solves). The study says which toolkit does each stage, and may run the
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
version, `trust`, and what was converted or derived. A toolkit whose
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
SIGKILL), and the window never waits on it.

## 6. Surrogates (AI models)

A trained network that stands in for a finite-element analysis is a
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

## 7. Python

Python enters in two ways, both through files:

- **As a toolkit**: a folder with `toolkit.json` whose command is
  `["python3", "solve.py", "{job}"]`, for any stage. A reference toolkit
  with a small Python library for the job folder (read `job.json` and the
  mesh, write results, report progress) is shipped as an example.
- **Driving the application**: `gf-study` runs a study file without a
  window, so a Python script (or any program) can build study files and
  run them, for parameter studies or for training a surrogate.

No Python interpreter is embedded in the application.

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

1. Python: is the first need a user's own solver or post-processing
   (toolkit), or scripting the application (`gf-study`), or both?
2. The surrogate: should the application also help make one (run a
   parameter study with CalculiX, collect the training set), or only run
   one made elsewhere?
3. Should installing a toolkit from outside (a folder someone sends) ask
   the user to confirm its command before its first run? It runs a
   program of its own choosing.

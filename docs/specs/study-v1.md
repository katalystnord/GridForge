# The study file, version 1

**Status: DRAFT, for David's review (2026-11-17). Written so far: the
graph (section 2). The set-up itself (geometry groups, materials, steps,
loads, supports, units) is still to write (section 4).**

## 1. What a study is

A study is a folder. `study.json` describes it; every file it uses or
makes lives inside it, so the folder alone is the whole study, and it can
be zipped, versioned and handed on (tenet 10).

```
my-study/
  study.json       the graph and the set-up
  inputs/          files brought in: geometry, material cards, measurements
  steps/<id>/      each step's own folder: its outputs, its run record,
                   and, for a toolkit, its job folder (toolkits-v1, section 4)
```

## 2. The study is a graph of steps

David, 2026-11-17: a study is a graph, not a fixed sequence. The
everyday editor stays a tree with a properties panel; the graph is the
model underneath, what Python and agents build and run, and later a
read-mostly node view shows it.

### 2.1 Steps and links

- A **step** does one thing: brings a file in (`input`), meshes, sets up,
  solves, derives. It is done either by GridForge itself (`"by":
  "gridforge"`, as `gf_mesh` meshes) or by a toolkit (`"by": {"toolkit":
  "calculix"}`), through the toolkit door.
- A step has named **inputs** and named **outputs**. An output is a file
  (or a folder) in the step's own folder.
- A **link** joins one step's output to another step's input, written on
  the input as `"step.output"`. Links carry files, never objects (tenet 6).
- The graph has **no cycles**. An input may take several outputs (a
  comparison takes two solves).
- Each step keeps its **parameters**: the mesh recipe, the solver's
  settings. Two steps of the same kind may differ only in parameters
  (one geometry, two meshes, two solves to compare).

```json
{
  "format": "gridforge-study",
  "version": "1.0",
  "steps": [
    {"id": "part", "kind": "input", "file": "inputs/dogbone.step"},
    {"id": "mesh", "kind": "mesh", "by": "gridforge",
     "inputs": {"geometry": "part.file"},
     "parameters": {"maxSize": 2, "order": 2}},
    {"id": "solve", "kind": "solve", "by": {"toolkit": "calculix"},
     "inputs": {"mesh": "mesh.mesh"},
     "parameters": {"kind": "structural.static"}},
    {"id": "surrogate", "kind": "solve", "by": {"toolkit": "dogbone-ann"},
     "inputs": {"mesh": "mesh.mesh"}},
    {"id": "compare", "kind": "derive", "by": {"toolkit": "python"},
     "inputs": {"results": ["solve.results", "surrogate.results"]}}
  ]
}
```

(The set-up the solve needs, its materials, steps, loads and supports by
set name, is section 4; how it reaches the step is part of that.)

### 2.2 Out of date, and running again

After a step runs, its **run record** (`steps/<id>/run.json`) keeps a
fingerprint of what it was made from:

- the hash of every input file;
- its parameters;
- who did it: GridForge's version and build, or the toolkit's id and
  version, and the version of the program it ran (ccx 2.23, Netgen and
  its commit);
- the hash of every output, the time it took, and its log.

A step is **out of date** when its fingerprint now would differ from the
recorded one: an input changed, a parameter changed, or the program did.
Every step downstream of an out-of-date step is out of date too.

Running a study runs only the out-of-date steps, in the graph's order;
steps on separate branches may run at the same time. A step that has
never run is out of date.

### 2.3 Repeatable, and saying when not

Each step says whether it is **repeatable**: the same fingerprint gives
the same outputs, byte for byte. `gf_mesh` is (tested); Gmsh on more than
one thread is not (the meshing study, section 5). When a repeatable
step is run again with an unchanged fingerprint and its outputs differ,
that is reported as a fault, not passed over. A step that is not
repeatable is shown as such wherever its outputs are used (tenet 9).

### 2.4 Who edits the graph

- **The window**: the tree and properties panel make and change steps;
  the user rarely sees links, because the usual chain (geometry, mesh,
  set-up, solve) is made for them.
- **Python and agents**, through the program's API (toolkits-v1,
  section 7): add, change, link and run steps; read run records.
- **A node view**, later, once there are several toolkits: the graph
  drawn, what is out of date, and what a run would redo. Mostly for
  reading; not the main way to work (David, 2026-11-17).
  Candidate to draw it (David, 2026-11-17): QtNodes,
  github.com/paceholder/nodeeditor: BSD-3-Clause, Qt 6 by default (Qt
  Widgets, no QML), active (last push 2026-11-14), no tagged releases.
  To be measured against our needs when the view is built.

## 3. Not in version 1

- **Variants** (a parameter study): one step or branch run over a set of
  parameter values, each run kept. Wanted for surrogate training
  (toolkits-v1, section 6); to be specified when the first sweep is
  built, on top of 2.1 and 2.2.
- **Loops** (optimisation, mesh refinement until converged): outside the
  graph; a script or a toolkit that runs the study repeatedly.

## 4. To write

The set-up: units; geometry and its named groups; materials (FreeCAD's
`.FCMat` and TenSense's plastic data); analysis steps; loads and
supports by set name; how they reach a solve step's job folder.

## 5. Open questions for David

1. Hashes: SHA-256 of every input and output file is simple and exact;
   for large results it costs time on every check. Hash on write and
   trust file times after that?
2. Should a study keep the outputs of earlier runs of a step (a history),
   or only the latest, with the run records of earlier ones?

# The study file, version 1

**Status: approved by David, 2026-11-17 ("that all looks sensible"):
the graph (section 2) and the set-up (section 4). Section 5's questions
are settled by the draft's own answers, listed there; David may still
overrule any.**

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

After a step runs, its **run record** (`steps/<id>/runs/<n>/run.json`) keeps a
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

## 4. The set-up

What a solve step needs besides its mesh: units, named groups of the
geometry, materials, analysis steps, supports and loads. It lives in
`study.json` under `"setup"`, shared by every solve step that uses it
(two solvers of one set-up are compared like for like).

The first worked example is the fixture `box-gfmesh`: its hand-written
`tension.inp` is exactly what the set-up below must produce (4.8), and
will be the test of it.

```json
"setup": {
  "units": "mm-N-s",
  "groups": {
    "Steel part": {"solids": [1]},
    "Back":   {"faces": [1]},
    "Side":   {"faces": [3]},
    "Bottom": {"faces": [5]},
    "Pulled": {"faces": [2]}
  },
  "materials": {
    "Steel": {"card": "inputs/Steel-Generic.FCMat"}
  },
  "assign": {"Steel part": "Steel"},
  "supports": [
    {"group": "Back",   "symmetry": true},
    {"group": "Side",   "symmetry": true},
    {"group": "Bottom", "symmetry": true}
  ],
  "steps": [
    {"name": "Pull", "kind": "static",
     "loads": [{"group": "Pulled", "displacement": {"x": 0.01}}],
     "output": ["displacement", "reaction", "stress", "strain"]}
  ]
}
```

### 4.1 Units

One consistent system per study, named, never mixed: `"mm-N-s"`
(lengths in mm, forces in N, stresses in MPa, density in t/mm3, the
CalculiX and FreeCAD FEM habit) or `"m-N-s"` (SI). Every number in the
set-up is in the study's units. A value that arrives with its own unit
(a material card's `"200000 MPa"`, `"7900 kg/m^3"`) is converted on
reading, and the conversion is kept in the run record. The solver gets
plain numbers (ccx has no units).

### 4.2 Named groups

A group names faces or solids of the geometry, by their numbers as
`gf_mesh` gives them (OpenCASCADE's order, after joining; see
`src/mesh/Mesher.h`). A group of faces becomes a node set in the mesh
(and, for a pressure, a surface: 4.6); a group of solids an element set
and a node set. Group names are what the user sees and what the deck
uses; a name a solver cannot take (ccx: a space or a comma) is written
with underscores in the deck, and the deck's comments say so.

When the geometry file changes, face numbers may change with it. Each
group also keeps each face's signature when it was made (its kind:
plane, cylinder, ...; its area; its centre). On a new geometry a face
that still matches its signature keeps its place; one that does not is
**said, never guessed**: the group is marked out of date and the window
asks. (Matching faces across CAD changes in general is a known hard
problem; version 1 only notices.)

The signatures are kept in the group, by face number (added 2026-11-18,
David approving the shape):

```json
"Pushed grip": {"faces": [6],
                "signatures": {"6": {"kind": "plane", "area": 80, "centre": [150, 0, 2]}}}
```

- `kind` is OpenCASCADE's kind of surface, in words: `plane`, `cylinder`,
  `cone`, `sphere`, `torus`, `Bezier surface`, `B-spline surface`,
  `surface of revolution`, `surface of extrusion`, `offset surface` or
  `other`.
- `area` and `centre` (the centre of the face's area) are measured on the
  geometry, not the mesh, in the geometry's lengths as `gf_mesh` reads
  them.
- A face still matches when its kind is the same, its area within a
  millionth of what it was, and its centre within a millionth of the root
  of that area: a STEP written again rounds its numbers, and a changed
  face moves further than that.
- `"signatures"` is optional. The window writes it for every group it
  makes; a group written by hand without it is not checked. A signature
  for a face the group does not hold is refused.
- A group whose faces no longer match is refused when a deck is written,
  saying what the face was and what it is now. The window says so before
  a solve is tried, and offers to keep the faces as they are now, which
  signs them again.

### 4.3 Materials

A material is a FreeCAD 1.x material card (`.FCMat`, YAML), copied into
`inputs/` so the study stays whole. Read, by their model UUIDs:

| FreeCAD model | UUID | read |
|---|---|---|
| IsotropicLinearElastic | f6f9e48c-b116-4e82-ad7f-3659a9219c50 | YoungsModulus, PoissonRatio |
| LinearElastic | 7b561d1d-fb9b-44f6-9da9-56a4f74d7536 | YoungsModulus, PoissonRatio |
| Density | 454661e5-265b-4320-8e6f-fcf6223ac3af | Density |

(FreeCAD 1.1's own cards use the first; TenSense writes the second.
Both checked in FreeCAD 1.1.4's model files.)

Plastic behaviour comes from TenSense: its `<name>-plastic.inp`, a
`*PLASTIC` block of true stress against true plastic strain, in MPa for
mm, N, s (TenSense's exports spec, section 1), named in the material:

```json
"Measured steel": {"card": "inputs/S355.FCMat", "plastic": "inputs/S355-plastic.inp"}
```

It is passed to the deck as it is, by `*INCLUDE`; its comment lines say
where it came from. A value the analysis needs and the card lacks is
refused by name: a TenSense card holds no Poisson's ratio (a tensile test
does not measure it), so the study asks for one, and records that it was
entered by hand, not measured.

### 4.4 Assigning materials

`"assign"` gives each group of solids its material. Every solid has
exactly one; an unassigned solid, or one assigned twice, is refused,
naming it. Version 1 has solid sections only (no shells or beams).

### 4.5 Analysis steps

An ordered list, one CalculiX `*STEP` each. Version 1:

| field | meaning |
|---|---|
| `name` | for people and for the results |
| `kind` | `static` first; `modal` and `dynamic` later |
| `nonlinear geometry` | large displacements (`NLGEOM`); off by default, on when a plastic material is used |
| `increments` | initial, period, smallest, largest; absent: the solver's defaults |
| `loads`, `supports` | added in this step (4.6) |
| `output` | `displacement`, `reaction`, `stress`, `strain`, `plastic strain`, ... |
| `raw` | keyword lines given by hand, written into this step unchanged and marked in the deck as such (the roadmap's passthrough) |

What a step changes **stays** for the following steps, as in CalculiX,
unless the step says `"replace": true` (ccx's `OP=NEW`).

### 4.6 Supports and loads

Supports, at the start (outside any step) or in a step:

| support | means | ccx |
|---|---|---|
| `"fixed": true` | no movement in x, y, z | `*BOUNDARY`, dofs 1 to 3 |
| `"displacement": {"x": 0.01}` | given movement of the named directions; the others free | `*BOUNDARY`, each dof given |
| `"symmetry": true` | no movement across the face | `*BOUNDARY` on the dof normal to it; version 1 takes faces normal to x, y or z, and refuses others by name |

Loads, in a step:

| load | means | ccx |
|---|---|---|
| `"force": {"x": 1000}` | a total force on the group, spread over its faces by area | `*CLOAD` at the nodes, each node's share from the faces' element areas (with quadratic elements' midside weights), summing exactly to the total |
| `"pressure": 5` | normal pressure on the group's faces, positive inward | `*DLOAD`, `Pn` on the element faces of a `*SURFACE` made from the group |
| `"gravity": {"z": -9810}` | an acceleration on solids (mm/s2 in mm-N-s) | `*DLOAD`, `GRAV`; needs density |

A displacement given in a step is a load in the user's words and a
`*BOUNDARY` in ccx's; the deck follows ccx, the set-up the user.

**Where a direction came from** (added 2026-11-18, David). A displacement
or a force may say how the user gave its direction in the window, so the
window can show the same choice again and people can read it:

| `"direction"` | means |
|---|---|
| `"+x"`, `"-x"`, ..., `"-z"` | along an axis |
| `"outward normal of face 6"`, `"inward normal of face 6"` | the face's normal when the load was made, out of the solid or into it |
| `"along edge 14"`, `"against edge 14"` | the edge's line when the load was made, from its first vertex to its last, or the other way |
| absent | given by components |

```json
{"group": "Pulled grip", "displacement": {"z": 2}, "direction": "+z"}
```

The numbers stay the record; the solver never reads `"direction"`. A
direction taken from the geometry does not follow the geometry when it
changes (ANSYS's help says the same of its own; see
`docs/survey/loads-and-supports.md`): it is measured once, and the
numbers keep it. One along an axis is checked against the numbers when
the study is read (`"+z"` with a force in x, or with a negative z, is
refused); a face's or an edge's needs the geometry, and is measured where
the window makes it. When the numbers are edited, the direction is kept
while it still holds (the same line, turned round if the numbers were),
and taken out otherwise. A movement along a direction off x, y and z needs
a local system, which version 1 does not write: the window says so and
asks for components.

### 4.7 What reaches the solve step

The solve step's job folder (toolkits-v1, section 4) gets the mesh
(`mesh/mesh.vtu`, its sets by group name) and `job.json` with the set-up
resolved: units, plain numbers, materials with their values, sets named.
The CalculiX toolkit's adapter writes the deck from it: the mesh part by
`*INCLUDE` (`calculix/InpWriter`, written), then materials, sections,
supports and steps.

### 4.8 The first test

The set-up above, with the box and Steel-Generic's values replaced by
`E = 210000`, `nu = 0.3`, must give a deck that ccx solves as
`fixtures/calculix/box-gfmesh/tension.inp` did: the same displacements,
stresses and the 126000 N reaction. (Not the same text: the hand deck
names faces by number; the written one by group.)

## 5. Settled with the approval (2026-11-17)

1. Units: mm-N-s and m-N-s only in version 1 (David, 2026-11-17).
2. A displacement given in a step is shown as a load and written as
   ccx's `*BOUNDARY` (4.6).
3. `"nonlinear geometry"` turns on by itself when a plastic material is
   used, and the deck's comments say it did; a step may turn it off
   (David, 2026-11-17: automatic).
4. Files are hashed (SHA-256) when written; after that, a file whose
   size and time are unchanged is taken as unchanged. A full check
   hashes everything again, on request.
5. A step keeps the outputs of every run, as a history, each run in its
   own folder (`steps/<id>/runs/<n>/`, with its run record), and says
   which is the latest (David, 2026-11-17).

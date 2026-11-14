# FluidStress / FlowTau: file formats at every edge

Survey date: 2026-11-14. Each claim gives the source I opened. "Not checked"
means I could not confirm it at a primary source in this session. Source files
I downloaded for grepping are in
/tmp/claude-1000/-home-david-store-code-katalystnord/f251fc0b-bfa6-4a12-8f1a-9bcdc1fd3dd6/scratchpad/fs-survey/src/

Current versions seen at source:

| Tool | Version | Source |
|---|---|---|
| FreeCAD | 1.1.4 (GitHub release, 2026-09-28) | https://api.github.com/repos/FreeCAD/FreeCAD/releases/latest |
| OCCT | 8.0.1 | https://occt3d.com/open-cascade-technology/index.html |
| Gmsh | 4.15.2 (2026-03-24) | https://gmsh.info/ |
| Netgen | v6.2.2604 latest GitHub release (tags up to v6.2.2608) | https://github.com/NGSolve/netgen |
| CalculiX ccx / cgx | 2.23 (ccx manual dated 2025-10-19, cgx manual 2025-10-04) | https://www.dhondt.de/ , https://www.dhondt.de/ccx_2.23.pdf , https://www.dhondt.de/cgx_2.23.pdf |
| OpenFOAM Foundation | v14 (2026-07-14, patch 2026-07-25) | https://openfoam.org/download/ |
| OpenFOAM ESI/OpenCFD | v2606 (2026-06-26); v2512, v2506 before | https://www.openfoam.com/news/main-news |
| ParaView | 6.1.0 (2026-03-31), 6.1.1 (2026-05-27); 6.0.0 was 2025-08-01 | https://www.kitware.com/paraview-6-1-0-release-notes/ , https://www.kitware.com/paraview-6-0-0-release-notes/ |
| PrePoMax | GPL-3.0, now at https://github.com/PrePoMax/PrePoMax (active, pushed 2026-11-05) | GitHub API |

---

## 1. Geometry in: STEP vs BREP from FreeCAD

### What FreeCAD's STEP export actually writes (source-checked, FreeCAD main)

- Writer: `STEPCAFControl_Writer` with `STEPControl_AsIs`; no
  `Interface_Static` settings and no sub-shape name switch are set.
  https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Import/App/WriterStep.cpp
- `ExportOCAF2.cpp` does create sub-shape labels per face with
  `AddSubShape`, but only to attach **colours** (`aColorTool->SetColor(subLabel, ...)`).
  `TDataStd_Name::Set` is only called on object labels, never on sub-shape
  labels. So **object/solid names survive in STEP, face names do not.**
  https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Import/App/ExportOCAF2.cpp
- OCCT itself can do it: `DESTEP_Parameters` has `WriteSubshapeNames` and
  `ReadSubshapeNames` ("whether to write/read sub-shape names to/from 'Name'
  attributes of STEP Representation Items"), both default `false`.
  `ReadName`/`WriteName` (product names) default `true`.
  https://github.com/Open-Cascade-SAS/OCCT/blob/master/src/DataExchange/TKDESTEP/DESTEP/DESTEP_Parameters.hxx
- FreeCAD STEP export dialog options: "Write out curves in parametric space
  of surface", "Export invisible objects", "Export single object placement",
  "Use legacy export function", units (mm, m, inch), Scheme: AP203, AP214 CD,
  AP214 DIS, AP214 IS, AP242 DIS.
  https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Part/Gui/DlgExportStep.ui
- FreeCAD's STEP reader sets `SetNameMode(true)`, `SetColorMode(true)`,
  `SetLayerMode(true)`, UTF-8 code page; no sub-shape-name switch.
  https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Import/App/ReaderStep.cpp
- OCCT STEP guide: reads AP203, AP214 and "some parts of AP242"; writes AP203/AP214
  (schema option also lists AP242DIS). XDE carries names, colours, layers.
  https://occt3d.com/dev/doc/overview/html/occt_user_guides__step.html
- Gmsh can import STEP labels through OCC (`Geometry.OCCImportLabels`), but
  issue reports say face-level names are unreliable:
  https://gitlab.onelab.info/gmsh/gmsh/-/issues/559 ,
  https://gitlab.onelab.info/gmsh/gmsh/-/issues/2546 (option docs page itself
  not opened: not checked).

### Face indices are the stable handle

- FreeCAD's `FaceN` is the 1-based index in `TopExp::MapShapes(shape, TopAbs_FACE)`.
  https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Part/App/TopoShape.cpp
- FreeCAD FEM itself depends on this: it exports a `.brep`, `Merge`s it in a
  Gmsh `.geo`, and writes `Physical Surface(...) = {i}` with the comment "we use
  the element index of FreeCAD which starts with 1 (example: "Face1"), same as
  Gmsh". So BREP + FreeCAD face index = Gmsh surface tag is production-proven.
  https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Fem/femmesh/gmshtools.py (lines ~1443-1475)
- `.FCStd` stores each shape as a BREP (`.brp`, or `.bin` binary) inside the zip.
  https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Part/App/PropertyTopoShape.cpp

### How a user could mark load/support faces so they arrive named

1. **BREP + JSON sidecar from a small FreeCAD macro (recommended).** The user
   selects faces, gives each group a name ("fixed", "load_top"); the macro
   writes `part.brep` (`Shape.exportBrep`) and `part.groups.json`
   (`{"fixed": [3, 7], "load_top": [12]}`). Uses exactly the index contract
   FreeCAD FEM already relies on. Macro is ours (LGPL), runs inside FreeCAD.
2. **STEP with face colours.** Colours do survive (verified above). A user
   colours load faces red, supports blue; we map colours to groups. Works with
   no macro, but colour is a weak carrier (lighting/appearance edits, two
   groups with the same colour).
3. **Our macro writes AP242 with sub-shape names.** Would need the macro to
   drive OCCT XCAF directly with `WriteSubshapeNames=true`; FreeCAD Python does
   not expose that path as far as I saw (not checked in depth).
4. **Pick faces inside our app** (what PrePoMax does). Always needed as a
   fallback, and the only option for plain STEP from other CAD.

Risk: face indices change whenever the FreeCAD model is edited
(topological naming); the sidecar must be regenerated together with the BREP,
and the app should store a geometric fingerprint (area, centroid) per named
face to detect a mismatch.

### OCCT licence for reading STEP/BREP in our app

LGPL-2.1 with the "Open CASCADE exception (version 1.0)": object code that
incorporates material from OCCT header files may be distributed under terms of
your choice, given a prominent notice in supporting documentation that it uses
OCCT. https://github.com/Open-Cascade-SAS/OCCT/blob/master/OCCT_LGPL_EXCEPTION.txt ,
https://occt3d.com/open-cascade-technology/index.html . Linking it into an
LGPL-2.1-or-later app is straightforward. (The required notice goes in docs/README,
which fits `policy-on-attribution.md`.)

---

## 2. Meshing

| Mesher | Licence (checked) | Use from our app |
|---|---|---|
| Gmsh 4.15.2 | GPL-2.0-or-later with an exception allowing combination with Netgen, METIS, OpenCASCADE and ParaView. Closed-source integration needs a commercial licence. https://gmsh.info/LICENSE.txt , https://gmsh.info/ | **Separate process** (gmsh binary or a Python script using the gmsh module). Linking libgmsh into our LGPL app would make the combination GPL. API in C++, C, Python, Julia, Fortran. |
| Netgen | LGPL-2.1 (LICENSE file title). https://github.com/NGSolve/netgen/blob/master/LICENSE | Can be **linked**. `nglib` has an OCC API: `Ng_OCC_Load_STEP`, `Ng_OCC_Load_BREP`, `Ng_OCC_GenerateSurfaceMesh`, `Ng_OCC_GetFMap`, `Ng_OCC_Generate_SecondOrder`. https://github.com/NGSolve/netgen/blob/master/nglib/nglib_occ.h |
| TetGen 1.5.0 (2013) | AGPLv3 or later; paid commercial licence from WIAS. https://wias-berlin.de/software/tetgen/1.5/FAQ-license.html | Nothing it does that Gmsh/Netgen do not; AGPL adds network-use obligations; old. Skip. Separate-process use is not addressed in the FAQ (not checked further). |
| snappyHexMesh | Part of OpenFOAM, GPL-3.0-or-later (header of OpenFOAM sources, e.g. gmshToFoam.C). | Run as a process. Takes STL or OBJ; named regions in the surface map to patches via `regions { leftOpening { name inlet; } }`. https://doc.cfd.direct/openfoam/user-guide-v13/snappyhexmesh |
| cfMesh | GPLv3 (SourceForge project page); shipped by ESI and foam-extend. https://sourceforge.net/projects/cfmesh | Process. Free version lacks some features of commercial CF-MESH+ (boundary layers: only a weak source, not checked). |

**What others use (source-checked):**
- FreeCAD FEM: Gmsh as a subprocess (BREP + `.geo`, mesh written as `.unv`
  with groups) in `femmesh/gmshtools.py`; Netgen via its Python `occ.OCCGeometry`
  run through `QProcess` in `femmesh/netgentools.py`.
  https://github.com/FreeCAD/FreeCAD/tree/main/src/Mod/Fem/femmesh
- PrePoMax (GPL-3.0, C#, Windows): Netgen as a separate `ThirdParty/netgen/NetGenMesher.exe`,
  Gmsh through the bundled `gmsh-4.13.0-Windows64-sdk`, mmg for remeshing.
  https://github.com/PrePoMax/PrePoMax

**MSH 4.1 and physical groups.** File sections: `$MeshFormat`, `$PhysicalNames`,
`$Entities` (each point/curve/surface/volume lists its physical tags),
`$Nodes` and `$Elements` in entity blocks; ASCII or binary. Checked against
the meshio 4.1 reader, not the Gmsh manual's format chapter, which I could not
reach in the single-page manual: https://sources.debian.org/data/main/p/python-meshio/7.0.0-3/src/meshio/gmsh/_gmsh41.py .
By default Gmsh only saves elements that belong to a physical group
(`Mesh.SaveAll` overrides): manual section 1.2.3, https://gmsh.info/doc/texinfo/gmsh.html .
Physical names set with `gmsh.model.setPhysicalName` (API section 6.3).

Gmsh can also write Abaqus `.inp` directly; `Mesh.SaveGroupsOfNodes` adds `*NSET`
per physical group, but forum reports show ELSET/NSET naming has had gaps
(https://calculix.discourse.group/t/gmsh-export-to-inp-missing-nsets/2641 ,
https://gitlab.onelab.info/gmsh/gmsh/issues/479). Better: read `.msh` ourselves
and write the `.inp` ourselves, including `*SURFACE` element faces, which Gmsh's
INP export does not produce as far as I saw (not checked).

**OpenFOAM side:** `gmshToFoam` (OpenFOAM-13 source) reads MSH "Version 1 or 2
format" and turns `$PhysicalNames` into patch names; it does not read MSH 4.
So for CFD export from Gmsh, set `Mesh.MshFileVersion = 2.2`, or write
`constant/polyMesh` ourselves.
https://github.com/OpenFOAM/OpenFOAM-13/blob/master/applications/utilities/mesh/conversion/gmshToFoam/gmshToFoam.C

---

## 3. CalculiX

- Licence: GPL-2 (linked from https://www.dhondt.de/). Run `ccx` as a process.
- Keyword reference: ccx 2.23 manual chapter 7 "Input deck format"
  (https://www.dhondt.de/ccx_2.23.pdf). All requested keywords exist there:
  `*NSET` (7.101), `*ELSET` (7.54), `*SURFACE` (7.129), `*MATERIAL` (7.84),
  `*ELASTIC` (7.47), `*PLASTIC` (7.106), `*STEP` (7.125), `*STATIC` (7.123),
  `*BOUNDARY` (7.4), `*CLOAD` (7.15), `*DLOAD` (7.44), `*CONTACT PAIR` (7.22),
  `*SURFACE INTERACTION` (7.131), `*SURFACE BEHAVIOR` (7.130), `*FRICTION` (7.64),
  `*TIE` (7.133), `*NODE FILE` (7.97), `*EL FILE` (7.52), `*NODE OUTPUT`, `*ELEMENT OUTPUT`,
  `*SOLID SECTION` (7.119), `*AMPLITUDE`, `*INCLUDE` (7.76).
- `*SURFACE`: NAME required, TYPE=ELEMENT (default) or NODE, cannot mix; faces
  as `Sx` with per-element-type numbering (tet: S1 = 1-2-3, S2 = 1-4-2,
  S3 = 2-4-3, S4 = 3-4-1; hex and wedge tables on p. 612).
- `*PLASTIC`: data lines are von Mises stress, equivalent plastic strain,
  temperature; first point must be the yield stress at zero plastic strain;
  HARDENING=ISOTROPIC/KINEMATIC/COMBINED/JOHNSON COOK (pp. 576-577).
- Contact: node-to-face penalty, face-to-face penalty, face-to-face mortar,
  massless node-to-face (theory 6.7.6 to 6.7.9).
- Elements (chapter 6.2): C3D4, C3D10, C3D10T, C3D8, C3D8R, C3D8I, C3D20, C3D20R,
  C3D6, C3D15, shells S3/S4/S4R/S6/S8/S8R, CPS*/CPE*/CAX*, B31/B32 beams, T3D2/T3D3,
  springs, gap, mass. Shells and beams are expanded internally to 3D elements
  (manual lines on B32 to C3D20 expansion). For a first release: C3D10
  (Gmsh/Netgen second-order tets) and C3D4 for previews.

### .frd results

- ccx writes `.frd` in ASCII with `*NODE FILE`/`*EL FILE`, and binary data
  lines with `*NODE OUTPUT`/`*ELEMENT OUTPUT`; element variables are
  extrapolated to nodes in `.frd`, exact at integration points only in `.dat`
  (ccx manual 6.13 "Output variables").
- The spec: cgx 2.23 manual chapter 11 "Result Format" (pp. 144-150): Model
  header, User header, Nodal point coordinate block (2C), Element definition
  block (3C), Parameter header, Nodal results block (100C), records `-1`, `-2`,
  `-3`, `-4`, `-5`, `9999`, fixed-column Fortran formats, short/long node-number
  forms, binary FORMAT=2 (float) / 3 (double) with no `-3` end line.
  https://www.dhondt.de/cgx_2.23.pdf ; older HTML copy:
  https://web.mit.edu/calculix_v2.7/CalculiX/cgx_2.7/doc/cgx/node168.html .
  It is documented and has kept its record structure since at least 2.7, but
  is not versioned as a separate standard; treat it as "stable in practice"
  (field-by-field diff across versions: not checked).
- Converters:
  - **ccx2paraview** (https://github.com/calculix/ccx2paraview): **GPL-3.0**, Python,
    v3.2.0; writes `.vtk` or `.vtu` plus `.pvd` for time steps; **ASCII .frd only**
    (binary on its TODO); no VTKHDF. Usable only as a separate tool or as a
    format reference; not as code in our LGPL app.
  - **FreeCAD `importCcxFrdResults.py`**: **LGPL-2.1-or-later**, ASCII only,
    fixed-column parser, blocks DISP, STRESS, TOSTRAIN, PE, NDTEMP, FLUX
    (+ network MAFLOW/STPRES), elements C3D4/6/8/10/15/20, S3/4/6/8, B31/B32;
    others silently skipped. Licence-compatible to port or learn from.
    https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Fem/feminout/importCcxFrdResults.py
  - **PrePoMax `FrdFileReader.cs`**: GPL-3.0, reads **both ASCII and binary**
    blocks (and files mixing both), also reads `.dat`. Good behavioural
    reference, cannot copy code.
    https://github.com/PrePoMax/PrePoMax/blob/master/CaeResults/FileInOut/Input/CalculiX/FrdFileReader.cs
- Conclusion: write our own C++ frd reader (ASCII + binary), from the cgx
  manual spec, tested against ccx test-suite outputs.

---

## 4. OpenFOAM

- Case layout: `0/` (initial and boundary fields, one file per field, each
  `boundaryField` entry matching a patch in `constant/polyMesh/boundary`),
  `constant/` (mesh and physical properties), `system/` (`controlDict`,
  `fvSchemes`, `fvSolution`, mesher dictionaries). Every file starts with a
  `FoamFile` header (`version`, `format`, `class`, `object`, `location`).
  https://doc.cfd.direct/openfoam/user-guide-v13/basic-file-format
- **Differences that matter to a case writer (checked on pitzDaily):**

  | | Foundation v13 (openfoam.org) | ESI v2606 (openfoam.com) |
  |---|---|---|
  | Solver choice | `solver incompressibleFluid;` in `controlDict`, run with `foamRun` (modular solvers since v11; old `simpleFoam` name runs a script) | separate application, `simpleFoam` |
  | Turbulence file | `constant/momentumTransport` | `constant/turbulenceProperties` |
  | Viscosity file | `constant/physicalProperties` | `constant/transportProperties` |
  | Tutorial path | `tutorials/incompressibleFluid/pitzDaily` | `tutorials/incompressible/simpleFoam/pitzDaily` |

  Sources: https://github.com/OpenFOAM/OpenFOAM-13/tree/master/tutorials/incompressibleFluid/pitzDaily ,
  https://openfoam.org/release/11/ , https://doc.openfoam.com/2606/quickstart .
  Boundary-condition and function-object keyword drift beyond this: not checked.
  Plan for two case-writer back-ends sharing a mesh writer.
- Boundary names: whatever writes `constant/polyMesh/boundary` names the
  patches: gmshToFoam takes `$PhysicalNames` (MSH 2 only); snappyHexMesh takes
  STL/OBJ region names or a `name` override per region.
- Post: `foamToVTK` exists in both; Foundation's user guide recommends its own
  ParaView reader module (`paraFoam`, built against ParaView 5.10.1), and says
  data can go through `foamToVTK` otherwise.
  https://doc.cfd.direct/openfoam/user-guide-v13/paraview
- ParaView has a **built-in** reader (`vtkOpenFOAMReader`) that opens a case
  directly: patches selectable, zones optional, decomposed cases via
  `vtkPOpenFOAMReader`, gzip. https://vtk.org/doc/nightly/html/classvtkOpenFOAMReader.html .
  It is triggered by an empty marker file with extension `.foam` in the case
  root (`paraFoam -builtin` / `-touch`):
  https://www.openfoamwiki.net/index.php/Case_Name_.foam_File (the ESI
  `paraFoam` script itself is on a GitLab that now requires sign-in, so I saw it
  only through search snippets).
- So: **no conversion needed for OpenFOAM results**; write `case.foam`, and
  ParaView reads it. Our quick look can use the same VTK reader in-process
  (VTK is BSD).

---

## 5. Results out to ParaView

| Format | ParaView 6.x | Time series | Named sets | Writer for us |
|---|---|---|---|---|
| `.vtu` + `.pvd` | native (VTK XML) | `.pvd` collection, one `.vtu` per step | Only as data arrays (e.g. int mask per set) or as named blocks in a `.vtm` multiblock | VTK XML writers (BSD), trivial |
| VTKHDF (`.vtkhdf`) | native; file format version 2.8; temporal `Steps` group (`NSteps`, `Values`, offsets; offsets may repeat for a static mesh); `PartitionedDataSetCollection` with an `Assembly` group whose link names become block names | yes, one file | **yes, named blocks via Assembly** | `vtkHDFWriter` (VTK 9.4+/ParaView 5.13+, temporal composite since VTK 9.5) |
| ExodusII | native (Exodus reader, extensions `.e`, `.exo`, `.ex2`, `.g`, ...) | yes | **yes**: named element blocks, node sets, side sets (`ex_put_name(s)` for `EX_NODE_SET`, `EX_SIDE_SET`) | SEACAS libexodus, Modified BSD; also VTK's Exodus writer |
| MED | plugin `MEDReader`, said to ship in binaries since 5.12 (forum post, not release notes) | yes | yes (groups/families) | med-fichier, LGPL-3-or-later per 4.2 headers; v5 licence not checked |

Sources: https://docs.vtk.org/en/latest/vtk_file_formats/vtkhdf_file_format/index.html ,
https://docs.vtk.org/en/latest/vtk_file_formats/vtkhdf_file_format/vtkhdf_specifications.html ,
https://www.kitware.com/vtkhdf-file-format-2025-status-update/ ,
https://docs.paraview.org/en/latest/UsersGuide/dataIngestion.html ,
https://github.com/sandialabs/seacas (exodusII.h, LICENSE),
https://discourse.paraview.org/t/med-format-native-support-in-paraview/7028 ,
https://docs.salome-platform.org/med-4.2.0-documentation/ .
Forum report of trouble with VTKHDF files with many time steps:
https://discourse.paraview.org/t/reading-error-with-vtkhdf-file-with-large-number-of-timesteps/16776 (not read in full).

---

## 6. Materials in

- FreeCAD 1.x material cards are YAML `.FCMat` with `General`, `Inherits`,
  `Models` (each model referenced by UUID). Example `CalculiX-Steel.FCMat`:
  `IsotropicLinearElastic` (YoungsModulus "210000 MPa", PoissonRatio),
  `Density`, `Thermal`. Values are strings with units.
  https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Material/Resources/Materials/Standard/Metal/Steel/CalculiX-Steel.FCMat
- Mechanical models shipped: Density, Hardness, IsotropicLinearElastic(+Arrays),
  LinearElastic(+Arrays), OrthotropicLinearElastic, hyperelastic ones
  (ArrudaBoyce, MooneyRivlin, NeoHooke, Ogden N1-N3, OgdenYld2004p18, Polynomial
  N1-N3, ReducedPolynomial N1-N3, Yeoh), Toughness(+Arrays). **No plasticity /
  hardening-curve model.**
  https://github.com/FreeCAD/FreeCAD/tree/main/src/Mod/Material/Resources/Models/Mechanical
  (models are LGPL-2.1-or-later YAML).
- FreeCAD FEM writes `*PLASTIC` (or `*PLASTIC, HARDENING=KINEMATIC`) only from
  a separate FEM object, `MaterialMechanicalNonlinear`, whose `YieldPoints` is a
  list of "stress, plastic strain" strings, and only when the solver's
  `MaterialNonlinearity` is on. Not from the `.FCMat` card.
  https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Fem/femsolver/calculix/write_femelement_material.py ,
  https://github.com/FreeCAD/FreeCAD/blob/main/src/Mod/Fem/femobjects/material_mechanicalnonlinear.py
- So TenForge's two outputs are complementary: `.FCMat` for elastic and
  density, a ccx `*PLASTIC` block for hardening. Whether FreeCAD accepts a
  user-defined model YAML (own UUID) carrying a hardening table: not checked.

---

## Recommendation per edge

| Edge | Recommended file | Why | Risks |
|---|---|---|---|
| FreeCAD to us (geometry) | **`.brep` + `<part>.groups.json` sidecar** written by a small FreeCAD macro we ship; also accept **STEP AP214/AP242** (names of solids, face colours) and let the user pick faces in-app | BREP is OCCT's native, loss-free form; FreeCAD `FaceN` = MapShapes index = Gmsh surface tag, the contract FreeCAD FEM already uses; FreeCAD STEP export drops face names (source-checked) | Indices shift after any model edit: store a per-face fingerprint and warn. BREP has no units (OCCT/FreeCAD work in mm). |
| Us to mesher | Same `.brep` + a generated `.geo`/Python script; Gmsh as a **process**; Netgen linkable (LGPL) as a second engine | Gmsh is GPL; process boundary keeps us LGPL. Netgen nglib reads BREP/STEP directly | Gmsh option behaviour drifts between releases; pin and test. |
| Mesher to us | **`.msh` 4.1** (binary for size) with physical groups named from the sidecar | Physical groups carry the names; MSH 4.1 documented and widely read (meshio, Dune) | Only grouped elements are saved unless `Mesh.SaveAll`; handle surface elements vs `*SURFACE` faces ourselves. |
| Us to CalculiX | **`.inp`** written by us: `*NODE`, `*ELEMENT`, `*NSET`/`*ELSET`, `*SURFACE` (element faces), `*MATERIAL` (`*ELASTIC`, `*PLASTIC` from TenForge), `*SOLID SECTION`, `*STEP`/`*STATIC`, `*BOUNDARY`, `*CLOAD`/`*DLOAD`, later `*CONTACT PAIR`; `*INCLUDE` for mesh and material | Keyword manual is thorough; `*INCLUDE` lets TenForge's `*PLASTIC` drop in unchanged | ccx is GPL: run as a process only. |
| CalculiX to us | **`.frd`**, read by our own C++ reader (ASCII and binary); request `*NODE FILE`/`*EL FILE` | Spec in cgx manual ch. 11; FreeCAD's LGPL reader is a legal porting reference | No formal versioning of the format; integration-point accuracy only in `.dat`. |
| Us to OpenFOAM | Case directory written by us: `constant/polyMesh` (or MSH 2.2 + `gmshToFoam`), `0/`, `system/`, with two back-ends (Foundation v13/v14, ESI v25xx/v26xx) | Patch names flow from the same groups | File names and solver selection differ between the forks (table above); more drift likely. |
| OpenFOAM to ParaView | **Nothing to convert**: empty `case.foam` marker, ParaView's built-in reader | Native, no extra copies | Built-in reader vs Foundation's own reader module differ in features (not compared in detail). |
| CalculiX results to ParaView | **VTKHDF** (`PartitionedDataSetCollection` + `Assembly` for named sets, temporal `Steps`), with **`.vtu` + `.pvd`** as the safe fallback option | One file, time series, named blocks, static mesh stored once; written with VTK (BSD) which we link anyway | VTKHDF is young (spec 2.8, writer features land per VTK release): needs ParaView 6.x on the reader side. Exodus is the mature alternative with named sets if VTKHDF disappoints. |
| TenForge to us (materials) | **`.FCMat`** for elastic/density/thermal plus TenForge's **ccx `*PLASTIC` include** | FreeCAD cards have no plasticity model; FreeCAD FEM itself keeps plasticity outside the card | Two files per material; keep them paired by name/UUID. |

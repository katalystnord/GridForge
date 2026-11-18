# Named groups from FreeCAD, version 1

**Status: approved by David, 2026-11-18; built the same day.**

How a part leaves FreeCAD with its faces already named, so loads and
supports can be set up on them in GridForge without picking them again.
The survey chose the carrier (`docs/survey/formats.md`, section 1): a
`.brep` of the part and a `<part>.groups.json` beside it, both written
by a FreeCAD macro GridForge ships. This is that file.

## 1. What the user does in FreeCAD

1. Select faces (or solids) in the 3D view, as for any FreeCAD tool.
2. Run the macro's "Add group", give the group a name ("Fixed grip").
   Repeat for each group.
3. Run "Export for GridForge": the macro writes `<part>.brep` and
   `<part>.groups.json` into a folder the user chooses.

GridForge then opens the `.brep` as a study's geometry (a STEP still
works as today) and offers the groups from the file beside it.

## 2. The file

```json
{
  "format": "gridforge-groups",
  "version": "1.0",
  "geometry": "bracket.brep",
  "units": "mm",
  "written by": "GridForge's FreeCAD macro 0.1, in FreeCAD 1.1.4",
  "groups": {
    "Fixed grip": {
      "faces": [
        {"freecad": "Face12", "kind": "plane", "area": 80,
         "centre": [0, 0, 2]}
      ]
    },
    "Body": {
      "solids": [
        {"freecad": "Solid1", "volume": 8699.69, "centre": [75, 0, 2]}
      ]
    }
  }
}
```

- `geometry` is the `.brep` written with it, by its file name. A group
  file is read only beside its own geometry.
- `units`: a `.brep` carries none; FreeCAD works in millimetres, and the
  macro says so. It must match the study's lengths.
- Each face is given by FreeCAD's name for it (`Face12`, for the user to
  recognise) and by its **signature**, as in study spec 4.2: its kind,
  area and centre. Each solid by its name, volume and centre.
- `kind` is GridForge's word, not FreeCAD's class name. The macro maps
  them:

  | FreeCAD (`type(face.Surface).__name__`) | `kind` |
  |---|---|
  | `Plane` | `plane` |
  | `Cylinder` | `cylinder` |
  | `Cone` | `cone` |
  | `Sphere` | `sphere` |
  | `Toroid` | `torus` |
  | `BezierSurface` | `Bezier surface` |
  | `BSplineSurface` | `B-spline surface` |
  | `SurfaceOfRevolution` | `surface of revolution` |
  | `SurfaceOfExtrusion` | `surface of extrusion` |
  | `OffsetSurface` | `offset surface` |
  | anything else | `other` |

  `Plane` and `Cylinder` were checked in FreeCAD 1.1.4 (2026-11-18); the
  other class names are from FreeCAD's Part module and not each checked.

## 3. How GridForge reads it

GridForge finds each face **by its signature, not by its number**. FreeCAD's
`FaceN` is OpenCASCADE's face order, and on a single solid it is
GridForge's own (checked on the box, 2026-11-18). But GridForge joins
solids that touch, and joining renumbers faces; a signature survives that,
a number does not.

- A face of the group file matches the one face of GridForge's mesh with
  the same signature (study spec 4.2's rule: the same kind, the area within
  a millionth, the centre within a millionth of the face's size).
- No face matching, or more than one (two faces of one kind, area and
  centre: rare, but not impossible): **said, never guessed**, naming the
  group and FreeCAD's face. The group is not made; the user
  picks those faces in GridForge.
- A solid matches by its volume (within a millionth) and centre.
- Each group found becomes a group in study.json with GridForge's face
  numbers and the signatures, exactly as a group made in the window (study
  spec 4.2), so it is checked the same way when the geometry changes.
- A name that is already a group in the study: said, and that group left
  as it is.

## 4. The macro

- One Python file, `tools/freecad/GridForge.FCMacro`, LGPL-2.1-or-later
  like GridForge, using FreeCAD's own API only (`Shape.exportBrep`,
  `Face.Area`, `Face.CenterOfMass`, `Solid.Volume`; each checked in
  FreeCAD 1.1.4).
- It is run inside FreeCAD by the user; GridForge never runs FreeCAD.
  The two meet only at these two files (tenet 6).
- Its tests run it in FreeCADCmd where FreeCAD is installed, on the box
  fixture, and compare its file with one written by hand from the box's
  known faces; GridForge then reads the macro's own files. FreeCADCmd keeps
  neither a script's prints nor its exit code (FreeCAD 1.1.4), so the
  test's verdict is a file (`tests/freecad/`).
- In GridForge: File, New Study from Geometry takes the `.brep` (or a
  STEP) and copies the group file beside it into the study's `inputs/`;
  once meshed, Study, Add Groups From FreeCAD adds the groups found.

## 5. Not in version 1

- Edges and vertices as groups (a load along an edge is picked in
  GridForge).
- Several parts in one file: one `.brep`, one group file.
- Reading names from a STEP (FreeCAD's STEP export writes no face names;
  survey, section 1).

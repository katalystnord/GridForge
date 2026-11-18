# GridForge's FreeCAD macro

`GridForge.FCMacro` names groups of faces in FreeCAD and writes the part
for GridForge: `<part>.brep` and `<part>.groups.json` side by side
(`docs/specs/groups-v1.md`). LGPL-2.1-or-later, as GridForge.

**Install:** copy the file into FreeCAD's macro folder. FreeCAD's Macro
menu, Macros..., shows the folder (for FreeCAD 1.1's Flatpak:
`~/.var/app/org.freecad.FreeCAD/data/FreeCAD/v1-1/Macro/`). Nothing else is
needed: it uses FreeCAD's own Part module and the Qt bindings FreeCAD ships.

**Use:**

1. Run the macro (Macro menu). A small panel opens.
2. Select faces of one part in the 3D view (or the part in the tree, for
   its whole solid), type a name, Add group. Repeat.
3. Export for GridForge, and choose a folder.
4. In GridForge: File, New Study from Geometry, the `.brep`; once it is
   meshed, Study, Add Groups From FreeCAD.

The groups are kept on the part (its GridForgeGroups property), so they
are saved with the FreeCAD document.

Checked with FreeCAD 1.1.4. Its test: `tests/freecad/`.

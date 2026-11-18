# FreeCAD fixtures

What GridForge's FreeCAD macro hands over (docs/specs/groups-v1.md).

| file | what | known answer |
|---|---|---|
| `box.brep` | the 10 x 20 x 30 mm box at the origin, written by FreeCAD 1.1.4's `exportBrep` (`make.py`) | as `../step/box.step`: faces 1 to 6 are x = 0, x = 10, y = 0, y = 20, z = 0, z = 30 (600, 600, 300, 300, 200, 200 mm2); one solid of 6000 mm3 centred at (5, 10, 15) |
| `box.groups.json` | three groups on it, written by hand from those faces | Back: face 1; Ends: faces 5 and 6; Steel part: solid 1 |

FreeCAD's `Face1` to `Face6` on this box are those faces in that order
(checked in FreeCAD 1.1.4, 2026-11-18).

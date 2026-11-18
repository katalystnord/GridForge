# Writes box.brep with FreeCAD's own exportBrep: the 10 x 20 x 30 mm box at
# the origin, as tests/fixtures/step/box.step. Run once, in FreeCAD's
# command line program; the file is committed.
#
#   OUT=$PWD flatpak run --filesystem=$PWD --env=OUT=$PWD \
#       --command=FreeCADCmd org.freecad.FreeCAD make.py
import os
import Part

Part.makeBox(10, 20, 30).exportBrep(os.path.join(os.environ["OUT"], "box.brep"))

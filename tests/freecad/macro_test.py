# GridForge's FreeCAD macro (tools/freecad/GridForge.FCMacro), run in
# FreeCAD's command line program: the box exported with three groups, its
# group file compared with the one written by hand from the box's known
# faces (tests/fixtures/freecad/box.groups.json). GridForge then reads the
# macro's own files (test_groups_file, GF_FREECAD_OUT).
#
#   MACRO=... FIXTURES=... OUT=... FreeCADCmd macro_test.py
import json
import os
import runpy

import Part

macro = runpy.run_path(os.environ["MACRO"])
out = os.environ["OUT"]
os.makedirs(out, exist_ok=True)
groups = {"Back": ["Face1"], "Ends": ["Face5", "Face6"], "Steel part": ["Solid1"]}
brep, path = macro["export"](Part.makeBox(10, 20, 30), groups, out, "box")

with open(path) as f:
    written = json.load(f)
with open(os.path.join(os.environ["FIXTURES"], "freecad", "box.groups.json")) as f:
    kept = json.load(f)

failures = []


def same(a, b, where):
    if isinstance(a, dict) and isinstance(b, dict):
        if set(a) != set(b):
            failures.append("%s: keys %s, not %s" % (where, sorted(a), sorted(b)))
        for k in set(a) & set(b):
            same(a[k], b[k], where + "." + k)
    elif isinstance(a, list) and isinstance(b, list):
        if len(a) != len(b):
            failures.append("%s: %d entries, not %d" % (where, len(a), len(b)))
        for i, (x, y) in enumerate(zip(a, b)):
            same(x, y, "%s[%d]" % (where, i))
    elif isinstance(a, (int, float)) and isinstance(b, (int, float)):
        if abs(a - b) > 1e-9 * max(1, abs(b)):
            failures.append("%s: %r, not %r" % (where, a, b))
    elif a != b:
        failures.append("%s: %r, not %r" % (where, a, b))


for key in ("format", "version", "geometry", "units", "groups"):
    same(written.get(key), kept.get(key), key)
if not written.get("written by", "").startswith("GridForge's FreeCAD macro 0.1, in FreeCAD "):
    failures.append("written by: %r" % written.get("written by"))
if not os.path.getsize(brep):
    failures.append("no .brep written")
# FreeCADCmd keeps neither a script's prints nor its exit code (seen in
# FreeCAD 1.1.4, 2026-11-18): the verdict goes to a file, which the test
# reads.
with open(os.path.join(out, "macro-test.txt"), "w") as f:
    for line in failures:
        f.write("  FAIL " + line + "\n")
    f.write("%d failure(s)\n" % len(failures))

# box-setup: a CalculiX job written by GridForge from a set-up

The study spec's own example set-up (`docs/specs/study-v1.md`, section
4, with the material's values given in the study as section 4.8 says),
in `tension.json`. GridForge wrote the whole job:

    gf-study deck tests/fixtures/calculix/box-setup/tension.json \
        tests/fixtures/step/box.step --size 5 --order 2

gives `mesh.inp` (the box's 229 C3D10 and 508 nodes, with the set-up's
groups as sets: Back, Side, Bottom, Pulled, Steel_part) and `tension.inp`
(material, section, symmetry supports, the step pulling Pulled 0.01 mm).
ccx 2.23 (FreeCAD's Flatpak, `make.sh`) solved it on 2026-11-17 with no
warning.

The hand answer is box-gfmesh's (the same box, the same tension, there
in a deck written by hand): every node, the 210 MPa stress (209.998 at
the nodes, extrapolated) and the 126000 N reaction. The test
`test_mesh_to_ccx` holds both fixtures to it, and holds GridForge to
both files here line for line, all but the first (the build that wrote
them).

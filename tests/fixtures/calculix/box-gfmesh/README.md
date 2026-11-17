# box-gfmesh: GridForge's own mesh, solved by ccx

The 10 x 20 x 30 mm box (`tests/fixtures/step/box.step`) meshed by
GridForge's mesher and written by its deck writer:

    gf-study mesh tests/fixtures/step/box.step --size 5 --order 2 --inp

gives `mesh.inp` (229 C3D10, 508 nodes, the sets Solid1 and Face1 to
Face6). `tension.inp`, written by hand, includes it, and ccx 2.23 (from
FreeCAD's Flatpak, `make.sh`) solved it on 2026-11-17.

## The hand answer

Symmetry planes hold Face1 (x = 0) in x, Face3 (y = 0) in y and Face5
(z = 0) in z; Face2 (x = 10) is pulled 0.01 mm in x. Steel, E = 210000
MPa, nu = 0.3. Uniaxial tension, so at every node:

| quantity | by hand |
|---|---|
| ux, uy, uz | 0.001 x, -0.0003 y, -0.0003 z |
| stress | sxx = E * 0.001 = 210 MPa; every other component 0 |
| reaction on Face2 | 210 MPa * 20 mm * 30 mm = 126000 N in x |

## What the files hold

- Displacements as the hand answer to every digit written (5e-8 at
  worst, the sixth digit of 0.01).
- The reaction (`tension.dat`, `*NODE PRINT, TOTALS=ONLY`): 1.260000E+05.
- sxx = 209.998 at every node, the other components below 1.4e-12:
  extrapolated to the nodes, as in bar-tension.
- `ccx.log` shows no warning.

`mesh.inp` is GridForge's output, kept because ccx solved it; the test
`the_committed_mesh_is_what_gridforge_writes_today` holds GridForge to it
line for line, all but the first (the build that wrote it). `make.sh`
runs ccx on `tension.inp` again but does not write `mesh.inp`; that is
the command above.

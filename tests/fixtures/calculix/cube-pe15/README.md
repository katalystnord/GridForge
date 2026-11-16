# Cube in tension, C3D15

`cube.inp`: a 10 mm steel cube, 16 C3D15 elements (two per cell, the square cut on its diagonal), written by
`../cubes.py`. Units mm, N, MPa; E = 210000 MPa, nu = 0.3. z = 0 held in
z, x = 0 in x, y = 0 in y; z = 10 moved 0.01 mm in z.

Written by CalculiX ccx 2.23 (FreeCAD 1.1.4's Flatpak), 2026-11-16, with
the command in `../make.sh`. `ccx.log`, `cube.sta` and `cube.cvg` are kept with it.

## The known answer (by hand)

A uniform strain, which this element reproduces exactly (the patch test),
so the hand answer holds at every node:

| | Value |
|---|---|
| strain ezz | 0.01 / 10 = 1.0e-3 |
| stress szz | E * ezz = 210 MPa; the others 0 |
| displacement uz, ux, uy | 1.0e-3 z, -3.0e-4 x, -3.0e-4 y |

## What the file holds

Displacements as the hand answer to every digit written; szz = 209.998 at
the nodes (extrapolated from the integration points, as in bar-tension).

As with C3D20, ccx writes the vertical edges' middle nodes (deck 13 to 15)
before the top edges' (deck 10 to 12); the converter maps them.

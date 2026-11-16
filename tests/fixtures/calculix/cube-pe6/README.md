# Cube in tension, C3D6

`cube.inp`: a 10 mm steel cube, 16 C3D6 elements (two per cell, the square cut on its diagonal), written by
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

Displacements as the hand answer to every digit written; szz = 210.000 at
the nodes (extrapolated from the integration points, as in bar-tension).

ccx writes the deck's order. VTK's linear wedge is the mirror of it (its
face 0, 1, 2 must face out of the cell), so the converter writes the
nodes as 1, 3, 2, 4, 6, 5. VTK's quadratic wedge is not mirrored.

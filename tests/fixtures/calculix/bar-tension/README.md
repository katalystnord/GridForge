# Bar in tension

`bar.inp`: a steel bar, 10 x 10 x 100 mm, four C3D8 elements along z.
Units mm, N, MPa; E = 210000 MPa, nu = 0.3. The end z = 0 is held in z,
the end z = 100 is moved 0.1 mm in z, and the bar is free to contract
(x held on x = 0, y held on y = 0).

Written by CalculiX ccx 2.23 (FreeCAD 1.1.4's Flatpak) with `../make.sh`,
2026-11-16. `ccx.log`, `bar.sta` and `bar.cvg` are kept with it.

## The known answer (by hand)

| | Value |
|---|---|
| strain ezz | 0.1 / 100 = 1.0e-3 |
| strain exx, eyy | -nu * ezz = -3.0e-4 |
| stress szz | E * ezz = 210 MPa; the others 0 |
| displacement uz at z | 1.0e-3 * z (0.1 at z = 100) |
| displacement ux at x = 10, uy at y = 10 | -3.0e-4 * 10 = -0.003 |
| reaction on the moved end | 210 MPa * 100 mm2 = 21000 N in z |

## What the file holds

- Displacements and the reaction (`FORC`, 5250 N on each of nodes 17 to
  20) match the hand answer to every digit written.
- Stress and strain are uniform but 0.004 % low at the nodes: szz =
  209.992, ezz = 0.99996e-3. ccx computes them exactly at the integration
  points (210.000 and 1.000e-3, checked with `*EL PRINT` into `.dat`) and
  extrapolates them to the nodes for the `.frd`; the shortfall is in that
  step. A reader test reads the file's own numbers exactly, and checks
  them against the hand answer to 1e-4 relative.
- The format is fixed-column: a negative number follows the one before
  it with no space (`0.00000E+00-3.00000E-03`), so the file must be read
  by column, not split on spaces.

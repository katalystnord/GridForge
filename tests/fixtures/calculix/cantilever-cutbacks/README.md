# Cantilever bent plastic: cutbacks

`cantilever.inp`: the mesh of `../cantilever-te10` (480 C3D10), steel
yielding at 250 MPa (300 MPa at 5 % plastic strain), NLGEOM, the tip moved
20 mm in y in one step that asks for one increment (`*STATIC` 1.0, 1.0).
ccx cannot take it in one: it cuts back, then grows the increment again.
Results are written at the step's end only (`FREQUENCY=1000`), as the
subject here is the progress files, `cantilever.sta` and `cantilever.cvg`.

Written by CalculiX ccx 2.23 (FreeCAD 1.1.4's Flatpak), 2026-11-16, with
the command in `../make.sh` (two runs gave the same `.sta`, byte for byte).

## What the files hold (read by hand)

- `.sta`: 17 rows. Four are attempts that did not converge, marked by a
  `U` after the attempt number: increment 1 attempts 1 and 2 (increment
  time 1.0, then 0.25), increment 5 attempt 1, increment 9 attempt 1. The
  13 converged increments' times add up to 1.0, the step's length; the
  last row's step time is 1.0.
- `.cvg`: 64 iterations, one row each. Per attempt they number exactly as
  the `.sta`'s ITRS column says (4 for each failed attempt; 2 to 5 for
  the others); the ITRS column adds up to 64.
- ccx spells the `.cvg` header "SUMMARY OF C0NVERGENCE INFORMATION", with
  a zero.

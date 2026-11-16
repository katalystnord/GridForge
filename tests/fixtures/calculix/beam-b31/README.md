# Strip in tension, B31 (beam)

`strip.inp`, written by `../strips.py`: a 100 x 10 mm strip, 1 mm thick (a 10 x 1 mm rectangular beam section), B31, with `*NODE FILE, OUTPUT=2D` and `*EL FILE, OUTPUT=2D`.
Units mm, N, MPa; E = 210000 MPa, nu = 0.3. x = 0 held in x, x = 100 moved
0.1 mm in x.

Written by CalculiX ccx 2.23 (FreeCAD 1.1.4's Flatpak), 2026-11-16, with
the command in `../make.sh`; `ccx.log`, `strip.sta` and `strip.cvg` kept.
`cgx-reference.txt` holds what cgx 2.21 shows for it (`../cgx-reference.py`).

## What the file holds

be2 (type 11), 4 elements on the deck's 5 nodes.

## The known answer (by hand)

A uniform strain, which this element reproduces exactly: exx = 1.0e-3,
sxx = 210 MPa, ux = 1.0e-3 x. The file: ux as the hand answer at every
node; sxx = 209.992 (extrapolated to the nodes, as in bar-tension).

# Strip in tension, B32R (beam)

`strip.inp`, written by `../strips.py`: a 100 x 10 mm strip, 1 mm thick (a 10 x 1 mm rectangular beam section), B32R, with `*NODE FILE, OUTPUT=2D` and `*EL FILE, OUTPUT=2D`.
Units mm, N, MPa; E = 210000 MPa, nu = 0.3. x = 0 held in x, x = 100 moved
0.1 mm in x.

Written by CalculiX ccx 2.23 (FreeCAD 1.1.4's Flatpak), 2026-11-16, with
the command in `../make.sh`; `ccx.log`, `strip.sta` and `strip.cvg` kept.
`cgx-reference.txt` holds what cgx 2.21 shows for it (`../cgx-reference.py`).

## What the file holds

be3 (type 12), 4 elements on the deck's 9 nodes, written end, end, middle (the deck has end, middle, end).

## The known answer, and where ccx departs from it

By hand, as the other strips: sxx = 210 MPa, ux = 1.0e-3 x. ccx's B32R
does not give it: sxx is 202.279 at every node (3.7 % low) and ux is not
linear (0.0120 at x = 12.5 where the hand answer is 0.0125; 0.0034 off
at worst). Supporting the beam only against rigid-body motion (1, 1, 4
at node 1 and 2, 3 at the far end) changes nothing; the cause was not
investigated further. B31 gives the hand answer.

So this fixture proves the node order by geometry (each middle node
halfway between its ends, as VTK's quadratic edge wants), and the test
checks that the converter keeps ccx's 202.279, not that ccx is right.

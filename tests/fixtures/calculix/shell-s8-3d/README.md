# Strip in tension, S8 (shell)

`strip.inp`, written by `../strips.py`: a 100 x 10 mm strip, 1 mm thick, S8, with ccx's default output.
Units mm, N, MPa; E = 210000 MPa, nu = 0.3. x = 0 held in x, x = 100 moved
0.1 mm in x.

Written by CalculiX ccx 2.23 (FreeCAD 1.1.4's Flatpak), 2026-11-16, with
the command in `../make.sh`; `ccx.log`, `strip.sta` and `strip.cvg` kept.
`cgx-reference.txt` holds what cgx 2.21 shows for it (`../cgx-reference.py`).

## What the file holds

he20 (type 4), 4 elements on 56 nodes ccx made (ids 28 and up): the deck's nodes 1 to 27 are not in the file, the element ids are.

## The known answer (by hand)

A uniform strain, which this element reproduces exactly: exx = 1.0e-3,
sxx = 210 MPa, ux = 1.0e-3 x. The file: ux as the hand answer at every
node; sxx = 209.992 to 210.010 (extrapolated to the nodes, as in bar-tension).

Without OUTPUT=2D the deck's node sets name nodes the results do not
hold: the converter keeps the element sets, leaves the node sets empty
and says so in the provenance. Use OUTPUT=2D for shells.

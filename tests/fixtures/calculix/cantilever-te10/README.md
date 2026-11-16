# Cantilever in quadratic tetrahedra

`cantilever.inp`, written by `deck.py`: a steel cantilever, 10 x 10 x 100
mm, in 480 C3D10 elements (2 x 2 x 20 cubes of 5 mm, six tetrahedra each)
and 1025 nodes. Units mm, N, MPa; E = 210000 MPa, nu = 0.3. The root
z = 0 is held in x, y and z. The tip z = 100 carries 100 N in y, spread
as a uniform shear traction (on each 6-node face triangle a third of its
share goes to each midside node, none to the corners).

Written by CalculiX ccx 2.23 (FreeCAD 1.1.4's Flatpak), 2026-11-16, by
running ccx on the deck as `../make.sh` does. `ccx.log`, `.sta` and
`.cvg` are kept with it.

What it proves for the reader: ccx writes C3D10 as `.frd` element type 6
(te10), with its ten nodes in the deck's order.

## The known answer (by hand)

I = b h^3 / 12 = 833.33 mm4, G = E / 2.6 = 80769 MPa, A = 100 mm2.

| | Value |
|---|---|
| tip deflection, bending (P L^3 / 3 E I) | 0.19048 mm |
| plus shear (P L / (5/6 G A)) | 0.00149 mm |
| tip deflection, Timoshenko | 0.19196 mm |
| reaction at the root | 100 N in -y |
| szz at z = 50, x = 5, y = 0 and y = 10 (M c / I, M = 5000 N mm) | +30 and -30 MPa |

## What the file holds

- Tip deflection, the mean of uy over the 25 tip nodes (9 corners, 16 midsides): 0.18983 mm,
  1.1 % under Timoshenko. A root held in all three directions also
  stops the Poisson contraction there, which stiffens it a little.
- Root reaction in y: -100.00014 N.
- szz at mid-span: 29.75 and -30.25 MPa, within 1 % of beam theory.

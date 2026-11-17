# box-gravity: the box hanging under its own weight

Held at its top (Top, z = 30, symmetry: no movement in z; Back and Side
as the other fixtures), density 7.9e-9 t/mm^3, gravity 9810 mm/s^2 in -z
(`*DLOAD, EALL, GRAV`). Written whole by GridForge, solved by ccx 2.23 on
2026-11-17 with no warning.

Hand answer: the reaction at the top is the weight, rho g V = 7.9e-9 *
9810 * 6000 = 0.464994 N.

What the file holds: ccx's FORC sums to zero over the model, so at a
held node it is the reaction less the load gravity puts on that node.
Summed over Top it is 0.4521409 N; the top nodes' own consistent gravity
load (a 10-node tetrahedron's: -W/20 at each corner, W/5 at each middle
node, W its weight) is -0.0128530 N; the difference is 0.4649939 N, the
weight to 2e-7. The test adds that load back, computed from the mesh,
not taken from ccx. The stress is not checked: holding the whole top
face in z is not the free hanging bar's exact solution near the top.

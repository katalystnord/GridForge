# A plastic dogbone, from a study file to ParaView

The benchmark's dogbone (150 x 20 x 4 mm, narrow part 60 x 10 mm, R30
transitions) in steel measured by TenSense: its card (`tensense-ro.FCMat`,
E = 199.9 GPa) and its `*PLASTIC` block (Rp0.2 = 301 MPa, hardening to
667 MPa), both as TenSense 0.3.0 wrote them. Poisson's ratio, which a
tensile test does not measure, is given in the study (0.3). One grip
(face 12, x = 0) is clamped; the other (face 6, x = 150) is pulled 1.5 mm.

    GF_CCX=../../tools/ccx-flatpak \
        ../../build-ninja/src/gf-study solve pull.json dogbone.step --size 3 --order 2 -o job

(`GF_CCX` only where ccx is not on PATH; `tools/ccx-flatpak` uses FreeCAD's.)
No file is edited by hand on the way: the mesh (gf_mesh, Netgen), the
job folder, the deck (large displacements turned on by the plastic
material, and said in the deck), ccx, and the results in
`job/results/results.vtkhdf` for ParaView.

## What it gave (2026-11-17, ccx 2.23, 1624 C3D10, 3575 nodes)

- 29 increments, every one converged, in about a minute on this machine.
- The grip force grows from 6469 N (the first increment, elastic) to
  14990 N: 375 MPa on the narrow section's 40 mm2, where TenSense's
  curve stands at that plastic strain.
- Equivalent plastic strain (PE) about 1.9 % along the narrow part,
  falling to none through the transitions; none in the grips. Von Mises
  stress at most 383 MPa.

- PE (equivalent plastic strain) reads as low as -0.00044 at some nodes,
  which cannot be. Chased down 2026-11-17: asked for at its integration
  points (`*EL PRINT ... PEEQ`), ccx gives none below 0 (6496 points, 0
  to 0.018777); the .frd's nodal values are ccx's extrapolation from
  them, and overshoot at the yield fronts in the R30 transitions (65
  nodes below 0 at the end, x = 32 to 118). GridForge leaves the values
  as ccx wrote them and says so: results.json's "notes", the results'
  provenance, and a warning while it runs.

Not checked against a hand answer: a dogbone in plasticity has none.
The box fixtures in tests/fixtures/calculix hold the deck to hand
answers; this example shows the whole chain running on a real case.

Known: the results file is 199 MB against ccx's 34 MB .frd. Every one of
the 22 sets carries every field at each of the 29 time points. To be made
smaller (the roadmap says how).
